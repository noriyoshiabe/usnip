#pragma once

#include "Crypto.h"
#include "Snippet.h"

#include <QByteArray>
#include <QString>

#include <optional>
#include <vector>

namespace usnip {

// The encrypted snippet file.
//
// File layout: "USNP" | version (1 byte) | seal(keys, aad = first 5 bytes, json)
// JSON: {"version": 1, "items": [{"id", "title", "body": base64(seal(keys, "body:" + id, body))}]}
//
// Bodies are sealed a second time so that only titles are plain text while the app runs.
class SnippetStore {
public:
    static constexpr std::uint8_t kFormatVersion = 1;

    explicit SnippetStore(crypto::Keys keys);

    // Replaces the contents. Returns false (and leaves the store unchanged) on a wrong key,
    // a tampered file or an unknown format.
    bool load(const QByteArray &fileData);
    std::optional<QByteArray> serialize() const;

    // A missing file is an empty store, not an error.
    bool loadFile(const QString &path);
    // Writes atomically with 0600 permissions.
    bool saveFile(const QString &path) const;

    const std::vector<Snippet> &snippets() const
    {
        return m_snippets;
    }
    std::optional<SecureBytes> body(std::size_t index) const;

    bool add(const QString &title, const SecureBytes &body);
    bool update(std::size_t index, const QString &title, const SecureBytes &body);
    void remove(std::size_t index);
    void move(std::size_t from, std::size_t to);

    std::vector<PlainSnippet> decryptAll() const;

private:
    std::optional<crypto::Bytes> sealBody(const QString &id, const SecureBytes &body) const;

    crypto::Keys m_keys;
    std::vector<Snippet> m_snippets;
};

} // namespace usnip
