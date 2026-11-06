// SPDX-FileCopyrightText: (C) 2026 Ieum contributors
// SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
#include "SessionPlatform.h"
#include "deskflow/CanonicalScancode.h"
#import <AppKit/AppKit.h>
#import <AudioToolbox/AudioToolbox.h>
#import <Carbon/Carbon.h>
#import <CoreMedia/CoreMedia.h>
#import <IOKit/pwr_mgt/IOPMLib.h>
#include <QCoreApplication>
#include <QPointer>
#include <QProcess>
#include <QTimer>
#import <ScreenCaptureKit/ScreenCaptureKit.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <deque>
#include <functional>
#include <mutex>

API_AVAILABLE(macos(13.0))
@interface IeumSystemAudioSink : NSObject <SCStreamOutput, SCStreamDelegate> {
@public
  std::function<void(QByteArray)> samples;
  std::function<void()> failed;
}
@end
@implementation IeumSystemAudioSink
- (void)stream:(SCStream *)stream didStopWithError:(NSError *)error
{
  (void)stream;
  (void)error;
  if (failed)
    failed();
}
- (void)stream:(SCStream *)stream didOutputSampleBuffer:(CMSampleBufferRef)sample ofType:(SCStreamOutputType)type
{
  (void)stream;
  if (type != SCStreamOutputTypeAudio || !CMSampleBufferDataIsReady(sample))
    return;
  const auto frames = CMSampleBufferGetNumSamples(sample);
  const auto *format = CMAudioFormatDescriptionGetStreamBasicDescription(CMSampleBufferGetFormatDescription(sample));
  if (!format || frames <= 0 || frames > 48000 || format->mSampleRate != 48000 || format->mChannelsPerFrame < 1 ||
      format->mChannelsPerFrame > 2)
    return;
  const bool floating = (format->mFormatFlags & kAudioFormatFlagIsFloat) != 0;
  if ((floating && format->mBitsPerChannel != 32) || (!floating && format->mBitsPerChannel != 16))
    return;
  struct Buffers
  {
    AudioBufferList list;
    AudioBuffer extra;
  } buffers{};
  CMBlockBufferRef block = nullptr;
  if (CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(
          sample, nullptr, &buffers.list, sizeof(buffers), nullptr, nullptr,
          kCMSampleBufferFlag_AudioBufferList_Assure16ByteAlignment, &block
      ) != noErr)
    return;
  QByteArray pcm(static_cast<qsizetype>(frames * 4), Qt::Uninitialized);
  auto *output = reinterpret_cast<int16_t *>(pcm.data());
  const bool planar = (format->mFormatFlags & kAudioFormatFlagIsNonInterleaved) != 0;
  bool valid = buffers.list.mNumberBuffers >= (planar ? format->mChannelsPerFrame : 1);
  for (CMItemCount frame = 0; valid && frame < frames; ++frame) {
    for (int channel = 0; channel < 2; ++channel) {
      const auto sourceChannel = std::min<UInt32>(channel, format->mChannelsPerFrame - 1);
      const auto &buffer = buffers.list.mBuffers[planar ? sourceChannel : 0];
      const auto index = planar ? frame : frame * format->mChannelsPerFrame + sourceChannel;
      if (!buffer.mData || (index + 1) * (floating ? 4 : 2) > buffer.mDataByteSize) {
        valid = false;
        break;
      }
      if (floating) {
        const auto value = static_cast<const float *>(buffer.mData)[index];
        output[frame * 2 + channel] =
            std::isfinite(value) ? static_cast<int16_t>(std::clamp(value, -1.0f, 1.0f) * 32767.0f) : 0;
      } else
        output[frame * 2 + channel] = static_cast<const int16_t *>(buffer.mData)[index];
    }
  }
  if (block)
    CFRelease(block);
  if (valid && samples)
    samples(std::move(pcm));
}
@end

namespace deskflow::gui::session {
namespace {
constexpr int64_t kInjectedTag = 0x4945554d;
struct API_AVAILABLE(macos(13.0)) AudioCaptureState
{
  std::atomic<bool> canceled{false};
  std::atomic<int> queued{0};
  __strong SCStream *stream = nil;
  __strong IeumSystemAudioSink *sink = nil;
};
class MacSessionPlatform final : public SessionPlatform
{
public:
  ~MacSessionPlatform() override
  {
    stopAudioCapture();
    stopAudioPlayback();
    stopInputCapture();
    releaseInput();
  }
  bool requestScreenPermission() override
  {
    return CGPreflightScreenCaptureAccess() || CGRequestScreenCaptureAccess() ||
           fail(
               SessionPlatform::tr(
                   "Allow Ieum in System Settings → Privacy & Security → Screen Recording, then reopen Ieum."
               )
           );
  }
  bool startAudioCapture() override
  {
    stopAudioCapture();
    if (@available(macOS 13.0, *)) {
      if (!requestScreenPermission())
        return false;
      const auto state = std::make_shared<AudioCaptureState>();
      m_capture = state;
      QPointer<SessionPlatform> owner(this);
      const std::weak_ptr<AudioCaptureState> weakState = state;
      state->sink = [IeumSystemAudioSink new];
      state->sink->failed = [owner, weakState] {
        const auto state = weakState.lock();
        if (!state || state->canceled)
          return;
        QMetaObject::invokeMethod(
            qApp,
            [owner, state] {
              if (owner && !state->canceled)
                Q_EMIT owner->failure(
                    "audio-sharing",
                    SessionPlatform::tr("macOS system audio capture stopped. Check recording permission.")
                );
            },
            Qt::QueuedConnection
        );
      };
      state->sink->samples = [owner, weakState](QByteArray pcm) {
        const auto state = weakState.lock();
        if (!state || state->canceled)
          return;
        if (state->queued.fetch_add(1) >= 4) {
          state->queued.fetch_sub(1);
          return;
        }
        QMetaObject::invokeMethod(
            qApp,
            [owner, state, pcm] {
              state->queued.fetch_sub(1);
              if (owner && !state->canceled)
                for (qsizetype offset = 0; offset < pcm.size(); offset += 3840)
                  Q_EMIT owner->audioCaptured(pcm.mid(offset, 3840));
            },
            Qt::QueuedConnection
        );
      };
      [SCShareableContent
          getShareableContentExcludingDesktopWindows:YES
                                 onScreenWindowsOnly:YES
                                   completionHandler:^(SCShareableContent *content, NSError *error) {
                                     dispatch_async(dispatch_get_main_queue(), ^{
                                       if (state->canceled)
                                         return;
                                       if (error || content.displays.count == 0) {
                                         state->sink->failed();
                                         return;
                                       }
                                       SCContentFilter *filter =
                                           [[SCContentFilter alloc] initWithDisplay:content.displays.firstObject
                                                                   excludingWindows:@[]];
                                       SCStreamConfiguration *configuration = [SCStreamConfiguration new];
                                       configuration.width = 2;
                                       configuration.height = 2;
                                       configuration.minimumFrameInterval = CMTimeMake(1, 1);
                                       configuration.capturesAudio = YES;
                                       configuration.excludesCurrentProcessAudio = YES;
                                       configuration.sampleRate = 48000;
                                       configuration.channelCount = 2;
                                       configuration.queueDepth = 3;
                                       state->stream = [[SCStream alloc] initWithFilter:filter
                                                                          configuration:configuration
                                                                               delegate:state->sink];
                                       NSError *outputError = nil;
                                       if (![state->stream
                                                  addStreamOutput:state->sink
                                                             type:SCStreamOutputTypeAudio
                                               sampleHandlerQueue:dispatch_get_global_queue(QOS_CLASS_USER_INITIATED, 0)
                                                            error:&outputError]) {
                                         state->sink->failed();
                                         return;
                                       }
                                       IeumSystemAudioSink *sink = state->sink;
                                       [state->stream startCaptureWithCompletionHandler:^(NSError *startError) {
                                         if (startError && !state->canceled)
                                           sink->failed();
                                       }];
                                     });
                                   }];
      return true;
    }
    return fail(SessionPlatform::tr("Sharing system audio requires macOS 13 or later."));
  }
  void stopAudioCapture() override
  {
    if (@available(macOS 13.0, *)) {
      if (!m_capture)
        return;
      auto state = std::move(m_capture);
      state->canceled = true;
      // Callbacks hold a weak reference, so stopping never races a std::function mutation.
      if (state->stream)
        [state->stream stopCaptureWithCompletionHandler:^(NSError *) {
          dispatch_async(dispatch_get_main_queue(), ^{
            state->stream = nil;
            state->sink = nil;
          });
        }];
      else
        state->sink = nil;
    }
  }
  bool playAudio(const QByteArray &pcm) override
  {
    if (pcm.isEmpty() || pcm.size() > 15360 || pcm.size() % 4)
      return false;
    {
      std::lock_guard lock(m_audioMutex);
      while (m_audioBytes + pcm.size() > 38400 && !m_audio.empty()) {
        m_audioBytes -= m_audio.front().size();
        m_audio.pop_front();
      }
      m_audio.push_back(pcm);
      m_audioBytes += pcm.size();
    }
    if (m_queue)
      return true;
    AudioStreamBasicDescription format{};
    format.mSampleRate = 48000;
    format.mFormatID = kAudioFormatLinearPCM;
    format.mFormatFlags = kLinearPCMFormatFlagIsSignedInteger | kLinearPCMFormatFlagIsPacked;
    format.mBytesPerPacket = 4;
    format.mFramesPerPacket = 1;
    format.mBytesPerFrame = 4;
    format.mChannelsPerFrame = 2;
    format.mBitsPerChannel = 16;
    if (AudioQueueNewOutput(&format, renderAudio, this, nullptr, nullptr, 0, &m_queue) != noErr)
      return fail(SessionPlatform::tr("macOS audio output is unavailable."));
    for (int i = 0; i < 3; ++i) {
      AudioQueueBufferRef buffer = nullptr;
      if (AudioQueueAllocateBuffer(m_queue, 3840, &buffer) != noErr) {
        stopAudioPlayback();
        return false;
      }
      renderAudio(this, m_queue, buffer);
    }
    if (AudioQueueStart(m_queue, nullptr) != noErr) {
      stopAudioPlayback();
      return false;
    }
    return true;
  }
  void stopAudioPlayback() override
  {
    if (m_queue) {
      AudioQueueStop(m_queue, true);
      AudioQueueDispose(m_queue, true);
      m_queue = nullptr;
    }
    std::lock_guard lock(m_audioMutex);
    m_audio.clear();
    m_audioBytes = 0;
  }
  bool startInputCapture(InputMode mode) override
  {
    stopInputCapture();
    m_mode = mode;
    if (!AXIsProcessTrusted())
      return fail(SessionPlatform::tr("Allow Ieum in System Settings → Privacy & Security → Accessibility."));
    const CGEventMask events = CGEventMaskBit(kCGEventKeyDown) | CGEventMaskBit(kCGEventKeyUp) |
                               CGEventMaskBit(kCGEventFlagsChanged) | CGEventMaskBit(kCGEventMouseMoved) |
                               CGEventMaskBit(kCGEventLeftMouseDown) | CGEventMaskBit(kCGEventLeftMouseUp) |
                               CGEventMaskBit(kCGEventRightMouseDown) | CGEventMaskBit(kCGEventRightMouseUp) |
                               CGEventMaskBit(kCGEventOtherMouseDown) | CGEventMaskBit(kCGEventOtherMouseUp) |
                               CGEventMaskBit(kCGEventScrollWheel) | CGEventMaskBit(kCGEventLeftMouseDragged) |
                               CGEventMaskBit(kCGEventRightMouseDragged) | CGEventMaskBit(kCGEventOtherMouseDragged);
    m_tap =
        CGEventTapCreate(kCGHIDEventTap, kCGHeadInsertEventTap, kCGEventTapOptionDefault, events, captureInput, this);
    if (!m_tap)
      return fail(SessionPlatform::tr("macOS did not allow input monitoring."));
    m_source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, m_tap, 0);
    CFRunLoopAddSource(CFRunLoopGetMain(), m_source, kCFRunLoopCommonModes);
    CGEventTapEnable(m_tap, true);
    return true;
  }
  void stopInputCapture() override
  {
    if (m_source) {
      CFRunLoopRemoveSource(CFRunLoopGetMain(), m_source, kCFRunLoopCommonModes);
      CFRelease(m_source);
      m_source = nullptr;
    }
    if (m_tap) {
      CGEventTapEnable(m_tap, false);
      CFMachPortInvalidate(m_tap);
      CFRelease(m_tap);
      m_tap = nullptr;
    }
    m_physicalKeys.clear();
  }
  bool inject(const Input &input) override
  {
    CGEventRef event = nullptr;
    CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStatePrivate);
    if (!source)
      return false;
    if (input.kind == Input::Kind::Key) {
      if (input.code == 0xf2) {
        CFRelease(source);
        return !input.down || toggleKorean();
      }
      const auto key = deskflow::scancode::macVirtualKeyFromSet1(input.code);
      if (!key) {
        CFRelease(source);
        return false;
      }
      const bool repeated = input.down && m_keys.contains(input.code);
      event = CGEventCreateKeyboardEvent(source, *key, input.down);
      if (input.down)
        m_keys.insert(input.code);
      else
        m_keys.remove(input.code);
      if (event) {
        CGEventSetFlags(event, modifierFlags(m_keys));
        CGEventSetIntegerValueField(event, kCGKeyboardEventAutorepeat, repeated);
      }
    } else if (input.kind == Input::Kind::Move) {
      const auto bounds = desktopBounds();
      const CGPoint point{
          bounds.origin.x + input.x * (bounds.size.width - 1) / 65535.0,
          bounds.origin.y + input.y * (bounds.size.height - 1) / 65535.0
      };
      const auto type = m_buttons.contains(1)   ? kCGEventLeftMouseDragged
                        : m_buttons.contains(3) ? kCGEventRightMouseDragged
                        : m_buttons.contains(2) ? kCGEventOtherMouseDragged
                                                : kCGEventMouseMoved;
      event = CGEventCreateMouseEvent(
          source, type, point,
          m_buttons.contains(3)   ? kCGMouseButtonRight
          : m_buttons.contains(2) ? kCGMouseButtonCenter
                                  : kCGMouseButtonLeft
      );
    } else if (input.kind == Input::Kind::Button) {
      auto position = CGEventCreate(nullptr);
      const auto point = CGEventGetLocation(position);
      CFRelease(position);
      const auto button = input.code == 1   ? kCGMouseButtonLeft
                          : input.code == 3 ? kCGMouseButtonRight
                          : input.code == 2 ? kCGMouseButtonCenter
                                            : static_cast<CGMouseButton>(input.code - 1);
      const auto type = input.code == 1   ? (input.down ? kCGEventLeftMouseDown : kCGEventLeftMouseUp)
                        : input.code == 3 ? (input.down ? kCGEventRightMouseDown : kCGEventRightMouseUp)
                                          : (input.down ? kCGEventOtherMouseDown : kCGEventOtherMouseUp);
      event = CGEventCreateMouseEvent(source, type, point, button);
      if (input.down)
        m_buttons.insert(input.code);
      else
        m_buttons.remove(input.code);
    } else if (input.kind == Input::Kind::Wheel)
      event = CGEventCreateScrollWheelEvent(source, kCGScrollEventUnitLine, 2, input.y / 120, input.x / 120);
    CFRelease(source);
    if (!event)
      return false;
    CGEventSetIntegerValueField(event, kCGEventSourceUserData, kInjectedTag);
    CGEventPost(kCGHIDEventTap, event);
    CFRelease(event);
    return true;
  }
  void releaseInput() override
  {
    const auto keys = m_keys, buttons = m_buttons;
    for (int key : keys)
      (void)inject({Input::Kind::Key, key, false});
    for (int button : buttons)
      (void)inject({Input::Kind::Button, button, false});
    m_keys.clear();
    m_buttons.clear();
  }
  bool lockDesktop() override
  {
    QProcess process;
    process.start("/System/Library/CoreServices/Menu Extras/User.menu/Contents/Resources/CGSession", {"-suspend"});
    return (process.waitForFinished(2000) && process.exitCode() == 0) ||
           fail(SessionPlatform::tr("macOS could not lock the current session."));
  }
  bool wakeDisplays() override
  {
    IOPMAssertionID assertion = kIOPMNullAssertionID;
    const auto result = IOPMAssertionDeclareUserActivity(CFSTR("Ieum display wake"), kIOPMUserActiveLocal, &assertion);
    if (assertion != kIOPMNullAssertionID)
      IOPMAssertionRelease(assertion);
    return result == kIOReturnSuccess;
  }
  bool locked() override
  {
    const auto dictionary = CGSessionCopyCurrentDictionary();
    if (!dictionary)
      return true;
    const auto value = CFDictionaryGetValue(dictionary, CFSTR("CGSSessionScreenIsLocked"));
    const bool result = value == kCFBooleanTrue;
    CFRelease(dictionary);
    return result;
  }

private:
  static CGRect desktopBounds()
  {
    CGDirectDisplayID displays[32]{};
    uint32_t count = 0;
    CGRect bounds = CGRectNull;
    if (CGGetActiveDisplayList(32, displays, &count) == kCGErrorSuccess)
      for (uint32_t i = 0; i < count; ++i)
        bounds = CGRectUnion(bounds, CGDisplayBounds(displays[i]));
    return CGRectIsNull(bounds) ? CGDisplayBounds(CGMainDisplayID()) : bounds;
  }
  static CGEventFlags modifierFlags(const QSet<int> &keys)
  {
    CGEventFlags flags = 0;
    if (keys.contains(0x2a) || keys.contains(0x36))
      flags |= kCGEventFlagMaskShift;
    if (keys.contains(0x1d) || keys.contains(0x11d))
      flags |= kCGEventFlagMaskControl;
    if (keys.contains(0x38) || keys.contains(0x138))
      flags |= kCGEventFlagMaskAlternate;
    if (keys.contains(0x15b) || keys.contains(0x15c))
      flags |= kCGEventFlagMaskCommand;
    return flags;
  }
  bool toggleKorean()
  {
    auto current = TISCopyCurrentKeyboardInputSource();
    auto identifier =
        current ? static_cast<CFStringRef>(TISGetInputSourceProperty(current, kTISPropertyInputSourceID)) : nullptr;
    const bool korean =
        identifier && CFStringFind(identifier, CFSTR("Korean"), kCFCompareCaseInsensitive).location != kCFNotFound;
    auto sources = TISCreateInputSourceList(nullptr, false);
    bool selected = false;
    for (CFIndex i = 0; sources && i < CFArrayGetCount(sources); ++i) {
      auto source = static_cast<TISInputSourceRef>(const_cast<void *>(CFArrayGetValueAtIndex(sources, i)));
      auto id = static_cast<CFStringRef>(TISGetInputSourceProperty(source, kTISPropertyInputSourceID));
      const bool matches =
          id && (korean ? CFEqual(id, CFSTR("com.apple.keylayout.ABC")) || CFEqual(id, CFSTR("com.apple.keylayout.US"))
                        : CFEqual(id, CFSTR("com.apple.inputmethod.Korean.2SetKorean")));
      if (matches && TISSelectInputSource(source) == noErr) {
        selected = true;
        break;
      }
    }
    if (sources)
      CFRelease(sources);
    if (current)
      CFRelease(current);
    return selected;
  }
  static CGEventRef captureInput(CGEventTapProxy, CGEventType type, CGEventRef event, void *context)
  {
    auto *self = static_cast<MacSessionPlatform *>(context);
    if (type == kCGEventTapDisabledByTimeout || type == kCGEventTapDisabledByUserInput) {
      QMetaObject::invokeMethod(
          self,
          [self] {
            Q_EMIT self->failure(
                "bidirectional-control", SessionPlatform::tr("macOS paused input capture. Restart the sharing session.")
            );
          },
          Qt::QueuedConnection
      );
      return event;
    }
    if (CGEventGetIntegerValueField(event, kCGEventSourceUserData) == kInjectedTag ||
        CGEventGetIntegerValueField(event, kCGEventSourceStateID) == kCGEventSourceStatePrivate)
      return event;
    Input input;
    if (type == kCGEventKeyDown || type == kCGEventKeyUp || type == kCGEventFlagsChanged) {
      const auto key = static_cast<uint16_t>(CGEventGetIntegerValueField(event, kCGKeyboardEventKeycode));
      const auto scan = deskflow::scancode::set1FromMacVirtualKey(key);
      if (!scan)
        return event;
      const bool down = type == kCGEventFlagsChanged
                            ? CGEventSourceKeyState(kCGEventSourceStateCombinedSessionState, key)
                            : type == kCGEventKeyDown;
      input = {Input::Kind::Key, *scan, down};
      if (down)
        self->m_physicalKeys.insert(*scan);
      else
        self->m_physicalKeys.remove(*scan);
      if (down && *scan == 1 && (modifierFlags(self->m_physicalKeys) & kCGEventFlagMaskControl) &&
          (modifierFlags(self->m_physicalKeys) & kCGEventFlagMaskAlternate))
        input.kind = Input::Kind::Escape;
    } else if (type == kCGEventScrollWheel) {
      input = {
          Input::Kind::Wheel, 0, false,
          static_cast<int>(
              std::clamp<int64_t>(CGEventGetIntegerValueField(event, kCGScrollWheelEventDeltaAxis2) * 120, -1200, 1200)
          ),
          static_cast<int>(
              std::clamp<int64_t>(CGEventGetIntegerValueField(event, kCGScrollWheelEventDeltaAxis1) * 120, -1200, 1200)
          )
      };
    } else if (type == kCGEventMouseMoved || type == kCGEventLeftMouseDragged || type == kCGEventRightMouseDragged ||
               type == kCGEventOtherMouseDragged) {
      const auto bounds = desktopBounds();
      const auto point = CGEventGetLocation(event);
      input.x = std::clamp(
          static_cast<int>((point.x - bounds.origin.x) * 65535 / std::max(1.0, bounds.size.width - 1)), 0, 65535
      );
      input.y = std::clamp(
          static_cast<int>((point.y - bounds.origin.y) * 65535 / std::max(1.0, bounds.size.height - 1)), 0, 65535
      );
    } else {
      const auto button = CGEventGetIntegerValueField(event, kCGMouseEventButtonNumber);
      input = {
          Input::Kind::Button,
          button == 0   ? 1
          : button == 1 ? 3
          : button == 2 ? 2
                        : static_cast<int>(button) + 1,
          type == kCGEventLeftMouseDown || type == kCGEventRightMouseDown || type == kCGEventOtherMouseDown
      };
      if (input.code > 5)
        return event;
    }
    const bool suppress = self->m_mode == InputMode::Control && input.kind != Input::Kind::Move;
    Q_EMIT self->inputCaptured(input);
    return suppress ? nullptr : event;
  }
  static void renderAudio(void *context, AudioQueueRef queue, AudioQueueBufferRef buffer)
  {
    auto *self = static_cast<MacSessionPlatform *>(context);
    memset(buffer->mAudioData, 0, buffer->mAudioDataBytesCapacity);
    qsizetype written = 0;
    {
      std::lock_guard lock(self->m_audioMutex);
      while (written < buffer->mAudioDataBytesCapacity && !self->m_audio.empty()) {
        auto &front = self->m_audio.front();
        const auto amount = std::min<qsizetype>(front.size(), buffer->mAudioDataBytesCapacity - written);
        memcpy(static_cast<char *>(buffer->mAudioData) + written, front.constData(), amount);
        front.remove(0, amount);
        written += amount;
        self->m_audioBytes -= amount;
        if (front.isEmpty())
          self->m_audio.pop_front();
      }
    }
    buffer->mAudioDataByteSize = buffer->mAudioDataBytesCapacity;
    AudioQueueEnqueueBuffer(queue, buffer, 0, nullptr);
  }
  std::shared_ptr<AudioCaptureState> m_capture API_AVAILABLE(macos(13.0));
  CFMachPortRef m_tap = nullptr;
  CFRunLoopSourceRef m_source = nullptr;
  InputMode m_mode = InputMode::Observe;
  QSet<int> m_keys, m_buttons, m_physicalKeys;
  AudioQueueRef m_queue = nullptr;
  std::mutex m_audioMutex;
  std::deque<QByteArray> m_audio;
  qsizetype m_audioBytes = 0;
};
} // namespace
std::unique_ptr<SessionPlatform> createSessionPlatform()
{
  return std::make_unique<MacSessionPlatform>();
}
} // namespace deskflow::gui::session
