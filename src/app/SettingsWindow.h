#pragma once

#include "platform/GlobalHotkey.h"

#include <QWidget>

class QComboBox;
class QSpinBox;

namespace usnip {

class PasteController;
class ShortcutEdit;

// The shortcut, the clear delay and where the popup appears. Changes apply and are stored at once
// (QSettings).
class SettingsWindow : public QWidget {
    Q_OBJECT

public:
    SettingsWindow(platform::GlobalHotkey &hotkey, PasteController &controller,
                   QWidget *parent = nullptr);

private:
    void applyShortcut(const platform::Shortcut &shortcut);

    platform::GlobalHotkey &m_hotkey;
    PasteController &m_controller;
    ShortcutEdit *m_shortcut;
    QSpinBox *m_clearDelay;
    QComboBox *m_popupPosition;
};

} // namespace usnip
