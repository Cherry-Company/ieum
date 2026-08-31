/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXFileTransferEdgeDropHost.h"

#include <AppKit/AppKit.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>
#include <vector>

namespace {

using namespace deskflow::filetransfer;

void require(bool condition, std::string_view message)
{
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}

const OSXFileTransferEdgeDropWindow &
windowFor(const std::vector<OSXFileTransferEdgeDropWindow> &windows, Direction direction)
{
  const auto found = std::ranges::find(windows, direction, &OSXFileTransferEdgeDropWindow::direction);
  require(found != windows.end(), "expected edge window is missing");
  return *found;
}

std::uint32_t allSides()
{
  using enum DirectionMask;
  return static_cast<std::uint32_t>(LeftMask) | static_cast<std::uint32_t>(RightMask) |
         static_cast<std::uint32_t>(TopMask) | static_cast<std::uint32_t>(BottomMask);
}

void laysOutMultiDisplayDesktopWithoutCornerOverlap()
{
  const deskflow::DisplayLayout displays{{-1920, 0, 1920, 1080}, {0, -120, 2560, 1440}};
  const auto windows = makeOSXFileTransferEdgeDropLayout(displays, allSides(), 3);
  require(windows.size() == 4, "all active sides should produce one panel each");
  require(
      windowFor(windows, Direction::Left) == OSXFileTransferEdgeDropWindow{Direction::Left, -1920, -120, 3, 1440},
      "left panel should use Ieum top-left desktop coordinates"
  );
  require(
      windowFor(windows, Direction::Right) == OSXFileTransferEdgeDropWindow{Direction::Right, 2557, -120, 3, 1440},
      "right panel should use the multi-display bound"
  );
  require(
      windowFor(windows, Direction::Top) == OSXFileTransferEdgeDropWindow{Direction::Top, -1917, -120, 4474, 3},
      "top panel should exclude both corners"
  );
  require(
      windowFor(windows, Direction::Bottom) == OSXFileTransferEdgeDropWindow{Direction::Bottom, -1917, 1317, 4474, 3},
      "bottom panel should exclude both corners"
  );
  require(
      appKitToIeumFileDropPoint(-20.0, 900.0, 1440.0) == OSXFileTransferDropPoint{-20, 540},
      "AppKit bottom-left points should convert to Ieum top-left points"
  );
}

void extractsOnlyBoundedLocalRegularFiles()
{
  @autoreleasepool {
    const auto base = std::filesystem::temp_directory_path() / "ieum-osx-drop-host-tests";
    std::error_code error;
    std::filesystem::remove_all(base, error);
    std::filesystem::create_directory(base);
    const auto file = base / "한글 file.txt";
    std::ofstream(file) << "payload";

    NSURL *fileUrl = [NSURL fileURLWithPath:[NSString stringWithUTF8String:file.c_str()] isDirectory:NO];
    auto extracted = extractOSXFileDropURLs(@[ fileUrl ], 100);
    require(extracted.ok() && extracted.paths == std::vector{file}, "local regular file URL should extract");

    NSURL *remote = [NSURL URLWithString:@"https://example.com/file.txt"];
    require(extractOSXFileDropURLs(@[ remote ], 100).error == OSXFileDropError::RemoteUrl, "remote URL should fail");

    NSURL *directoryUrl = [NSURL fileURLWithPath:[NSString stringWithUTF8String:base.c_str()] isDirectory:YES];
    require(
        extractOSXFileDropURLs(@[ directoryUrl ], 100).error == OSXFileDropError::DirectoryOrPackage,
        "directory URL should fail"
    );

    NSMutableArray<NSURL *> *tooMany = [NSMutableArray array];
    for (NSUInteger index = 0; index < 101; ++index) {
      [tooMany addObject:fileUrl];
    }
    require(
        extractOSXFileDropURLs(tooMany, 100).error == OSXFileDropError::TooManyItems, "more than 100 items should fail"
    );
    std::filesystem::remove_all(base, error);
  }
}

void clearsConfiguredPanels()
{
  @autoreleasepool {
    OSXFileTransferEdgeDropHost host([](FileTransferEdgeDrop) {}, std::chrono::milliseconds::zero());
    require(
        host.configure({{0, 0, 800, 600}}, static_cast<std::uint32_t>(DirectionMask::LeftMask)),
        "host should configure one active panel"
    );
    require(host.windowCount() == 1, "one active side should create one panel");
    host.clear();
    require(host.windowCount() == 0, "clear should remove every panel");
  }
}

} // namespace

int main()
{
  laysOutMultiDisplayDesktopWithoutCornerOverlap();
  extractsOnlyBoundedLocalRegularFiles();
  clearsConfiguredPanels();
  std::cout << "PASS: OSXFileTransferEdgeDropHostTests\n";
  return 0;
}
