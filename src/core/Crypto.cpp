#include "Crypto.h"

#include <CommonCrypto/CommonCrypto.h>
#include <CommonCrypto/CommonRandom.h>

#include <array>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace usnip::crypto {

namespace {

void hmacSha256(const SecureBytes &key, std::initializer_list<ByteView> parts, std::uint8_t *out)
{
    CCHmacContext ctx;
    CCHmacInit(&ctx, kCCHmacAlgSHA256, key.data(), key.size());
    for (ByteView part : parts) {
        CCHmacUpdate(&ctx, part.data(), part.size());
    }
    CCHmacFinal(&ctx, out);
    secureWipe(&ctx, sizeof(ctx));
}

SecureBytes hmacKey(const SecureBytes &master, std::string_view label)
{
    SecureBytes out(kKeySize);
    hmacSha256(master, {asBytes(label)}, out.data());
    return out;
}

std::array<std::uint8_t, 8> lengthPrefix(std::size_t length)
{
    std::array<std::uint8_t, 8> out{};
    auto value = static_cast<std::uint64_t>(length);
    for (int i = 7; i >= 0; --i) {
        out[i] = static_cast<std::uint8_t>(value & 0xff);
        value >>= 8;
    }
    return out;
}

void computeMac(const Keys &keys, ByteView aad, ByteView ivAndCiphertext, std::uint8_t *out)
{
    const auto prefix = lengthPrefix(aad.size());
    hmacSha256(keys.mac, {prefix, aad, ivAndCiphertext}, out);
}

} // namespace

SecureBytes randomBytes(std::size_t size)
{
    SecureBytes out(size);
    if (CCRandomGenerateBytes(out.data(), out.size()) != kCCSuccess) {
        // There is no safe fallback for a broken system RNG.
        throw std::runtime_error("CCRandomGenerateBytes failed");
    }
    return out;
}

Keys deriveKeys(const SecureBytes &master)
{
    return {hmacKey(master, "uSnip enc v1"), hmacKey(master, "uSnip mac v1")};
}

SecureBytes deriveFromPassphrase(const SecureBytes &passphrase, ByteView salt,
                                 std::uint32_t iterations)
{
    SecureBytes out(kKeySize);
    const int status = CCKeyDerivationPBKDF(
        kCCPBKDF2, reinterpret_cast<const char *>(passphrase.data()), passphrase.size(),
        salt.data(), salt.size(), kCCPRFHmacAlgSHA256, iterations, out.data(), out.size());
    if (status != kCCSuccess) {
        throw std::runtime_error("CCKeyDerivationPBKDF failed");
    }
    return out;
}

std::optional<Bytes> seal(const Keys &keys, ByteView aad, ByteView plaintext)
{
    if (keys.enc.size() != kKeySize || keys.mac.size() != kKeySize) {
        return std::nullopt;
    }

    Bytes out(kIvSize + plaintext.size() + kBlockSize + kMacSize);
    const SecureBytes iv = randomBytes(kIvSize);
    std::memcpy(out.data(), iv.data(), kIvSize);

    std::size_t written = 0;
    const CCCryptorStatus status =
        CCCrypt(kCCEncrypt, kCCAlgorithmAES, kCCOptionPKCS7Padding, keys.enc.data(),
                keys.enc.size(), iv.data(), plaintext.data(), plaintext.size(),
                out.data() + kIvSize, out.size() - kIvSize - kMacSize, &written);
    if (status != kCCSuccess) {
        return std::nullopt;
    }

    const std::size_t macOffset = kIvSize + written;
    computeMac(keys, aad, ByteView(out.data(), macOffset), out.data() + macOffset);
    out.resize(macOffset + kMacSize);
    return out;
}

std::optional<SecureBytes> open(const Keys &keys, ByteView aad, ByteView sealed)
{
    if (keys.enc.size() != kKeySize || keys.mac.size() != kKeySize) {
        return std::nullopt;
    }
    if (sealed.size() < kIvSize + kBlockSize + kMacSize) {
        return std::nullopt;
    }
    const std::size_t macOffset = sealed.size() - kMacSize;
    if ((macOffset - kIvSize) % kBlockSize != 0) {
        return std::nullopt;
    }

    std::array<std::uint8_t, kMacSize> expected{};
    computeMac(keys, aad, sealed.first(macOffset), expected.data());
    if (timingsafe_bcmp(expected.data(), sealed.data() + macOffset, kMacSize) != 0) {
        return std::nullopt;
    }

    const std::size_t ciphertextSize = macOffset - kIvSize;
    SecureBytes plaintext(ciphertextSize);
    std::size_t written = 0;
    const CCCryptorStatus status =
        CCCrypt(kCCDecrypt, kCCAlgorithmAES, kCCOptionPKCS7Padding, keys.enc.data(),
                keys.enc.size(), sealed.data(), sealed.data() + kIvSize, ciphertextSize,
                plaintext.data(), plaintext.size(), &written);
    if (status != kCCSuccess) {
        return std::nullopt;
    }
    plaintext.truncate(written);
    return plaintext;
}

} // namespace usnip::crypto
