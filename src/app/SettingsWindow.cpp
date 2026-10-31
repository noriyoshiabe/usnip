#include "SettingsWindow.h"

#include "PasteController.h"
#include "Settings.h"
#include "ShortcutEdit.h"

#include <QComboBox>
#include <QFormLayout>
#include <QLabel>
#include <QMessageBox>
#include <QShortcut>
#include <QSpinBox>

namespace usnip {

SettingsWindow::SettingsWindow(platform::GlobalHotkey &hotkey, PasteController &controller,
                               QWidget *parent)
    : QWidget(parent, Qt::Window), m_hotkey(hotkey), m_controller(controller),
      m_shortcut(new ShortcutEdit(this)), m_clearDelay(new QSpinBox(this)),
      m_popupPosition(new QComboBox(this))
{
    setAttribute(Qt::WA_DeleteOnClose);
    setWindowTitle(tr("Settings"));

    m_shortcut->setShortcut(settings::shortcut());
    m_clearDelay->setRange(static_cast<int>(settings::kMinClearDelay.count()),
                           static_cast<int>(settings::kMaxClearDelay.count()));
    m_clearDelay->setSuffix(tr(" s"));
    m_clearDelay->setValue(static_cast<int>(settings::clearDelay().count()));
    m_popupPosition->addItem(tr("Near the cursor"),
                             static_cast<int>(settings::PopupPosition::NearCursor));
    m_popupPosition->addItem(tr("Center of the screen"),
                             static_cast<int>(settings::PopupPosition::ScreenCenter));
    m_popupPosition->setCurrentIndex(
        m_popupPosition->findData(static_cast<int>(settings::popupPosition())));

    // RegisterEventHotKey accepts a shortcut another app holds (roadmap phase 1), so a clash
    // cannot be detected and is not warned about.
    auto *shortcutNote = new QLabel(tr("Click and type a new shortcut."), this);
    auto *clearNote = new QLabel(tr("Then the previous clipboard contents come back."), this);
    for (QLabel *note : {shortcutNote, clearNote}) {
        note->setEnabled(false);
    }

    auto *layout = new QFormLayout(this);
    layout->addRow(tr("Shortcut:"), m_shortcut);
    layout->addRow(QString(), shortcutNote);
    layout->addRow(tr("Clear the copy after:"), m_clearDelay);
    layout->addRow(QString(), clearNote);
    layout->addRow(tr("Show the popup:"), m_popupPosition);

    connect(m_shortcut, &ShortcutEdit::recordingStarted, this, [this] { m_hotkey.clear(); });
    connect(m_shortcut, &ShortcutEdit::recordingFinished, this,
            [this] { m_hotkey.setShortcut(settings::shortcut()); });
    connect(m_shortcut, &ShortcutEdit::shortcutRecorded, this, &SettingsWindow::applyShortcut);
    connect(m_clearDelay, &QSpinBox::valueChanged, this, [this](int seconds) {
        const std::chrono::seconds delay{seconds};
        settings::setClearDelay(delay);
        m_controller.setClearDelay(delay);
    });
    connect(m_popupPosition, &QComboBox::currentIndexChanged, this, [this] {
        const auto position =
            static_cast<settings::PopupPosition>(m_popupPosition->currentData().toInt());
        settings::setPopupPosition(position);
        m_controller.setPopupPosition(position);
    });
    new QShortcut(QKeySequence::Close, this, this, &QWidget::close);
}

void SettingsWindow::applyShortcut(const platform::Shortcut &shortcut)
{
    if (!m_hotkey.setShortcut(shortcut)) {
        const platform::Shortcut previous = settings::shortcut();
        m_hotkey.setShortcut(previous);
        m_shortcut->setShortcut(previous);
        QMessageBox::warning(
            this, windowTitle(),
            tr("Could not register the shortcut %1.").arg(platform::shortcutText(shortcut)));
        return;
    }
    settings::setShortcut(shortcut);
}

} // namespace usnip
