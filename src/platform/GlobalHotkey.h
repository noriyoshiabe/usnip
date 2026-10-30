#pragma once

#include <QObject>
#include <QString>

#include <cstdint>
#include <memory>

namespace usnip::platform {

// Carbon virtual key codes and modifier flags (HIToolbox/Events.h), so callers need no Carbon.
namespace keys {
constexpr std::uint32_t kB = 0x0B;
constexpr std::uint32_t kV = 0x09;
constexpr std::uint32_t kCommand = 0x0100;
constexpr std::uint32_t kShift = 0x0200;
constexpr std::uint32_t kOption = 0x0800;
constexpr std::uint32_t kControl = 0x1000;
} // namespace keys

struct Shortcut {
    // A virtual key code: the physical key, independent of the keyboard layout.
    std::uint32_t keyCode = 0;
    std::uint32_t modifiers = 0;

    bool operator==(const Shortcut &) const = default;
};

// For display, such as "⇧⌘B". The key is named after the current keyboard layout.
QString shortcutText(const Shortcut &shortcut);

// A system-wide shortcut via Carbon RegisterEventHotKey. Needs no permission.
class GlobalHotkey : public QObject {
    Q_OBJECT

public:
    explicit GlobalHotkey(QObject *parent = nullptr);
    ~GlobalHotkey() override;

    // Replaces the current shortcut. False if macOS refuses it. A shortcut another app has
    // registered is not refused: both apps then receive it.
    bool setShortcut(const Shortcut &shortcut);
    // Unregisters the shortcut, so that it reaches the focused window instead.
    void clear();

signals:
    void activated();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace usnip::platform
