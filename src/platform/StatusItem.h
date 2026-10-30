#pragma once

#include <QImage>
#include <QString>

#include <functional>
#include <memory>

namespace usnip::platform {

// The menu bar icon and its menu, on NSStatusItem.
//
// Not QSystemTrayIcon: Qt 6.11's status item delegate asks NSApp.currentEvent for its
// clickCount without checking that it is a mouse event. On macOS 27 the menu opens from a scene
// action (NSSceneStatusItem) rather than from the click, so the current event is whatever the app
// received last. After typing in the editor that is a key event, and AppKit aborts the app.
class StatusItem {
public:
    // image is drawn at 2x and used as a template, so macOS tints it for the menu bar.
    StatusItem(const QImage &image, const QString &toolTip);
    ~StatusItem();

    // triggered runs on the main thread after the menu has closed.
    void addAction(const QString &title, std::function<void()> triggered);
    void addSeparator();

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace usnip::platform
