#include "EditorWindow.h"
#include "PasteController.h"
#include "core/SnippetStore.h"
#include "platform/Auth.h"
#include "platform/GlobalHotkey.h"
#include "platform/Keychain.h"
#include "platform/Paste.h"
#include "platform/StatusItem.h"

#include <QApplication>
#include <QMessageBox>
#include <QPainter>
#include <QPointer>
#include <QStandardPaths>

using namespace usnip;

namespace {

const QString kAppName = QStringLiteral("μSnip");

QString storePath()
{
    // ~/Library/Application Support/uSnip/snippets.bin
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
           QStringLiteral("/snippets.bin");
}

// Drawn at 2x for a 22 pt menu bar icon. Only the alpha matters: it is used as a template.
QImage statusIcon()
{
    QImage image(44, 44, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    QFont font = painter.font();
    font.setPixelSize(36);
    font.setBold(true);
    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(image.rect(), Qt::AlignCenter, QStringLiteral("μ"));
    painter.end();
    return image;
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

    platform::StatusItem statusItem(statusIcon(), kAppName);
    QPointer<EditorWindow> editor;
    bool authenticating = false;
    statusItem.addAction(QObject::tr("Edit…"), [&] {
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
    statusItem.addSeparator();
    statusItem.addAction(QObject::tr("Quit %1").arg(kAppName), &QCoreApplication::quit);

    // Shift+Command+B, the snippet shortcut of Clipy, which μSnip replaces. Fixed until the
    // shortcut becomes configurable (roadmap phase 4).
    if (!hotkey.setShortcut(platform::keys::kB,
                            platform::keys::kShift | platform::keys::kCommand)) {
        QMessageBox::warning(nullptr, kAppName,
                             QObject::tr("Could not register the shortcut ⇧⌘B."));
    }

    return app.exec();
}
