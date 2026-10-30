#include "SnippetStore.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace usnip {

namespace {

constexpr char kMagic[4] = {'U', 'S', 'N', 'P'};
constexpr std::size_t kHeaderSize = sizeof(kMagic) + 1;

QByteArray header()
{
    QByteArray out(kMagic, sizeof(kMagic));
    out.append(static_cast<char>(SnippetStore::kFormatVersion));
    return out;
}

crypto::ByteView view(const QByteArray &bytes)
{
    return {reinterpret_cast<const std::uint8_t *>(bytes.constData()),
            static_cast<std::size_t>(bytes.size())};
}

QByteArray bodyAad(const QString &id)
{
    return QByteArrayLiteral("body:") + id.toUtf8();
}

QString newId()
{
    const SecureBytes raw = crypto::randomBytes(16);
    return QString::fromLatin1(
        QByteArray(reinterpret_cast<const char *>(raw.data()), static_cast<qsizetype>(raw.size()))
            .toHex());
}

} // namespace

SnippetStore::SnippetStore(crypto::Keys keys) : m_keys(std::move(keys))
{
}

bool SnippetStore::load(const QByteArray &fileData)
{
    const QByteArray expectedHeader = header();
    if (!fileData.startsWith(expectedHeader)) {
        return false;
    }
    const QByteArray sealed = fileData.mid(kHeaderSize);
    const auto json = crypto::open(m_keys, view(expectedHeader), view(sealed));
    if (!json) {
        return false;
    }

    const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromRawData(
        reinterpret_cast<const char *>(json->data()), static_cast<qsizetype>(json->size())));
    const QJsonObject root = doc.object();
    if (root.value(QLatin1String("version")).toInt() != kFormatVersion) {
        return false;
    }

    std::vector<Snippet> loaded;
    for (const QJsonValue value : root.value(QLatin1String("items")).toArray()) {
        const QJsonObject item = value.toObject();
        const QByteArray body =
            QByteArray::fromBase64(item.value(QLatin1String("body")).toString().toLatin1());
        loaded.push_back({item.value(QLatin1String("id")).toString(),
                          item.value(QLatin1String("title")).toString(),
                          crypto::Bytes(body.begin(), body.end())});
    }
    m_snippets = std::move(loaded);
    return true;
}

std::optional<QByteArray> SnippetStore::serialize() const
{
    QJsonArray items;
    for (const Snippet &snippet : m_snippets) {
        QJsonObject item;
        item.insert(QLatin1String("id"), snippet.id);
        item.insert(QLatin1String("title"), snippet.title);
        item.insert(QLatin1String("body"),
                    QString::fromLatin1(
                        QByteArray(reinterpret_cast<const char *>(snippet.sealedBody.data()),
                                   static_cast<qsizetype>(snippet.sealedBody.size()))
                            .toBase64()));
        items.append(item);
    }
    QJsonObject root;
    root.insert(QLatin1String("version"), kFormatVersion);
    root.insert(QLatin1String("items"), items);
    const QByteArray json = QJsonDocument(root).toJson(QJsonDocument::Compact);

    QByteArray out = header();
    const auto sealed = crypto::seal(m_keys, view(out), view(json));
    if (!sealed) {
        return std::nullopt;
    }
    out.append(reinterpret_cast<const char *>(sealed->data()),
               static_cast<qsizetype>(sealed->size()));
    return out;
}

bool SnippetStore::loadFile(const QString &path)
{
    QFile file(path);
    if (!file.exists()) {
        m_snippets.clear();
        return true;
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    return load(file.readAll());
}

bool SnippetStore::saveFile(const QString &path) const
{
    const auto data = serialize();
    if (!data) {
        return false;
    }
    QDir().mkpath(QFileInfo(path).absolutePath());

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(*data) != data->size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

std::optional<SecureBytes> SnippetStore::body(std::size_t index) const
{
    if (index >= m_snippets.size()) {
        return std::nullopt;
    }
    const Snippet &snippet = m_snippets[index];
    return crypto::open(m_keys, view(bodyAad(snippet.id)), snippet.sealedBody);
}

bool SnippetStore::add(const QString &title, const SecureBytes &body)
{
    const QString id = newId();
    auto sealed = sealBody(id, body);
    if (!sealed) {
        return false;
    }
    m_snippets.push_back({id, title, std::move(*sealed)});
    return true;
}

bool SnippetStore::update(std::size_t index, const QString &title, const SecureBytes &body)
{
    if (index >= m_snippets.size()) {
        return false;
    }
    Snippet &snippet = m_snippets[index];
    auto sealed = sealBody(snippet.id, body);
    if (!sealed) {
        return false;
    }
    snippet.title = title;
    snippet.sealedBody = std::move(*sealed);
    return true;
}

void SnippetStore::remove(std::size_t index)
{
    if (index < m_snippets.size()) {
        m_snippets.erase(m_snippets.begin() + static_cast<std::ptrdiff_t>(index));
    }
}

void SnippetStore::move(std::size_t from, std::size_t to)
{
    if (from >= m_snippets.size() || to >= m_snippets.size() || from == to) {
        return;
    }
    Snippet snippet = std::move(m_snippets[from]);
    m_snippets.erase(m_snippets.begin() + static_cast<std::ptrdiff_t>(from));
    m_snippets.insert(m_snippets.begin() + static_cast<std::ptrdiff_t>(to), std::move(snippet));
}

std::vector<PlainSnippet> SnippetStore::decryptAll() const
{
    std::vector<PlainSnippet> out;
    for (std::size_t i = 0; i < m_snippets.size(); ++i) {
        if (auto plain = body(i)) {
            out.push_back({m_snippets[i].title, std::move(*plain)});
        }
    }
    return out;
}

std::optional<crypto::Bytes> SnippetStore::sealBody(const QString &id,
                                                    const SecureBytes &body) const
{
    return crypto::seal(m_keys, view(bodyAad(id)), body.view());
}

} // namespace usnip
