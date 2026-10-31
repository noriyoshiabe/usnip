#pragma once

#include "Settings.h"
#include "platform/Pasteboard.h"

#include <QObject>
#include <QTimer>

#include <chrono>

#include <memory>
#include <sys/types.h>

namespace usnip {

class PopupWindow;
class SnippetStore;

// Runs one paste: remember the frontmost app, show the popup, put the chosen body on the
// pasteboard, return focus, send Cmd+V, and clear + restore the pasteboard after a delay.
class PasteController : public QObject {
    Q_OBJECT

public:
    explicit PasteController(SnippetStore &store, QObject *parent = nullptr);
    ~PasteController() override;

    // How long a pasted secret stays on the pasteboard before the previous contents return.
    void setClearDelay(std::chrono::milliseconds delay);
    void setPopupPosition(settings::PopupPosition position);

public slots:
    void showPopup();
    // Clears and restores the pasteboard now if a restore is pending. Called on quit.
    void flush();

private:
    void paste(int index);
    void returnToPreviousApp();

    SnippetStore &m_store;
    std::unique_ptr<PopupWindow> m_popup;
    platform::Pasteboard m_pasteboard;
    QTimer m_restoreTimer;
    pid_t m_previousApp = 0;
    settings::PopupPosition m_popupPosition = settings::kDefaultPopupPosition;
};

} // namespace usnip
