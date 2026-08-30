/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/MSWindowsFileDropExtractor.h"

#include <ShlObj_core.h>
#include <Windows.h>
#include <objidl.h>
#include <shellapi.h>

#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using deskflow::filetransfer::extractWindowsFileDrop;
using deskflow::filetransfer::extractWindowsFileDropDataObject;
using deskflow::filetransfer::WindowsFileDropError;
using deskflow::filetransfer::WindowsFileDropLimits;

class ReleaseTracker final : public IUnknown
{
public:
  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interfaceId, void **object) override
  {
    if (object == nullptr) {
      return E_POINTER;
    }
    if (IsEqualIID(interfaceId, IID_IUnknown)) {
      *object = static_cast<IUnknown *>(this);
      AddRef();
      return S_OK;
    }
    *object = nullptr;
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override
  {
    ++m_addRefCalls;
    return ++m_refCount;
  }

  ULONG STDMETHODCALLTYPE Release() override
  {
    ++m_releaseCalls;
    return --m_refCount;
  }

  [[nodiscard]] ULONG addRefCalls() const noexcept
  {
    return m_addRefCalls;
  }

  [[nodiscard]] ULONG releaseCalls() const noexcept
  {
    return m_releaseCalls;
  }

  [[nodiscard]] ULONG refCount() const noexcept
  {
    return m_refCount;
  }

private:
  ULONG m_refCount = 1;
  ULONG m_addRefCalls = 0;
  ULONG m_releaseCalls = 0;
};

class FakeDataObject final : public IDataObject
{
public:
  explicit FakeDataObject(HRESULT queryResult = S_OK, HRESULT getResult = S_OK)
      : m_queryResult(queryResult),
        m_getResult(getResult)
  {
  }

  void setHGlobal(HGLOBAL handle, IUnknown *releaser)
  {
    m_medium = {};
    m_medium.tymed = TYMED_HGLOBAL;
    m_medium.hGlobal = handle;
    m_medium.pUnkForRelease = releaser;
  }

  void setInvalidMedium(IUnknown *releaser)
  {
    m_medium = {};
    m_medium.tymed = TYMED_NULL;
    m_medium.pUnkForRelease = releaser;
  }

  [[nodiscard]] ULONG queryCalls() const noexcept
  {
    return m_queryCalls;
  }

  [[nodiscard]] ULONG getCalls() const noexcept
  {
    return m_getCalls;
  }

  [[nodiscard]] const FORMATETC &queryFormat() const noexcept
  {
    return m_queryFormat;
  }

  HRESULT STDMETHODCALLTYPE QueryInterface(REFIID interfaceId, void **object) override
  {
    if (object == nullptr) {
      return E_POINTER;
    }
    if (IsEqualIID(interfaceId, IID_IUnknown) || IsEqualIID(interfaceId, IID_IDataObject)) {
      *object = static_cast<IDataObject *>(this);
      AddRef();
      return S_OK;
    }
    *object = nullptr;
    return E_NOINTERFACE;
  }

  ULONG STDMETHODCALLTYPE AddRef() override
  {
    return ++m_refCount;
  }

  ULONG STDMETHODCALLTYPE Release() override
  {
    return --m_refCount;
  }

  HRESULT STDMETHODCALLTYPE GetData(FORMATETC *format, STGMEDIUM *medium) override
  {
    ++m_getCalls;
    if (format == nullptr || medium == nullptr) {
      return E_POINTER;
    }
    if (m_getResult != S_OK) {
      return m_getResult;
    }

    *medium = m_medium;
    if (medium->pUnkForRelease != nullptr) {
      medium->pUnkForRelease->AddRef();
    }
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC *, STGMEDIUM *) override
  {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC *format) override
  {
    ++m_queryCalls;
    if (format == nullptr) {
      return E_POINTER;
    }
    m_queryFormat = *format;
    return m_queryResult;
  }

  HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC *, FORMATETC *) override
  {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE SetData(FORMATETC *, STGMEDIUM *, BOOL) override
  {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD, IEnumFORMATETC **) override
  {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC *, DWORD, IAdviseSink *, DWORD *) override
  {
    return OLE_E_ADVISENOTSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override
  {
    return OLE_E_ADVISENOTSUPPORTED;
  }

  HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA **) override
  {
    return OLE_E_ADVISENOTSUPPORTED;
  }

private:
  ULONG m_refCount = 1;
  ULONG m_queryCalls = 0;
  ULONG m_getCalls = 0;
  HRESULT m_queryResult;
  HRESULT m_getResult;
  FORMATETC m_queryFormat{};
  STGMEDIUM m_medium{};
};

class DropBlock
{
public:
  explicit DropBlock(const std::vector<std::wstring> &paths)
  {
    std::wstring payload;
    for (const auto &path : paths) {
      payload.append(path);
      payload.push_back(L'\0');
    }

    payload.push_back(L'\0');
    if (paths.empty()) {
      payload.push_back(L'\0');
    }

    const auto byteCount = sizeof(DROPFILES) + payload.size() * sizeof(wchar_t);
    m_handle = GlobalAlloc(GMEM_MOVEABLE | GMEM_ZEROINIT, byteCount);
    if (m_handle == nullptr) {
      throw std::runtime_error("GlobalAlloc failed");
    }

    auto *memory = GlobalLock(m_handle);
    if (memory == nullptr) {
      GlobalFree(m_handle);
      m_handle = nullptr;
      throw std::runtime_error("GlobalLock failed");
    }

    auto *header = static_cast<DROPFILES *>(memory);
    header->pFiles = static_cast<DWORD>(sizeof(DROPFILES));
    header->fWide = TRUE;

    auto *pathBytes = static_cast<unsigned char *>(memory) + sizeof(DROPFILES);
    std::memcpy(pathBytes, payload.data(), payload.size() * sizeof(wchar_t));
    GlobalUnlock(m_handle);
  }

  DropBlock(const DropBlock &) = delete;
  DropBlock &operator=(const DropBlock &) = delete;

  ~DropBlock()
  {
    if (m_handle != nullptr) {
      GlobalFree(m_handle);
    }
  }

  [[nodiscard]] HDROP get() const noexcept
  {
    return reinterpret_cast<HDROP>(m_handle);
  }

private:
  HGLOBAL m_handle = nullptr;
};

bool expect(bool condition, const std::string_view message)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
  }
  return condition;
}

bool rejectsNullAndEmptyDrops()
{
  const auto nullResult = extractWindowsFileDrop(nullptr);
  if (!expect(nullResult.error == WindowsFileDropError::NullHandle, "null handle error")) {
    return false;
  }
  if (!expect(nullResult.paths.empty(), "null handle result is atomic")) {
    return false;
  }

  const DropBlock empty({});
  const auto emptyResult = extractWindowsFileDrop(empty.get());
  return expect(emptyResult.error == WindowsFileDropError::EmptyDrop, "empty drop error") &&
         expect(emptyResult.paths.empty(), "empty drop has no paths");
}

bool extractsOneUnicodePath()
{
  const std::wstring path = L"C:\\전송\\보고서-😀.txt";
  const DropBlock drop({path});

  const auto result = extractWindowsFileDrop(drop.get());
  return expect(result.ok(), "Unicode drop succeeds") && expect(result.paths.size() == 1, "one path returned") &&
         expect(result.paths.front().native() == path, "Unicode path is preserved exactly");
}

bool preservesMultiplePathOrder()
{
  const std::vector<std::wstring> paths = {
      L"C:\\first.txt",
      L"D:\\폴더\\second.bin",
      L"E:\\third file.dat",
  };
  const DropBlock drop(paths);

  const auto result = extractWindowsFileDrop(drop.get());
  if (!expect(result.ok(), "multi-file drop succeeds") ||
      !expect(result.paths.size() == paths.size(), "all paths returned")) {
    return false;
  }

  for (std::size_t index = 0; index < paths.size(); ++index) {
    if (!expect(result.paths[index].native() == paths[index], "path order is stable")) {
      return false;
    }
  }
  return true;
}

bool supportsPathsBeyondLegacyMaxPath()
{
  const std::wstring path = L"C:\\" + std::wstring(300, L'a') + L".txt";
  const DropBlock drop({path});

  const auto result = extractWindowsFileDrop(drop.get());
  return expect(result.ok(), "long native path succeeds") &&
         expect(result.paths.front().native() == path, "long path is not truncated");
}

bool honorsItemLimitBoundary()
{
  const DropBlock drop({L"C:\\one.txt", L"C:\\two.txt"});

  const WindowsFileDropLimits exactLimit{.maxItems = 2};
  if (!expect(extractWindowsFileDrop(drop.get(), exactLimit).ok(), "exact item limit succeeds")) {
    return false;
  }

  const WindowsFileDropLimits lowLimit{.maxItems = 1};
  const auto rejected = extractWindowsFileDrop(drop.get(), lowLimit);
  return expect(rejected.error == WindowsFileDropError::TooManyItems, "item count over limit is rejected") &&
         expect(rejected.paths.empty(), "item-limit rejection is atomic");
}

bool honorsPathLimitBoundary()
{
  const std::wstring path = L"C:\\boundary.txt";
  const DropBlock drop({path});

  const WindowsFileDropLimits exactLimit{.maxPathCharacters = path.size()};
  if (!expect(extractWindowsFileDrop(drop.get(), exactLimit).ok(), "exact path limit succeeds")) {
    return false;
  }

  const WindowsFileDropLimits lowLimit{.maxPathCharacters = path.size() - 1};
  const auto rejected = extractWindowsFileDrop(drop.get(), lowLimit);
  return expect(rejected.error == WindowsFileDropError::PathTooLong, "path over limit is rejected") &&
         expect(rejected.itemPosition == 0, "rejected path position is reported") &&
         expect(rejected.paths.empty(), "path-limit rejection is atomic");
}

bool discardsEarlierPathsWhenALaterPathFails()
{
  const std::wstring shortPath = L"C:\\ok.txt";
  const std::wstring longPath = L"C:\\" + std::wstring(40, L'x') + L".txt";
  const DropBlock drop({shortPath, longPath});
  const WindowsFileDropLimits limits{.maxPathCharacters = shortPath.size()};

  const auto result = extractWindowsFileDrop(drop.get(), limits);
  return expect(result.error == WindowsFileDropError::PathTooLong, "later invalid path is rejected") &&
         expect(result.itemPosition == 1, "later invalid path position is reported") &&
         expect(result.paths.empty(), "partial extraction is discarded");
}

bool rejectsNullUnsupportedAndFailedDataObjects()
{
  const auto nullResult = extractWindowsFileDropDataObject(nullptr);
  if (!expect(nullResult.error == WindowsFileDropError::NullDataObject, "null data object error")) {
    return false;
  }

  FakeDataObject unsupported(DV_E_FORMATETC);
  const auto unsupportedResult = extractWindowsFileDropDataObject(&unsupported);
  if (!expect(unsupportedResult.error == WindowsFileDropError::UnsupportedFormat, "unsupported format error") ||
      !expect(unsupported.queryCalls() == 1, "unsupported format is queried once") ||
      !expect(unsupported.getCalls() == 0, "unsupported format is not acquired")) {
    return false;
  }

  FakeDataObject failed(S_OK, E_FAIL);
  const auto failedResult = extractWindowsFileDropDataObject(&failed);
  return expect(failedResult.error == WindowsFileDropError::DataRequestFailed, "failed GetData error") &&
         expect(failed.queryCalls() == 1, "failed object is queried once") &&
         expect(failed.getCalls() == 1, "failed object acquisition is attempted once") &&
         expect(failedResult.paths.empty(), "failed acquisition has no paths");
}

bool extractsAndReleasesDataObjectPayload()
{
  const std::vector<std::wstring> paths = {L"C:\\첫 번째.txt", L"D:\\second.bin"};
  const DropBlock drop(paths);
  ReleaseTracker releaser;
  FakeDataObject dataObject;
  dataObject.setHGlobal(reinterpret_cast<HGLOBAL>(drop.get()), &releaser);

  const auto result = extractWindowsFileDropDataObject(&dataObject);
  const auto &format = dataObject.queryFormat();
  return expect(result.ok(), "data object extraction succeeds") &&
         expect(result.paths.size() == paths.size(), "data object returns every path") &&
         expect(
             result.paths[0].native() == paths[0] && result.paths[1].native() == paths[1],
             "data object preserves path contents"
         ) &&
         expect(format.cfFormat == CF_HDROP, "CF_HDROP is requested") &&
         expect(format.ptd == nullptr, "no target device is requested") &&
         expect(format.dwAspect == DVASPECT_CONTENT, "content aspect is requested") &&
         expect(format.lindex == -1, "all content is requested") &&
         expect(format.tymed == TYMED_HGLOBAL, "HGLOBAL storage is requested") &&
         expect(releaser.addRefCalls() == 1, "returned medium takes one release reference") &&
         expect(releaser.releaseCalls() == 1, "successful medium is released once") &&
         expect(releaser.refCount() == 1, "release reference is balanced");
}

bool releasesInvalidAndRejectedDataObjectPayloads()
{
  ReleaseTracker invalidReleaser;
  FakeDataObject invalid;
  invalid.setInvalidMedium(&invalidReleaser);
  const auto invalidResult = extractWindowsFileDropDataObject(&invalid);
  if (!expect(invalidResult.error == WindowsFileDropError::InvalidStorage, "invalid medium is rejected") ||
      !expect(invalidReleaser.releaseCalls() == 1, "invalid medium is still released")) {
    return false;
  }

  const DropBlock empty({});
  ReleaseTracker emptyReleaser;
  FakeDataObject emptyObject;
  emptyObject.setHGlobal(reinterpret_cast<HGLOBAL>(empty.get()), &emptyReleaser);
  const auto emptyResult = extractWindowsFileDropDataObject(&emptyObject);
  return expect(emptyResult.error == WindowsFileDropError::EmptyDrop, "empty data-object payload is rejected") &&
         expect(emptyReleaser.releaseCalls() == 1, "rejected payload medium is released") &&
         expect(emptyResult.paths.empty(), "rejected payload has no paths");
}

bool forwardsLimitsAndReleasesDataObjectPayload()
{
  const DropBlock drop({L"C:\\one.txt", L"C:\\two.txt"});
  ReleaseTracker releaser;
  FakeDataObject dataObject;
  dataObject.setHGlobal(reinterpret_cast<HGLOBAL>(drop.get()), &releaser);
  const WindowsFileDropLimits limits{.maxItems = 1};

  const auto result = extractWindowsFileDropDataObject(&dataObject, limits);
  return expect(result.error == WindowsFileDropError::TooManyItems, "data-object item limit is forwarded") &&
         expect(result.paths.empty(), "limited data-object result is atomic") &&
         expect(releaser.releaseCalls() == 1, "limited data-object medium is released");
}

struct TestCase
{
  const char *name;
  bool (*run)();
};

} // namespace

int main()
{
  const TestCase tests[] = {
      {"rejectsNullAndEmptyDrops", rejectsNullAndEmptyDrops},
      {"extractsOneUnicodePath", extractsOneUnicodePath},
      {"preservesMultiplePathOrder", preservesMultiplePathOrder},
      {"supportsPathsBeyondLegacyMaxPath", supportsPathsBeyondLegacyMaxPath},
      {"honorsItemLimitBoundary", honorsItemLimitBoundary},
      {"honorsPathLimitBoundary", honorsPathLimitBoundary},
      {"discardsEarlierPathsWhenALaterPathFails", discardsEarlierPathsWhenALaterPathFails},
      {"rejectsNullUnsupportedAndFailedDataObjects", rejectsNullUnsupportedAndFailedDataObjects},
      {"extractsAndReleasesDataObjectPayload", extractsAndReleasesDataObjectPayload},
      {"releasesInvalidAndRejectedDataObjectPayloads", releasesInvalidAndRejectedDataObjectPayloads},
      {"forwardsLimitsAndReleasesDataObjectPayload", forwardsLimitsAndReleasesDataObjectPayload},
  };

  int failures = 0;
  for (const auto &test : tests) {
    try {
      if (test.run()) {
        std::cout << "PASS: " << test.name << '\n';
      } else {
        ++failures;
      }
    } catch (const std::exception &error) {
      std::cerr << "FAIL: " << test.name << ": " << error.what() << '\n';
      ++failures;
    }
  }

  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
