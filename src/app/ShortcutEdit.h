#pragma once

#include "platform/GlobalHotkey.h"

#include <QLineEdit>

namespace usnip {

// Shows a shortcut and records a new one: click the field, then type the combination.
// Esc or clicking elsewhere keeps the old one.
//
// The key is recorded as its virtual key code (QKeyEvent::nativeVirtualKey), which is what
// RegisterEventHotKey takes, rather than as a Qt key, which depends on the layout and on Shift.
class ShortcutEdit : public QLineEdit {
    Q_OBJECT

public:
    explicit ShortcutEdit(QWidget *parent = nullptr);

    void setShortcut(const platform::Shortcut &shortcut);

signals:
    // The global shortcut has to be unregistered while recording, or typing it would open the
    // popup instead of reaching this field.
    void recordingStarted();
    void recordingFinished();
    // Emitted after recordingFinished.
    void shortcutRecorded(const platform::Shortcut &shortcut);

protected:
    bool event(QEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void focusInEvent(QFocusEvent *event) override;
    void focusOutEvent(QFocusEvent *event) override;

private:
    platform::Shortcut m_shortcut;
};

} // namespace usnip
