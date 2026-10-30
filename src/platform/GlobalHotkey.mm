#include "GlobalHotkey.h"

#import <Carbon/Carbon.h>

namespace usnip::platform {

namespace {

constexpr OSType kSignature = 0x75536e70; // 'uSnp'

OSStatus onHotKeyPressed(EventHandlerCallRef, EventRef, void *userData)
{
    auto *hotkey = static_cast<GlobalHotkey *>(userData);
    QMetaObject::invokeMethod(hotkey, [hotkey] { emit hotkey->activated(); }, Qt::QueuedConnection);
    return noErr;
}

// Keys whose character from the layout is a control code or invisible.
QString specialKeyName(std::uint32_t keyCode)
{
    switch (keyCode) {
    case kVK_Return:
        return QStringLiteral("↩");
    case kVK_Tab:
        return QStringLiteral("⇥");
    case kVK_Space:
        return QStringLiteral("Space");
    case kVK_Delete:
        return QStringLiteral("⌫");
    case kVK_ForwardDelete:
        return QStringLiteral("⌦");
    case kVK_Escape:
        return QStringLiteral("⎋");
    case kVK_Home:
        return QStringLiteral("↖");
    case kVK_End:
        return QStringLiteral("↘");
    case kVK_PageUp:
        return QStringLiteral("⇞");
    case kVK_PageDown:
        return QStringLiteral("⇟");
    case kVK_LeftArrow:
        return QStringLiteral("←");
    case kVK_RightArrow:
        return QStringLiteral("→");
    case kVK_DownArrow:
        return QStringLiteral("↓");
    case kVK_UpArrow:
        return QStringLiteral("↑");
    case kVK_F1:
        return QStringLiteral("F1");
    case kVK_F2:
        return QStringLiteral("F2");
    case kVK_F3:
        return QStringLiteral("F3");
    case kVK_F4:
        return QStringLiteral("F4");
    case kVK_F5:
        return QStringLiteral("F5");
    case kVK_F6:
        return QStringLiteral("F6");
    case kVK_F7:
        return QStringLiteral("F7");
    case kVK_F8:
        return QStringLiteral("F8");
    case kVK_F9:
        return QStringLiteral("F9");
    case kVK_F10:
        return QStringLiteral("F10");
    case kVK_F11:
        return QStringLiteral("F11");
    case kVK_F12:
        return QStringLiteral("F12");
    default:
        return QString();
    }
}

// The character the key types without modifiers in the current layout, upper-cased like the
// menu shortcuts of macOS.
QString layoutKeyName(std::uint32_t keyCode)
{
    TISInputSourceRef source = TISCopyCurrentKeyboardLayoutInputSource();
    if (!source) {
        return QString();
    }
    const auto layoutData =
        static_cast<CFDataRef>(TISGetInputSourceProperty(source, kTISPropertyUnicodeKeyLayoutData));
    QString name;
    if (layoutData) {
        const auto *layout =
            reinterpret_cast<const UCKeyboardLayout *>(CFDataGetBytePtr(layoutData));
        UInt32 deadKeyState = 0;
        UniChar chars[4];
        UniCharCount length = 0;
        if (UCKeyTranslate(layout, static_cast<UInt16>(keyCode), kUCKeyActionDisplay, 0,
                           LMGetKbdType(), kUCKeyTranslateNoDeadKeysBit, &deadKeyState, 4, &length,
                           chars) == noErr) {
            name = QString(reinterpret_cast<const QChar *>(chars), static_cast<qsizetype>(length))
                       .toUpper();
        }
    }
    CFRelease(source);
    return name;
}

} // namespace

QString shortcutText(const Shortcut &shortcut)
{
    // The order macOS menus use.
    QString text;
    if (shortcut.modifiers & keys::kControl) {
        text += QStringLiteral("⌃");
    }
    if (shortcut.modifiers & keys::kOption) {
        text += QStringLiteral("⌥");
    }
    if (shortcut.modifiers & keys::kShift) {
        text += QStringLiteral("⇧");
    }
    if (shortcut.modifiers & keys::kCommand) {
        text += QStringLiteral("⌘");
    }
    QString key = specialKeyName(shortcut.keyCode);
    if (key.isEmpty()) {
        key = layoutKeyName(shortcut.keyCode);
    }
    if (key.trimmed().isEmpty()) {
        key = QStringLiteral("#%1").arg(shortcut.keyCode);
    }
    return text + key;
}

struct GlobalHotkey::Impl {
    EventHandlerRef handler = nullptr;
    EventHotKeyRef hotKey = nullptr;
};

GlobalHotkey::GlobalHotkey(QObject *parent) : QObject(parent), d(std::make_unique<Impl>())
{
    const EventTypeSpec spec = {kEventClassKeyboard, kEventHotKeyPressed};
    InstallEventHandler(GetApplicationEventTarget(), &onHotKeyPressed, 1, &spec, this, &d->handler);
}

GlobalHotkey::~GlobalHotkey()
{
    clear();
    if (d->handler) {
        RemoveEventHandler(d->handler);
    }
}

bool GlobalHotkey::setShortcut(const Shortcut &shortcut)
{
    clear();
    const EventHotKeyID id = {kSignature, 1};
    return RegisterEventHotKey(shortcut.keyCode, shortcut.modifiers, id,
                               GetApplicationEventTarget(), 0, &d->hotKey) == noErr;
}

void GlobalHotkey::clear()
{
    if (d->hotKey) {
        UnregisterEventHotKey(d->hotKey);
        d->hotKey = nullptr;
    }
}

} // namespace usnip::platform
