#pragma once

#include <sys/types.h>

// Focus handling and the synthetic Cmd+V.
namespace usnip::platform {

// 0 when there is no frontmost application.
pid_t frontmostApplicationPid();
void activateApplication(pid_t pid);
void activateSelf();

// With prompt = true, macOS shows its Accessibility dialog if the app is not trusted yet.
bool isAccessibilityTrusted(bool prompt);

// Needs Accessibility permission; silently does nothing without it.
void postCommandV();

} // namespace usnip::platform
