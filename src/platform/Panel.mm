#include "Panel.h"

#include <QWidget>

#import <AppKit/AppKit.h>

namespace usnip::platform {

namespace {

NSWindow *nativeWindow(QWidget *widget)
{
    auto *view = (__bridge NSView *)reinterpret_cast<void *>(widget->winId());
    return view.window;
}

} // namespace

void makeNonactivatingPanel(QWidget *widget)
{
    NSWindow *window = nativeWindow(widget);
    window.styleMask |= NSWindowStyleMaskNonactivatingPanel;
    // Qt sets this for Qt::Tool. The app stays inactive while the panel is up, so it would never
    // be shown.
    window.hidesOnDeactivate = NO;
}

void showAsKeyPanel(QWidget *widget)
{
    widget->show();
    [nativeWindow(widget) makeKeyAndOrderFront:nil];
}

bool isKeyWindow(QWidget *widget)
{
    return nativeWindow(widget).isKeyWindow;
}

void onResignKey(QWidget *widget, std::function<void()> resigned)
{
    id observer =
        [NSNotificationCenter.defaultCenter addObserverForName:NSWindowDidResignKeyNotification
                                                        object:nativeWindow(widget)
                                                         queue:NSOperationQueue.mainQueue
                                                    usingBlock:^(NSNotification *) {
                                                        resigned();
                                                    }];
    QObject::connect(widget, &QObject::destroyed,
                     [observer] { [NSNotificationCenter.defaultCenter removeObserver:observer]; });
}

} // namespace usnip::platform
