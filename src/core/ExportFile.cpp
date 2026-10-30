#include "ExportFile.h"

#include "Crypto.h"
#include "JsonImport.h"

namespace usnip::exportfile {

namespace {

constexpr char kMagic[4] = {'U', 'S', 'N', 'X'};
constexpr std::size_t kHeaderSize = sizeof(kMagic) + 1 + crypto::kSaltSize + 4;

crypto::ByteView view(const QByteArray &bytes)
{
    return {reinterpret_cast<const std::uint8_t *>(bytes.constData()),
            static_cast<std::size_t>(bytes.size())};
}

} // namespace

std::optional<QByteArray> write(const std::vector<PlainSnippet> &snippets,
                                const SecureBytes &passphrase, std::uint32_t iterations)
{
    const SecureBytes salt = crypto::randomBytes(crypto::kSaltSize);

    QByteArray out(kMagic, sizeof(kMagic));
    out.append(static_cast<char>(kFormatVersion));
    out.append(reinterpret_cast<const char *>(salt.data()), static_cast<qsizetype>(salt.size()));
    for (int shift = 24; shift >= 0; shift -= 8) {
        out.append(static_cast<char>((iterations >> shift) & 0xff));
    }

    const crypto::Keys keys =
        crypto::deriveKeys(crypto::deriveFromPassphrase(passphrase, salt.view(), iterations));
    QByteArray json = jsonimport::serialize(snippets);
    const auto sealed = crypto::seal(keys, view(out), view(json));
    secureWipe(json.data(), static_cast<std::size_t>(json.size()));
    if (!sealed) {
        return std::nullopt;
    }
    out.append(reinterpret_cast<const char *>(sealed->data()),
               static_cast<qsizetype>(sealed->size()));
    return out;
}

std::optional<std::vector<PlainSnippet>> read(const QByteArray &data, const SecureBytes &passphrase)
{
    if (static_cast<std::size_t>(data.size()) < kHeaderSize ||
        !data.startsWith(QByteArray(kMagic, sizeof(kMagic))) ||
        static_cast<std::uint8_t>(data.at(sizeof(kMagic))) != kFormatVersion) {
        return std::nullopt;
    }

    const QByteArray header = data.left(kHeaderSize);
    const crypto::ByteView salt = view(header).subspan(sizeof(kMagic) + 1, crypto::kSaltSize);
    std::uint32_t iterations = 0;
    for (std::size_t i = kHeaderSize - 4; i < kHeaderSize; ++i) {
        iterations = (iterations << 8) | static_cast<std::uint8_t>(header.at(i));
    }
    if (iterations == 0 || iterations > kMaxIterations) {
        return std::nullopt;
    }

    const crypto::Keys keys =
        crypto::deriveKeys(crypto::deriveFromPassphrase(passphrase, salt, iterations));
    const auto json = crypto::open(keys, view(header), view(data.mid(kHeaderSize)));
    if (!json) {
        return std::nullopt;
    }
    return jsonimport::parse(QByteArray::fromRawData(reinterpret_cast<const char *>(json->data()),
                                                     static_cast<qsizetype>(json->size())));
}

} // namespace usnip::exportfile
