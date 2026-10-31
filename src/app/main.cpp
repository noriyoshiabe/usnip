#include "EditorWindow.h"
#include "PasteController.h"
#include "Settings.h"
#include "SettingsWindow.h"
#include "core/SnippetStore.h"
#include "platform/Auth.h"
#include "platform/GlobalHotkey.h"
#include "platform/Keychain.h"
#include "platform/Paste.h"
#include "platform/StatusItem.h"

#include <QApplication>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QStandardPaths>

#include <algorithm>

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
// Centred on the ink rather than with drawText(): font metrics include the line height and side
// bearings, and μ hangs below the baseline, so text centred on its metrics looked off centre.
// The ink is then lowered a little: most of it sits above the baseline, with only the thin tail
// of μ below, so the exact ink centre still looked high next to the system icons.
QImage statusIcon()
{
    constexpr int kSize = 44;
    constexpr qreal kInkHeight = 23;
    constexpr qreal kMaxInkWidth = 40;
    constexpr qreal kDrop = 2;

    QFont font;
    font.setPixelSize(100);
    font.setBold(true);
    QPainterPath path;
    path.addText(0, 0, font, QStringLiteral("μS"));
    const QRectF ink = path.boundingRect();
    const qreal scale = std::min(kInkHeight / ink.height(), kMaxInkWidth / ink.width());

    QImage image(kSize, kSize, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.translate(kSize / 2.0, kSize / 2.0 + kDrop);
    painter.scale(scale, scale);
    painter.translate(-ink.center());
    painter.fillPath(path, Qt::black);
    painter.end();
    return image;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("uSnip"));
    // QSettings reverses it into io.github.noriyoshiabe.uSnip. AppDataLocation uses the
    // organization name instead, which stays empty, so the data file does not move.
    QApplication::setOrganizationDomain(QStringLiteral("noriyoshiabe.github.io"));
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
    controller.setClearDelay(settings::clearDelay());
    controller.setPopupPosition(settings::popupPosition());
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
    QPointer<SettingsWindow> settingsWindow;
    statusItem.addAction(QObject::tr("Settings…"), [&] {
        if (!settingsWindow) {
            settingsWindow = new SettingsWindow(hotkey, controller);
        }
        settingsWindow->show();
        settingsWindow->raise();
        settingsWindow->activateWindow();
        platform::activateSelf();
    });
    statusItem.addSeparator();
    statusItem.addAction(QObject::tr("Quit %1").arg(kAppName), &QCoreApplication::quit);

    const platform::Shortcut shortcut = settings::shortcut();
    if (!hotkey.setShortcut(shortcut)) {
        QMessageBox::warning(nullptr, kAppName,
                             QObject::tr("Could not register the shortcut %1. Choose another one "
                                         "in Settings…")
                                 .arg(platform::shortcutText(shortcut)));
    }

    return app.exec();
}
