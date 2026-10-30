#pragma once

#include "platform/GlobalHotkey.h"

#include <chrono>

// User settings in QSettings (~/Library/Preferences/io.github.noriyoshiabe.uSnip.plist).
// Values that fail validation fall back to the defaults.
namespace usnip::settings {

// ⇧⌘B, the snippet shortcut of Clipy, which μSnip replaces.
constexpr platform::Shortcut kDefaultShortcut = {platform::keys::kB,
                                                 platform::keys::kShift | platform::keys::kCommand};
constexpr std::chrono::seconds kDefaultClearDelay{5};
constexpr std::chrono::seconds kMinClearDelay{1};
constexpr std::chrono::seconds kMaxClearDelay{60};

// A global shortcut needs Command, Control or Option; Shift alone would take a typed character.
bool isValidShortcut(const platform::Shortcut &shortcut);

platform::Shortcut shortcut();
void setShortcut(const platform::Shortcut &shortcut);

std::chrono::seconds clearDelay();
void setClearDelay(std::chrono::seconds delay);

} // namespace usnip::settings
