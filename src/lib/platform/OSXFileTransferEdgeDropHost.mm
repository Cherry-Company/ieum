/*
 * Ieum -- IME-native software KVM
 * SPDX-FileCopyrightText: (C) 2026 Ieum contributors
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "platform/OSXFileTransferEdgeDropHost.h"

#include <AppKit/AppKit.h>
#include <dispatch/dispatch.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace deskflow::filetransfer {

std::vector<OSXFileTransferEdgeDropWindow> makeOSXFileTransferEdgeDropLayout(
    const deskflow::DisplayLayout &displays, std::uint32_t activeSides, std::int32_t thickness
)
{
  if (displays.empty() || activeSides == 0 || thickness <= 0) {
    return {};
  }
  std::int64_t left = std::numeric_limits<std::int32_t>::max();
  std::int64_t top = std::numeric_limits<std::int32_t>::max();
  std::int64_t right = std::numeric_limits<std::int32_t>::min();
  std::int64_t bottom = std::numeric_limits<std::int32_t>::min();
  for (const auto &display : displays) {
    if (display.width <= 0 || display.height <= 0) {
      return {};
    }
    left = (std::min)(left, static_cast<std::int64_t>(display.x));
    top = (std::min)(top, static_cast<std::int64_t>(display.y));
    right = (std::max)(right, static_cast<std::int64_t>(display.x) + display.width);
    bottom = (std::max)(bottom, static_cast<std::int64_t>(display.y) + display.height);
  }
  if (left < std::numeric_limits<std::int32_t>::min() || right > std::numeric_limits<std::int32_t>::max() ||
      top < std::numeric_limits<std::int32_t>::min() || bottom > std::numeric_limits<std::int32_t>::max() ||
      right <= left || bottom <= top) {
    return {};
  }

  const auto x = static_cast<std::int32_t>(left);
  const auto y = static_cast<std::int32_t>(top);
  const auto width = static_cast<std::int32_t>(right - left);
  const auto height = static_cast<std::int32_t>(bottom - top);
  const auto edge = (std::min)({thickness, width, height});
  const auto hasSide = [activeSides](DirectionMask side) {
    return (activeSides & static_cast<std::uint32_t>(side)) != 0;
  };

  std::vector<OSXFileTransferEdgeDropWindow> result;
  result.reserve(4);
  using enum DirectionMask;
  if (hasSide(LeftMask)) {
    result.push_back({Direction::Left, x, y, edge, height});
  }
  if (hasSide(RightMask)) {
    result.push_back({Direction::Right, static_cast<std::int32_t>(right) - edge, y, edge, height});
  }
  if (width > edge * 2 && hasSide(TopMask)) {
    result.push_back({Direction::Top, x + edge, y, width - edge * 2, edge});
  }
  if (width > edge * 2 && hasSide(BottomMask)) {
    result.push_back({Direction::Bottom, x + edge, static_cast<std::int32_t>(bottom) - edge, width - edge * 2, edge});
  }
  return result;
}

OSXFileTransferDropPoint appKitToIeumFileDropPoint(double x, double y, double appKitDesktopTop) noexcept
{
  const auto clamp = [](double value) {
    if (!std::isfinite(value)) {
      return std::int32_t{0};
    }
    return static_cast<std::int32_t>(std::clamp(
        std::llround(value), static_cast<long long>(std::numeric_limits<std::int32_t>::min()),
        static_cast<long long>(std::numeric_limits<std::int32_t>::max())
    ));
  };
  return {.x = clamp(x), .y = clamp(appKitDesktopTop - y)};
}

OSXFileDropExtraction extractOSXFileDropURLs(NSArray *urls, std::size_t maxItems)
{
  if (urls == nil || urls.count == 0) {
    return {.error = OSXFileDropError::Empty};
  }
  if (urls.count > maxItems) {
    return {.error = OSXFileDropError::TooManyItems};
  }

  std::vector<std::filesystem::path> paths;
  paths.reserve(urls.count);
  for (NSURL *url in urls) {
    if (![url isKindOfClass:NSURL.class] || !url.fileURL) {
      return {.error = OSXFileDropError::RemoteUrl};
    }
    NSNumber *regular = nil;
    NSNumber *directory = nil;
    NSNumber *package = nil;
    NSError *error = nil;
    if (![url getResourceValue:&regular forKey:NSURLIsRegularFileKey error:&error] || !regular.boolValue ||
        ![url getResourceValue:&directory forKey:NSURLIsDirectoryKey error:&error] || directory.boolValue ||
        ![url getResourceValue:&package forKey:NSURLIsPackageKey error:&error] || package.boolValue) {
      return {.error = OSXFileDropError::DirectoryOrPackage};
    }
    const char *representation = url.fileSystemRepresentation;
    if (representation == nullptr) {
      return {.error = OSXFileDropError::InvalidPath};
    }
    std::filesystem::path path(representation);
    if (path.empty() || !path.is_absolute()) {
      return {.error = OSXFileDropError::InvalidPath};
    }
    paths.push_back(std::move(path));
  }
  return {.paths = std::move(paths)};
}

} // namespace deskflow::filetransfer

@interface IeumFileTransferEdgeDropView : NSView <NSDraggingDestination> {
@private
  Direction _direction;
  deskflow::filetransfer::FileTransferEdgeDropHandler _handler;
  std::chrono::milliseconds _dwell;
  std::chrono::steady_clock::time_point _enteredAt;
  double _appKitDesktopTop;
  BOOL _armed;
}
- (instancetype)initWithFrame:(NSRect)frame
                    direction:(Direction)direction
                      handler:(deskflow::filetransfer::FileTransferEdgeDropHandler)handler
                        dwell:(std::chrono::milliseconds)dwell
                appKitTopEdge:(double)appKitDesktopTop;
@end

@implementation IeumFileTransferEdgeDropView

- (instancetype)initWithFrame:(NSRect)frame
                    direction:(Direction)direction
                      handler:(deskflow::filetransfer::FileTransferEdgeDropHandler)handler
                        dwell:(std::chrono::milliseconds)dwell
                appKitTopEdge:(double)appKitDesktopTop
{
  self = [super initWithFrame:frame];
  if (self != nil) {
    _direction = direction;
    _handler = std::move(handler);
    _dwell = (std::max)(dwell, std::chrono::milliseconds::zero());
    _appKitDesktopTop = appKitDesktopTop;
    _armed = NO;
    [self registerForDraggedTypes:@[ NSPasteboardTypeFileURL ]];
  }
  return self;
}

- (BOOL)hasAcceptableFiles:(id<NSDraggingInfo>)sender
{
  NSDictionary *options = @{NSPasteboardURLReadingFileURLsOnlyKey : @YES};
  NSArray<NSURL *> *urls = [sender.draggingPasteboard readObjectsForClasses:@[ NSURL.class ] options:options];
  return deskflow::filetransfer::extractOSXFileDropURLs(urls).ok();
}

- (NSDragOperation)updateDwell:(id<NSDraggingInfo>)sender
{
  if (![self hasAcceptableFiles:sender]) {
    _armed = NO;
    return NSDragOperationNone;
  }
  _armed = std::chrono::steady_clock::now() - _enteredAt >= _dwell;
  return _armed ? NSDragOperationCopy : NSDragOperationNone;
}

- (NSDragOperation)draggingEntered:(id<NSDraggingInfo>)sender
{
  _enteredAt = std::chrono::steady_clock::now();
  _armed = _dwell == std::chrono::milliseconds::zero();
  if (![self hasAcceptableFiles:sender]) {
    _armed = NO;
  }
  return _armed ? NSDragOperationCopy : NSDragOperationNone;
}

- (NSDragOperation)draggingUpdated:(id<NSDraggingInfo>)sender
{
  return [self updateDwell:sender];
}

- (void)draggingExited:(id<NSDraggingInfo>)sender
{
  (void)sender;
  _armed = NO;
}

- (BOOL)prepareForDragOperation:(id<NSDraggingInfo>)sender
{
  return [self updateDwell:sender] == NSDragOperationCopy;
}

- (BOOL)performDragOperation:(id<NSDraggingInfo>)sender
{
  if (!_armed || !_handler) {
    return NO;
  }
  NSDictionary *options = @{NSPasteboardURLReadingFileURLsOnlyKey : @YES};
  NSArray<NSURL *> *urls = [sender.draggingPasteboard readObjectsForClasses:@[ NSURL.class ] options:options];
  auto extraction = deskflow::filetransfer::extractOSXFileDropURLs(urls);
  if (!extraction.ok()) {
    _armed = NO;
    return NO;
  }
  const auto screenPoint = [self.window convertPointToScreen:sender.draggingLocation];
  const auto point = deskflow::filetransfer::appKitToIeumFileDropPoint(screenPoint.x, screenPoint.y, _appKitDesktopTop);
  _armed = NO;
  _handler(
      deskflow::filetransfer::FileTransferEdgeDrop{
          .direction = _direction,
          .x = point.x,
          .y = point.y,
          .paths = std::move(extraction.paths),
      }
  );
  return YES;
}

- (void)concludeDragOperation:(id<NSDraggingInfo>)sender
{
  (void)sender;
  _armed = NO;
}

@end

namespace deskflow::filetransfer {
namespace {

double appKitDesktopTop()
{
  NSScreen *main = NSScreen.mainScreen;
  return main == nil ? 0.0 : NSMaxY(main.frame);
}

void clearPanels(std::vector<NSPanel *> &panels)
{
  for (NSPanel *panel : panels) {
    [panel orderOut:nil];
    [panel setContentView:nil];
    [panel close];
    [panel release];
  }
  panels.clear();
}

template <typename Function> void onMainThread(Function &&function)
{
  if (NSThread.isMainThread) {
    function();
    return;
  }
  dispatch_sync(dispatch_get_main_queue(), ^{
    function();
  });
}

} // namespace

struct OSXFileTransferEdgeDropHost::Impl
{
  FileTransferEdgeDropHandler handler;
  std::chrono::milliseconds dwell;
  std::vector<NSPanel *> panels;
};

OSXFileTransferEdgeDropHost::OSXFileTransferEdgeDropHost(
    FileTransferEdgeDropHandler handler, std::chrono::milliseconds dwell
)
    : m_impl(std::make_unique<Impl>(Impl{.handler = std::move(handler), .dwell = dwell}))
{
}

OSXFileTransferEdgeDropHost::~OSXFileTransferEdgeDropHost()
{
  clear();
}

bool OSXFileTransferEdgeDropHost::configure(const deskflow::DisplayLayout &displays, std::uint32_t activeSides)
{
  const auto layout = makeOSXFileTransferEdgeDropLayout(displays, activeSides);
  if (activeSides != 0 && layout.empty()) {
    return false;
  }

  bool configured = true;
  onMainThread([&] {
    @autoreleasepool {
      clearPanels(m_impl->panels);
      const auto topEdge = appKitDesktopTop();
      for (const auto &window : layout) {
        const NSRect frame = NSMakeRect(
            window.x, topEdge - (static_cast<double>(window.y) + window.height), window.width, window.height
        );
        auto *panel =
            [[NSPanel alloc] initWithContentRect:frame
                                       styleMask:(NSWindowStyleMaskBorderless | NSWindowStyleMaskNonactivatingPanel)
                                         backing:NSBackingStoreBuffered
                                           defer:NO];
        if (panel == nil) {
          configured = false;
          break;
        }
        [panel setReleasedWhenClosed:NO];
        [panel setOpaque:NO];
        [panel setBackgroundColor:NSColor.clearColor];
        [panel setAlphaValue:0.01];
        [panel setHasShadow:NO];
        [panel setIgnoresMouseEvents:NO];
        [panel setLevel:NSStatusWindowLevel];
        [panel setCollectionBehavior:(NSWindowCollectionBehaviorCanJoinAllSpaces |
                                      NSWindowCollectionBehaviorFullScreenAuxiliary |
                                      NSWindowCollectionBehaviorStationary)];
        auto *view = [[IeumFileTransferEdgeDropView alloc] initWithFrame:NSMakeRect(0, 0, window.width, window.height)
                                                               direction:window.direction
                                                                 handler:m_impl->handler
                                                                   dwell:m_impl->dwell
                                                           appKitTopEdge:topEdge];
        [panel setContentView:view];
        [view release];
        [panel orderFront:nil];
        m_impl->panels.push_back(panel);
      }
      if (!configured) {
        clearPanels(m_impl->panels);
      }
    }
  });
  return configured;
}

void OSXFileTransferEdgeDropHost::clear() noexcept
{
  try {
    onMainThread([this] { clearPanels(m_impl->panels); });
  } catch (...) {
  }
}

std::size_t OSXFileTransferEdgeDropHost::windowCount() const noexcept
{
  return m_impl->panels.size();
}

} // namespace deskflow::filetransfer
