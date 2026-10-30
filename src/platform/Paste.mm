#include "Paste.h"

#import <AppKit/AppKit.h>
#import <ApplicationServices/ApplicationServices.h>
#import <Carbon/Carbon.h>

namespace usnip::platform {

pid_t frontmostApplicationPid()
{
    NSRunningApplication *app = NSWorkspace.sharedWorkspace.frontmostApplication;
    return app ? app.processIdentifier : 0;
}

void activateApplication(pid_t pid)
{
    if (pid == 0) {
        return;
    }
    NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];
    [app activateWithOptions:(NSApplicationActivationOptions)0];
}

void activateSelf()
{
    if (@available(macOS 14.0, *)) {
        [NSApp activate];
    }
    else {
        [NSApp activateIgnoringOtherApps:YES];
    }
}

bool isAccessibilityTrusted(bool prompt)
{
    NSDictionary *options = @{(__bridge id)kAXTrustedCheckOptionPrompt : @(prompt)};
    return AXIsProcessTrustedWithOptions((__bridge CFDictionaryRef)options);
}

void postCommandV()
{
    CGEventSourceRef source = CGEventSourceCreate(kCGEventSourceStateCombinedSessionState);
    CGEventRef down = CGEventCreateKeyboardEvent(source, kVK_ANSI_V, true);
    CGEventRef up = CGEventCreateKeyboardEvent(source, kVK_ANSI_V, false);
    CGEventSetFlags(down, kCGEventFlagMaskCommand);
    CGEventSetFlags(up, kCGEventFlagMaskCommand);
    CGEventPost(kCGAnnotatedSessionEventTap, down);
    CGEventPost(kCGAnnotatedSessionEventTap, up);
    CFRelease(down);
    CFRelease(up);
    if (source) {
        CFRelease(source);
    }
}

} // namespace usnip::platform
