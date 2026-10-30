#include "EditorWindow.h"
#include "PasteController.h"
#include "core/SnippetStore.h"
#include "platform/Auth.h"
#include "platform/GlobalHotkey.h"
#include "platform/Keychain.h"
#include "platform/Paste.h"

#include <QApplication>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <QStandardPaths>
#include <QSystemTrayIcon>

using namespace usnip;

namespace {

const QString kAppName = QStringLiteral("μSnip");

QString storePath()
{
    // ~/Library/Application Support/uSnip/snippets.bin
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
           QStringLiteral("/snippets.bin");
}

// A template image, so macOS tints it for light and dark menu bars.
QIcon trayIcon()
{
    QPixmap pixmap(44, 44);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    QFont font = painter.font();
    font.setPixelSize(36);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(pixmap.rect(), Qt::AlignCenter, QStringLiteral("μ"));
    painter.end();
    QIcon icon(pixmap);
    icon.setIsMask(true);
    return icon;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("uSnip"));
    QApplication::setApplicationDisplayName(kAppName);
    QApplication::setQuitOnLastWindowClosed(false);

    auto master = platform::loadOrCreateMasterKey();
    if (!master) {
        QMessageBox::critical(nullptr, kAppName,
                              QObject::tr("Could not read the encryption key from the Keychain."));
        return 1;
    }
    SnippetStore store(crypto::deriveKeys(*master));
    master->clear();
    if (!store.loadFile(storePath())) {
        QMessageBox::critical(nullptr, kAppName,
                              QObject::tr("Could not decrypt %1.").arg(storePath()));
        return 1;
    }

    // Shows the macOS permission dialog on first launch. Without it, uSnip only copies.
    platform::isAccessibilityTrusted(true);

    PasteController controller(store);
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &controller, &PasteController::flush);

    platform::GlobalHotkey hotkey;
    QObject::connect(&hotkey, &platform::GlobalHotkey::activated, &controller,
                     &PasteController::showPopup);

    QSystemTrayIcon tray(trayIcon());
    QMenu menu;
    QPointer<EditorWindow> editor;
    bool authenticating = false;
    menu.addAction(QObject::tr("Edit…"), [&] {
        if (editor) {
            editor->raise();
            editor->activateWindow();
            platform::activateSelf();
            return;
        }
        if (authenticating) {
            return;
        }
        authenticating = true;
        platform::authenticateOwner(QObject::tr("open the snippet editor"), [&](bool success) {
            authenticating = false;
            if (!success) {
                return;
            }
            editor = new EditorWindow(store, storePath());
            editor->show();
            editor->raise();
            editor->activateWindow();
            platform::activateSelf();
        });
    });
    menu.addSeparator();
    menu.addAction(QObject::tr("Quit %1").arg(kAppName), &app, &QCoreApplication::quit);
    tray.setContextMenu(&menu);
    tray.setToolTip(kAppName);
    tray.show();

    // Shift+Command+B, the snippet shortcut of Clipy, which μSnip replaces. Fixed until the
    // shortcut becomes configurable (roadmap phase 4).
    if (!hotkey.setShortcut(platform::keys::kB,
                            platform::keys::kShift | platform::keys::kCommand)) {
        tray.showMessage(kAppName, QObject::tr("Could not register the shortcut ⇧⌘B."));
    }

    return app.exec();
}
