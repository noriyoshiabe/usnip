#pragma once

#include "SecureBytes.h"

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

// AES-256-CBC + HMAC-SHA256 (Encrypt-then-MAC) on top of CommonCrypto.
// See docs/design.md "Encryption and key management" for the rationale and the layout.
namespace usnip::crypto {

using Bytes = std::vector<std::uint8_t>;
using ByteView = std::span<const std::uint8_t>;

constexpr std::size_t kKeySize = 32;
constexpr std::size_t kIvSize = 16;
constexpr std::size_t kBlockSize = 16;
constexpr std::size_t kMacSize = 32;
constexpr std::size_t kSaltSize = 16;

// Independent encryption and MAC keys derived from one master key.
struct Keys {
    SecureBytes enc;
    SecureBytes mac;
};

SecureBytes randomBytes(std::size_t size);

// enc = HMAC-SHA256(master, "uSnip enc v1"), mac = HMAC-SHA256(master, "uSnip mac v1").
Keys deriveKeys(const SecureBytes &master);

// PBKDF2-HMAC-SHA256 producing a kKeySize master key.
SecureBytes deriveFromPassphrase(const SecureBytes &passphrase, ByteView salt,
                                 std::uint32_t iterations);

// Returns IV || ciphertext || MAC, where MAC = HMAC(mac, len(aad) || aad || IV || ciphertext).
// aad is authenticated but not encrypted; it binds the ciphertext to its context.
std::optional<Bytes> seal(const Keys &keys, ByteView aad, ByteView plaintext);

// Verifies the MAC in constant time before decrypting. nullopt on any failure.
std::optional<SecureBytes> open(const Keys &keys, ByteView aad, ByteView sealed);

inline ByteView asBytes(std::string_view s)
{
    return {reinterpret_cast<const std::uint8_t *>(s.data()), s.size()};
}

} // namespace usnip::crypto
