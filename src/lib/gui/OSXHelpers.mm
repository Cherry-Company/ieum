/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2015 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#import "OSXHelpers.h"
#include "MacApplicationLifecyclePolicy.h"
#include "MacLoginItems.h"

#import <Carbon/Carbon.h>
#import <Cocoa/Cocoa.h>
#import <CoreData/CoreData.h>
#import <Foundation/Foundation.h>
#import <ServiceManagement/ServiceManagement.h>
#import <UserNotifications/UNNotification.h>
#import <UserNotifications/UNNotificationContent.h>
#import <UserNotifications/UNNotificationTrigger.h>
#import <UserNotifications/UNUserNotificationCenter.h>
#import <objc/runtime.h>

#import <QtGlobal>

#include <QGuiApplication>
#include <QMetaObject>
#include <QObject>
#include <QPointer>

#include <utility>

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

@interface IeumApplicationReopenHandler : NSObject {
  QPointer<QObject> m_receiver;
}
- (instancetype)initWithReceiver:(QObject *)receiver;
- (void)handleReopenApplication:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent;
@end

@implementation IeumApplicationReopenHandler
- (instancetype)initWithReceiver:(QObject *)receiver
{
  self = [super init];
  if (self != nil) {
    m_receiver = receiver;
  }
  return self;
}

- (void)handleReopenApplication:(NSAppleEventDescriptor *)event withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
  Q_UNUSED(event)
  Q_UNUSED(replyEvent)
  if (auto *receiver = m_receiver.data(); receiver != nullptr) {
    QMetaObject::invokeMethod(receiver, "showAndActivate", Qt::QueuedConnection);
  }
}
@end

void requestOSXNotificationPermission()
{
#if OSX_DEPLOYMENT_TARGET >= 1014
  if (isOSXDevelopmentBuild()) {
    qWarning("Not requesting notification permission in dev build");
    return;
  }

  UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];
  [center requestAuthorizationWithOptions:(UNAuthorizationOptionAlert + UNAuthorizationOptionSound)
                        completionHandler:^(BOOL granted, NSError *_Nullable error) {
                          if (error != nil) {
                            qWarning(
                                "Notification permission request error: %s",
                                [[NSString stringWithFormat:@"%@", error] UTF8String]
                            );
                          }
                        }];
#endif
}

bool isOSXDevelopmentBuild()
{
  // Moving an unsigned/ad-hoc alpha build into Applications does not give it
  // a stable signing identity. Notification Center can terminate such a build
  // before returning an error, so never use its APIs based on the path alone.
#ifdef IEUM_MACOS_STABLE_CODE_IDENTITY
  constexpr bool stableCodeIdentity = true;
#else
  constexpr bool stableCodeIdentity = false;
#endif
  const bool hasBundleIdentifier = [[[NSBundle mainBundle] bundleIdentifier] length] > 0;
  return !deskflow::gui::macCanUseNotifications(hasBundleIdentifier, stableCodeIdentity);
}

bool showOSXNotification(const QString &title, const QString &body)
{
#if OSX_DEPLOYMENT_TARGET >= 1014
  // accessing notification center on unsigned build causes an immidiate
  // application shutodown (in this case, server) and cannot be caught
  // to avoid issues with it need to first check if this is a dev build
  if (isOSXDevelopmentBuild()) {
    qWarning("Not showing notification in dev build");
    return false;
  }

  requestOSXNotificationPermission();

  UNUserNotificationCenter *center = [UNUserNotificationCenter currentNotificationCenter];

  UNMutableNotificationContent *content = [[UNMutableNotificationContent alloc] init];
  content.title = title.toNSString();
  content.body = body.toNSString();

  // Create the request object.
  UNNotificationRequest *request = [UNNotificationRequest requestWithIdentifier:@"SecureInput"
                                                                        content:content
                                                                        trigger:nil];
  [content release];

  [center
      addNotificationRequest:request
       withCompletionHandler:^(NSError *_Nullable error) {
         if (error != nil) {
           qWarning("Notification display request error: %s", [[NSString stringWithFormat:@"%@", error] UTF8String]);
         }
       }];
#else
  NSUserNotification *notification = [[NSUserNotification alloc] init];
  notification.title = title.toNSString();
  notification.informativeText = body.toNSString();
  notification.soundName = NSUserNotificationDefaultSoundName; // Will play a default sound
  [[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:notification];
  [notification autorelease];
#endif
  return true;
}

bool isOSXInterfaceStyleDark()
{
  // Implementation from http://stackoverflow.com/a/26472651
  NSDictionary *dict = [[NSUserDefaults standardUserDefaults] persistentDomainForName:NSGlobalDomain];
  id style = [dict objectForKey:@"AppleInterfaceStyle"];
  return (style && [style isKindOfClass:[NSString class]] && NSOrderedSame == [style caseInsensitiveCompare:@"dark"]);
}

void forceAppActive()
{
  [[NSApplication sharedApplication] setActivationPolicy:NSApplicationActivationPolicyRegular];
  [[NSApplication sharedApplication] activateIgnoringOtherApps:YES];
}

namespace {

IeumApplicationReopenHandler *g_applicationReopenHandler = nil;
std::function<void(bool)> g_applicationTerminationHandler;
IMP g_originalApplicationShouldTerminate = nullptr;
Method g_applicationShouldTerminateMethod = nullptr;
id g_workspacePowerOffObserver = nil;
bool g_systemShutdown = false;
bool g_terminationCallbackSent = false;
id g_applicationLaunchObserver = nil;
bool g_applicationLaunched = false;
bool g_loginLaunch = false;
QPointer<QObject> g_launchReceiver;
std::function<void(bool)> g_launchHandler;

void deliverApplicationLaunch()
{
  if (g_applicationLaunched && g_launchHandler && g_launchReceiver) {
    auto handler = std::move(g_launchHandler);
    const bool loginLaunch = g_loginLaunch;
    QMetaObject::invokeMethod(
        g_launchReceiver.data(), [handler, loginLaunch] { handler(loginLaunch); }, Qt::QueuedConnection
    );
  }
}

void notifyApplicationTermination(bool systemShutdown)
{
  if (g_terminationCallbackSent) {
    return;
  }
  g_terminationCallbackSent = true;
  if (g_applicationTerminationHandler) {
    g_applicationTerminationHandler(systemShutdown);
  }
}

NSApplicationTerminateReply ieumApplicationShouldTerminate(id self, SEL selector, NSApplication *sender)
{
  notifyApplicationTermination(g_systemShutdown);

  if (g_originalApplicationShouldTerminate != nullptr) {
    using ApplicationShouldTerminate = NSApplicationTerminateReply (*)(id, SEL, NSApplication *);
    return reinterpret_cast<ApplicationShouldTerminate>(g_originalApplicationShouldTerminate)(self, selector, sender);
  }
  return NSTerminateNow;
}

deskflow::gui::MacLoginItemOperations loginItemOperations(SMAppService *service) API_AVAILABLE(macos(13.0))
{
  using deskflow::gui::MacLoginItemStatus;
  return {
      [service] {
        switch (service.status) {
        case SMAppServiceStatusEnabled:
          return MacLoginItemStatus::Enabled;
        case SMAppServiceStatusRequiresApproval:
          return MacLoginItemStatus::RequiresApproval;
        case SMAppServiceStatusNotRegistered:
          return MacLoginItemStatus::NotRegistered;
        default:
          return MacLoginItemStatus::NotFound;
        }
      },
      [service](bool enabled, QString *error) {
        NSError *nativeError = nil;
        const bool success =
            enabled ? [service registerAndReturnError:&nativeError] : [service unregisterAndReturnError:&nativeError];
        if (!success && error != nullptr) {
          *error = nativeError == nil ? QStringLiteral("Unknown ServiceManagement error")
                                      : QString::fromUtf8(nativeError.localizedDescription.UTF8String);
        }
        return success;
      }
  };
}

} // namespace

void macOSObserveApplicationLaunch()
{
  macOSStopObservingApplicationLaunch();
  g_applicationLaunched = false;
  g_loginLaunch = false;
  // This runs before Qt installs its Cocoa autorelease pools.
  @autoreleasepool {
    g_applicationLaunchObserver = [[[NSNotificationCenter defaultCenter]
        addObserverForName:NSApplicationDidFinishLaunchingNotification
                    object:nil
                     queue:nil
                usingBlock:^(NSNotification *) {
                  auto *event = NSAppleEventManager.sharedAppleEventManager.currentAppleEvent;
                  g_loginLaunch =
                      event.eventID == kAEOpenApplication &&
                      [event paramDescriptorForKeyword:keyAEPropData].enumCodeValue == keyAELaunchedAsLogInItem;
                  g_applicationLaunched = true;
                  deliverApplicationLaunch();
                }] retain];
  }
}

void macOSStopObservingApplicationLaunch()
{
  if (g_applicationLaunchObserver != nil) {
    [[NSNotificationCenter defaultCenter] removeObserver:g_applicationLaunchObserver];
    [g_applicationLaunchObserver release];
    g_applicationLaunchObserver = nil;
  }
  g_launchHandler = {};
  g_launchReceiver.clear();
}

void macOSWhenApplicationLaunched(QObject *receiver, std::function<void(bool)> handler)
{
  if (QGuiApplication::platformName() != QStringLiteral("cocoa")) {
    handler(false);
    return;
  }
  g_launchReceiver = receiver;
  g_launchHandler = std::move(handler);
  deliverApplicationLaunch();
}

bool macOSStartAtLoginSupported()
{
  if (@available(macOS 13.0, *)) {
    return true;
  }
  return false;
}

bool macOSStartAtLoginEnabled()
{
  if (@available(macOS 13.0, *)) {
    auto *service = SMAppService.mainAppService;
    return service.status == SMAppServiceStatusEnabled;
  }
  return false;
}

bool macOSStartAtLoginRequiresApproval()
{
  if (@available(macOS 13.0, *)) {
    auto *service = SMAppService.mainAppService;
    return service.status == SMAppServiceStatusRequiresApproval;
  }
  return false;
}

bool macOSSetStartAtLogin(bool enabled, QString *error)
{
  if (@available(macOS 13.0, *)) {
    // The GUI is an application login item, not a launchd helper. Registering
    // its executable as an agent leaves a launch constraint tied to the old
    // code identity after ad-hoc alpha updates (CODESIGNING, code 4).
    // Keep the old plist in the bundle solely to unregister existing jobs.
    auto *legacyAgent = [SMAppService agentServiceWithPlistName:@"io.github.victoriousian.ieum.login.plist"];
    return deskflow::gui::reconcileMacLoginItems(
        enabled, loginItemOperations(SMAppService.mainAppService), loginItemOperations(legacyAgent), error
    );
  }

  if (error != nullptr) {
    *error = QStringLiteral("Start at login requires macOS 13 or later.");
  }
  return false;
}

void macOSOpenLoginItemsSettings()
{
  if (@available(macOS 13.0, *)) {
    [SMAppService openSystemSettingsLoginItems];
  }
}

void macOSOpenAccessibilitySettings()
{
  auto *url = [NSURL URLWithString:@"x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility"];
  [[NSWorkspace sharedWorkspace] openURL:url];
}

void macOSRevealCurrentApplication()
{
  NSURL *bundleUrl = [NSBundle mainBundle].bundleURL;
  if (bundleUrl != nil) {
    [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[ bundleUrl ]];
  }
}

void macOSInstallApplicationReopenHandler(QObject *receiver)
{
  // Offscreen/minimal Qt platforms do not own an AppKit event dispatcher.
  // Registering native application handlers there can block headless tools.
  if (QGuiApplication::platformName() != QStringLiteral("cocoa")) {
    return;
  }
  macOSRemoveApplicationReopenHandler();
  g_applicationReopenHandler = [[IeumApplicationReopenHandler alloc] initWithReceiver:receiver];
  [[NSAppleEventManager sharedAppleEventManager] setEventHandler:g_applicationReopenHandler
                                                     andSelector:@selector(handleReopenApplication:withReplyEvent:)
                                                   forEventClass:kCoreEventClass
                                                      andEventID:kAEReopenApplication];
}

void macOSRemoveApplicationReopenHandler()
{
  if (g_applicationReopenHandler == nil) {
    return;
  }
  [[NSAppleEventManager sharedAppleEventManager] removeEventHandlerForEventClass:kCoreEventClass
                                                                      andEventID:kAEReopenApplication];
  [g_applicationReopenHandler release];
  g_applicationReopenHandler = nil;
}

void macOSInstallApplicationTerminationHandler(std::function<void(bool systemShutdown)> handler)
{
  if (QGuiApplication::platformName() != QStringLiteral("cocoa")) {
    return;
  }
  macOSRemoveApplicationTerminationHandler();
  g_applicationTerminationHandler = std::move(handler);
  g_systemShutdown = false;
  g_terminationCallbackSent = false;

  const auto delegateClass = [[NSApp delegate] class];
  const auto selector = @selector(applicationShouldTerminate:);
  g_applicationShouldTerminateMethod = class_getInstanceMethod(delegateClass, selector);
  if (g_applicationShouldTerminateMethod != nullptr) {
    g_originalApplicationShouldTerminate = method_getImplementation(g_applicationShouldTerminateMethod);
    method_setImplementation(g_applicationShouldTerminateMethod, reinterpret_cast<IMP>(ieumApplicationShouldTerminate));
  } else {
    class_addMethod(delegateClass, selector, reinterpret_cast<IMP>(ieumApplicationShouldTerminate), "q@:@");
  }

  auto *notificationCenter = [[NSWorkspace sharedWorkspace] notificationCenter];
  g_workspacePowerOffObserver = [notificationCenter addObserverForName:NSWorkspaceWillPowerOffNotification
                                                                object:nil
                                                                 queue:[NSOperationQueue mainQueue]
                                                            usingBlock:^(NSNotification *notification) {
                                                              (void)notification;
                                                              g_systemShutdown = true;
                                                              notifyApplicationTermination(true);
                                                            }];
}

void macOSRemoveApplicationTerminationHandler()
{
  if (g_workspacePowerOffObserver != nil) {
    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:g_workspacePowerOffObserver];
    g_workspacePowerOffObserver = nil;
  }
  if (g_applicationShouldTerminateMethod != nullptr && g_originalApplicationShouldTerminate != nullptr) {
    method_setImplementation(g_applicationShouldTerminateMethod, g_originalApplicationShouldTerminate);
  }

  g_applicationShouldTerminateMethod = nullptr;
  g_originalApplicationShouldTerminate = nullptr;
  g_applicationTerminationHandler = {};
  g_systemShutdown = false;
  g_terminationCallbackSent = false;
}
