#include "StatusItem.h"

#include <QCoreApplication>

#import <AppKit/AppKit.h>

// Runs a C++ callback for one menu item. NSMenuItem holds its target weakly, so each target is
// also kept as the item's representedObject.
@interface USnipMenuTarget : NSObject
- (instancetype)initWithCallback:(std::function<void()>)callback;
- (void)trigger:(id)sender;
@end

@implementation USnipMenuTarget {
    std::function<void()> _callback;
}

- (instancetype)initWithCallback:(std::function<void()>)callback
{
    if ((self = [super init])) {
        _callback = std::move(callback);
    }
    return self;
}

- (void)trigger:(id)sender
{
    (void)sender;
    // Queued, so that dialogs and windows opened by the callback do not run inside menu tracking.
    QMetaObject::invokeMethod(qApp, [callback = _callback] { callback(); }, Qt::QueuedConnection);
}

@end

namespace usnip::platform {

struct StatusItem::Impl {
    NSStatusItem *item = nil;
    NSMenu *menu = nil;
};

StatusItem::StatusItem(const QImage &image, const QString &toolTip) : d(std::make_unique<Impl>())
{
    d->item = [NSStatusBar.systemStatusBar statusItemWithLength:NSSquareStatusItemLength];
    CGImageRef cgImage = image.toCGImage();
    NSImage *icon =
        [[NSImage alloc] initWithCGImage:cgImage
                                    size:NSMakeSize(image.width() / 2.0, image.height() / 2.0)];
    CGImageRelease(cgImage);
    [icon setTemplate:YES];
    d->item.button.image = icon;
    d->item.button.toolTip = toolTip.toNSString();
    d->menu = [[NSMenu alloc] init];
    d->item.menu = d->menu;
}

StatusItem::~StatusItem()
{
    [NSStatusBar.systemStatusBar removeStatusItem:d->item];
}

void StatusItem::addAction(const QString &title, std::function<void()> triggered)
{
    USnipMenuTarget *target = [[USnipMenuTarget alloc] initWithCallback:std::move(triggered)];
    NSMenuItem *menuItem = [[NSMenuItem alloc] initWithTitle:title.toNSString()
                                                      action:@selector(trigger:)
                                               keyEquivalent:@""];
    menuItem.target = target;
    menuItem.representedObject = target;
    [d->menu addItem:menuItem];
}

void StatusItem::addSeparator()
{
    [d->menu addItem:[NSMenuItem separatorItem]];
}

} // namespace usnip::platform
