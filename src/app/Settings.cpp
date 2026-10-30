#include "Settings.h"

#include <QSettings>

namespace usnip::settings {

namespace {

const QString kKeyCodeKey = QStringLiteral("shortcut/keyCode");
const QString kModifiersKey = QStringLiteral("shortcut/modifiers");
const QString kClearDelayKey = QStringLiteral("clearDelaySeconds");

} // namespace

bool isValidShortcut(const platform::Shortcut &shortcut)
{
    using namespace platform::keys;
    return shortcut.keyCode <= 0x7f && (shortcut.modifiers & (kCommand | kControl | kOption)) &&
           (shortcut.modifiers & ~(kCommand | kShift | kOption | kControl)) == 0;
}

platform::Shortcut shortcut()
{
    const QSettings settings;
    if (!settings.contains(kKeyCodeKey)) {
        return kDefaultShortcut;
    }
    const platform::Shortcut stored = {settings.value(kKeyCodeKey).toUInt(),
                                       settings.value(kModifiersKey).toUInt()};
    return isValidShortcut(stored) ? stored : kDefaultShortcut;
}

void setShortcut(const platform::Shortcut &shortcut)
{
    QSettings settings;
    settings.setValue(kKeyCodeKey, shortcut.keyCode);
    settings.setValue(kModifiersKey, shortcut.modifiers);
}

std::chrono::seconds clearDelay()
{
    const QSettings settings;
    bool ok = false;
    const std::chrono::seconds delay{settings.value(kClearDelayKey).toInt(&ok)};
    if (!ok || delay < kMinClearDelay || delay > kMaxClearDelay) {
        return kDefaultClearDelay;
    }
    return delay;
}

void setClearDelay(std::chrono::seconds delay)
{
    QSettings settings;
    settings.setValue(kClearDelayKey, static_cast<int>(delay.count()));
}

} // namespace usnip::settings
