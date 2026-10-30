#include "ShortcutEdit.h"

#include "Settings.h"

#include <QKeyEvent>

namespace usnip {

namespace {

// Qt on macOS maps Command to ControlModifier and Control to MetaModifier.
std::uint32_t carbonModifiers(Qt::KeyboardModifiers modifiers)
{
    using namespace platform::keys;
    std::uint32_t result = 0;
    if (modifiers & Qt::ControlModifier) {
        result |= kCommand;
    }
    if (modifiers & Qt::ShiftModifier) {
        result |= kShift;
    }
    if (modifiers & Qt::AltModifier) {
        result |= kOption;
    }
    if (modifiers & Qt::MetaModifier) {
        result |= kControl;
    }
    return result;
}

bool isModifierKey(int key)
{
    switch (key) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Meta:
    case Qt::Key_Alt:
    case Qt::Key_CapsLock:
        return true;
    default:
        return false;
    }
}

} // namespace

ShortcutEdit::ShortcutEdit(QWidget *parent) : QLineEdit(parent)
{
    setReadOnly(true);
    // Recording starts on focus, so it must not start just because the window opened.
    setFocusPolicy(Qt::ClickFocus);
    setAlignment(Qt::AlignCenter);
    setPlaceholderText(tr("Type a shortcut"));
}

void ShortcutEdit::setShortcut(const platform::Shortcut &shortcut)
{
    m_shortcut = shortcut;
    if (!hasFocus()) {
        setText(platform::shortcutText(shortcut));
    }
}

bool ShortcutEdit::event(QEvent *event)
{
    // Claim every key while recording, so that window shortcuts such as Cmd+W do not fire.
    if (event->type() == QEvent::ShortcutOverride) {
        event->accept();
        return true;
    }
    return QLineEdit::event(event);
}

void ShortcutEdit::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape && event->modifiers() == Qt::NoModifier) {
        clearFocus();
        return;
    }
    if (isModifierKey(event->key())) {
        return;
    }
    const platform::Shortcut shortcut = {event->nativeVirtualKey(),
                                         carbonModifiers(event->modifiers())};
    if (!settings::isValidShortcut(shortcut)) {
        return;
    }
    m_shortcut = shortcut;
    clearFocus();
    emit shortcutRecorded(shortcut);
}

void ShortcutEdit::focusInEvent(QFocusEvent *event)
{
    QLineEdit::focusInEvent(event);
    clear();
    emit recordingStarted();
}

void ShortcutEdit::focusOutEvent(QFocusEvent *event)
{
    QLineEdit::focusOutEvent(event);
    setText(platform::shortcutText(m_shortcut));
    emit recordingFinished();
    // Switching to another app keeps this the focus widget, so coming back would resume
    // recording with the global shortcut unregistered, and closing the window then would
    // leave it unregistered.
    if (event->reason() == Qt::ActiveWindowFocusReason) {
        clearFocus();
    }
}

} // namespace usnip
