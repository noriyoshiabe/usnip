#pragma once

#include "core/SecureBytes.h"

#include <optional>

namespace usnip::platform {

// Reads the master key from the login keychain, creating it on first launch.
// Returns nullopt when the keychain refuses access; a new key is only created when the
// item does not exist, so a denied prompt never replaces the key that encrypts the data.
std::optional<SecureBytes> loadOrCreateMasterKey();

} // namespace usnip::platform
