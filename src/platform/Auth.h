#pragma once

#include <QString>

#include <functional>

namespace usnip::platform {

// Asks for Touch ID, an Apple Watch or the login password (LAPolicyDeviceOwnerAuthentication).
// done runs on the main thread.
void authenticateOwner(const QString &reason, std::function<void(bool success)> done);

} // namespace usnip::platform
