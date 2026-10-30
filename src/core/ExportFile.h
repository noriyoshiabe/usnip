#pragma once

#include "Snippet.h"

#include <QByteArray>

#include <cstdint>
#include <optional>
#include <vector>

// Passphrase-protected export file (.usnip). It does not depend on the Keychain key,
// so it can be imported on another Mac.
//
// Layout: "USNX" | version (1) | salt (16) | iterations (uint32 BE)
//         | seal(deriveKeys(PBKDF2(passphrase, salt, iterations)), aad = header, json)
// The JSON payload is the plain exchange format of jsonimport.
namespace usnip::exportfile {

constexpr std::uint8_t kFormatVersion = 1;
constexpr std::uint32_t kDefaultIterations = 600000;
// Upper bound accepted on import, so a crafted file cannot stall the app.
constexpr std::uint32_t kMaxIterations = 10000000;

std::optional<QByteArray> write(const std::vector<PlainSnippet> &snippets,
                                const SecureBytes &passphrase,
                                std::uint32_t iterations = kDefaultIterations);

// nullopt on a wrong passphrase, a tampered file or an unknown format.
std::optional<std::vector<PlainSnippet>> read(const QByteArray &data,
                                              const SecureBytes &passphrase);

} // namespace usnip::exportfile
