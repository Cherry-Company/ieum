/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2015 Synergy App Ltd
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#import "OSXHelpers.h"

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
  std::string bundleURL = [[[NSBundle mainBundle] bundleURL].absoluteString UTF8String];
  return (bundleURL.find("Applications/Ieum.app") == std::string::npos);
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

NSApplicationTerminateReply
ieumApplicationShouldTerminate(id self, SEL selector, NSApplication *sender)
{
  notifyApplicationTermination(g_systemShutdown);

  if (g_originalApplicationShouldTerminate != nullptr) {
    using ApplicationShouldTerminate = NSApplicationTerminateReply (*)(id, SEL, NSApplication *);
    return reinterpret_cast<ApplicationShouldTerminate>(g_originalApplicationShouldTerminate)(self, selector, sender);
  }
  return NSTerminateNow;
}

SMAppService *ieumLoginAgent() API_AVAILABLE(macos(13.0))
{
  return [SMAppService agentServiceWithPlistName:@"io.github.victoriousian.ieum.login.plist"];
}

} // namespace

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
    auto *service = ieumLoginAgent();
    return service.status == SMAppServiceStatusEnabled;
  }
  return false;
}

bool macOSStartAtLoginRequiresApproval()
{
  if (@available(macOS 13.0, *)) {
    auto *service = ieumLoginAgent();
    return service.status == SMAppServiceStatusRequiresApproval;
  }
  return false;
}

bool macOSSetStartAtLogin(bool enabled, QString *error)
{
  if (@available(macOS 13.0, *)) {
    auto *service = ieumLoginAgent();
    const auto status = service.status;
    if ((enabled && (status == SMAppServiceStatusEnabled || status == SMAppServiceStatusRequiresApproval)) ||
        (!enabled && status == SMAppServiceStatusNotRegistered)) {
      return true;
    }

    NSError *nativeError = nil;
    const auto success =
        enabled ? [service registerAndReturnError:&nativeError] : [service unregisterAndReturnError:&nativeError];
    if (success) {
      return true;
    }
    if (error != nullptr) {
      *error = nativeError == nil ? QStringLiteral("Unknown ServiceManagement error")
                                  : QString::fromUtf8(nativeError.localizedDescription.UTF8String);
    }
    return false;
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
  g_workspacePowerOffObserver = [notificationCenter
      addObserverForName:NSWorkspaceWillPowerOffNotification
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
