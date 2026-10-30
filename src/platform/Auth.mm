#include "Auth.h"

#include <QCoreApplication>

#import <LocalAuthentication/LocalAuthentication.h>

#include <memory>

namespace usnip::platform {

void authenticateOwner(const QString &reason, std::function<void(bool success)> done)
{
    auto callback = std::make_shared<std::function<void(bool)>>(std::move(done));
    LAContext *context = [[LAContext alloc] init];
    [context evaluatePolicy:LAPolicyDeviceOwnerAuthentication
            localizedReason:reason.toNSString()
                      reply:^(BOOL success, NSError *) {
                          (void)context; // keeps the context alive until the reply arrives
                          QMetaObject::invokeMethod(
                              qApp, [callback, success] { (*callback)(success); },
                              Qt::QueuedConnection);
                      }];
}

} // namespace usnip::platform
