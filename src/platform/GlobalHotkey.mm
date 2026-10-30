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

} // namespace

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
    if (d->hotKey) {
        UnregisterEventHotKey(d->hotKey);
    }
    if (d->handler) {
        RemoveEventHandler(d->handler);
    }
}

bool GlobalHotkey::setShortcut(std::uint32_t keyCode, std::uint32_t modifiers)
{
    if (d->hotKey) {
        UnregisterEventHotKey(d->hotKey);
        d->hotKey = nullptr;
    }
    const EventHotKeyID id = {kSignature, 1};
    return RegisterEventHotKey(keyCode, modifiers, id, GetApplicationEventTarget(), 0,
                               &d->hotKey) == noErr;
}

} // namespace usnip::platform
