#include "PasteController.h"

#include "PopupWindow.h"
#include "core/SnippetStore.h"
#include "platform/Paste.h"

#include <QCursor>

#include <chrono>
#include <unistd.h>

namespace usnip {

namespace {

using namespace std::chrono_literals;

constexpr auto kRestoreDelay = 5s;
// Time for the previous app to take focus back before Cmd+V. Tuned in roadmap phase 1.
constexpr auto kPasteDelay = 50ms;

} // namespace

PasteController::PasteController(SnippetStore &store, QObject *parent)
    : QObject(parent), m_store(store), m_popup(std::make_unique<PopupWindow>())
{
    m_restoreTimer.setSingleShot(true);
    m_restoreTimer.setInterval(kRestoreDelay);
    connect(&m_restoreTimer, &QTimer::timeout, this, [this] { m_pasteboard.restoreIfUnchanged(); });
    connect(m_popup.get(), &PopupWindow::chosen, this, &PasteController::paste);
    connect(m_popup.get(), &PopupWindow::dismissed, this, &PasteController::returnToPreviousApp);
}

PasteController::~PasteController() = default;

void PasteController::showPopup()
{
    // Ask macOS who is in front rather than trusting isVisible() alone: after another app takes
    // over, Qt may still report the popup as visible, and the shortcut must not go dead.
    const pid_t frontmost = platform::frontmostApplicationPid();
    if (m_popup->isVisible() && frontmost == getpid()) {
        return;
    }
    m_previousApp = frontmost;

    QStringList titles;
    for (const Snippet &snippet : m_store.snippets()) {
        titles.append(snippet.title);
    }
    m_popup->setTitles(titles);
    platform::activateSelf();
    m_popup->showNear(QCursor::pos());
}

void PasteController::flush()
{
    m_restoreTimer.stop();
    m_pasteboard.restoreIfUnchanged();
}

void PasteController::paste(int index)
{
    auto body = m_store.body(static_cast<std::size_t>(index));
    if (!body) {
        returnToPreviousApp();
        return;
    }
    m_pasteboard.saveCurrent();
    const bool written = m_pasteboard.writeConcealed(*body);
    body->clear();
    returnToPreviousApp();
    if (!written) {
        return;
    }

    m_restoreTimer.start();
    if (platform::isAccessibilityTrusted(false)) {
        QTimer::singleShot(kPasteDelay, this, [] { platform::postCommandV(); });
    }
}

void PasteController::returnToPreviousApp()
{
    platform::activateApplication(m_previousApp);
    m_previousApp = 0;
}

} // namespace usnip
