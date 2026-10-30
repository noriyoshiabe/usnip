#include "Pasteboard.h"

#import <AppKit/AppKit.h>

namespace usnip::platform {

namespace {

// http://nspasteboard.org markers honored by clipboard history apps.
NSPasteboardType const kConcealedType = @"org.nspasteboard.ConcealedType";
NSPasteboardType const kTransientType = @"org.nspasteboard.TransientType";

} // namespace

struct Pasteboard::Impl {
    NSArray<NSPasteboardItem *> *saved = nil;
    NSInteger changeCount = 0;
    bool pending = false;
};

Pasteboard::Pasteboard() : d(std::make_unique<Impl>())
{
}

Pasteboard::~Pasteboard() = default;

void Pasteboard::saveCurrent()
{
    NSPasteboard *pasteboard = NSPasteboard.generalPasteboard;
    if (d->pending && pasteboard.changeCount == d->changeCount) {
        return;
    }

    NSMutableArray<NSPasteboardItem *> *items = [NSMutableArray array];
    for (NSPasteboardItem *item in pasteboard.pasteboardItems) {
        NSPasteboardItem *copy = [[NSPasteboardItem alloc] init];
        for (NSPasteboardType type in item.types) {
            NSData *data = [item dataForType:type];
            if (data) {
                [copy setData:data forType:type];
            }
        }
        [items addObject:copy];
    }
    d->saved = items;
}

bool Pasteboard::writeConcealed(const SecureBytes &utf8)
{
    NSString *text = [[NSString alloc] initWithBytes:utf8.data()
                                              length:utf8.size()
                                            encoding:NSUTF8StringEncoding];
    if (!text) {
        return false;
    }
    NSPasteboardItem *item = [[NSPasteboardItem alloc] init];
    [item setString:text forType:NSPasteboardTypeString];
    [item setData:[NSData data] forType:kConcealedType];
    [item setData:[NSData data] forType:kTransientType];

    NSPasteboard *pasteboard = NSPasteboard.generalPasteboard;
    [pasteboard clearContents];
    if (![pasteboard writeObjects:@[ item ]]) {
        return false;
    }
    d->changeCount = pasteboard.changeCount;
    d->pending = true;
    return true;
}

bool Pasteboard::restoreIfUnchanged()
{
    if (!d->pending) {
        return false;
    }
    d->pending = false;
    NSArray<NSPasteboardItem *> *saved = d->saved;
    d->saved = nil;

    NSPasteboard *pasteboard = NSPasteboard.generalPasteboard;
    if (pasteboard.changeCount != d->changeCount) {
        return false;
    }
    [pasteboard clearContents];
    if (saved.count > 0) {
        [pasteboard writeObjects:saved];
    }
    return true;
}

bool Pasteboard::hasPendingRestore() const
{
    return d->pending;
}

} // namespace usnip::platform
