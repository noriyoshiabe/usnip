#include "PasteController.h"
#include "core/JsonImport.h"
#include "core/SnippetStore.h"
#include "platform/GlobalHotkey.h"
#include "platform/Keychain.h"
#include "platform/Paste.h"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
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

// Temporary entry point until the editor exists (roadmap phase 3).
void importJson(SnippetStore &store)
{
    const QString path = QFileDialog::getOpenFileName(nullptr, QObject::tr("Import JSON"),
                                                      QString(), QObject::tr("JSON (*.json)"));
    if (path.isEmpty()) {
        return;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(nullptr, kAppName, QObject::tr("Could not open %1.").arg(path));
        return;
    }
    QString error;
    const auto snippets = jsonimport::parse(file.readAll(), &error);
    file.close();
    if (!snippets) {
        QMessageBox::warning(nullptr, kAppName, QObject::tr("Invalid JSON: %1").arg(error));
        return;
    }
    const auto answer =
        QMessageBox::question(nullptr, kAppName,
                              QObject::tr("Add %n snippet(s) after the existing ones?", nullptr,
                                          static_cast<int>(snippets->size())));
    if (answer != QMessageBox::Yes) {
        return;
    }
    for (const PlainSnippet &snippet : *snippets) {
        store.add(snippet.title, snippet.body);
    }
    if (!store.saveFile(storePath())) {
        QMessageBox::critical(nullptr, kAppName, QObject::tr("Could not save the snippets."));
        return;
    }
    const auto trash = QMessageBox::question(
        nullptr, kAppName, QObject::tr("The JSON file is not encrypted. Move it to the Trash?"));
    if (trash == QMessageBox::Yes) {
        QFile::moveToTrash(path);
    }
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
    menu.addAction(QObject::tr("Import JSON…"), [&store] { importJson(store); });
    menu.addSeparator();
    menu.addAction(QObject::tr("Quit %1").arg(kAppName), &app, &QCoreApplication::quit);
    tray.setContextMenu(&menu);
    tray.setToolTip(kAppName);
    tray.show();

    // Option+Command+V until the shortcut becomes configurable (roadmap phase 4).
    if (!hotkey.setShortcut(platform::keys::kV,
                            platform::keys::kOption | platform::keys::kCommand)) {
        tray.showMessage(kAppName, QObject::tr("The shortcut ⌥⌘V is already in use."));
    }

    return app.exec();
}
