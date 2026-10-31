#pragma once

#include <functional>

class QWidget;

namespace usnip::platform {

// Lets a top-level Qt::Tool widget take the keyboard without activating the app, like Spotlight.
//
// The popup cannot rely on activating the app: Terminal waiting for a password (mysql -p) does
// not give up activation, and macOS 14+ treats NSApp.activate as a request it may refuse. Qt then
// keeps its Tool window hidden, since Qt::Tool panels hide while the app is inactive.
// A non-activating panel becomes the key window while the other app stays active, so typing
// reaches it and focus goes back to that app as soon as it hides. Input methods only serve the
// active app, though, so the caller still asks for activation and gets Japanese input whenever
// macOS grants it.
void makeNonactivatingPanel(QWidget *widget);

// Shows the widget and makes it the key window. Call instead of show() + activateWindow().
void showAsKeyPanel(QWidget *widget);

bool isKeyWindow(QWidget *widget);

// resigned runs on the main thread whenever the widget's window stops being the key window,
// for example when the user clicks another app.
void onResignKey(QWidget *widget, std::function<void()> resigned);

} // namespace usnip::platform
