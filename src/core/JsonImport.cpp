#include "JsonImport.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace usnip::jsonimport {

namespace {

void setError(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
}

} // namespace

std::optional<std::vector<PlainSnippet>> parse(const QByteArray &json, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        setError(error, parseError.errorString());
        return std::nullopt;
    }
    if (!doc.isArray()) {
        setError(error, QStringLiteral("top level must be an array"));
        return std::nullopt;
    }

    std::vector<PlainSnippet> snippets;
    const QJsonArray items = doc.array();
    for (qsizetype i = 0; i < items.size(); ++i) {
        const QJsonObject item = items.at(i).toObject();
        const QJsonValue title = item.value(QLatin1String("title"));
        const QJsonValue body = item.value(QLatin1String("body"));
        if (!title.isString() || !body.isString() || title.toString().isEmpty()) {
            setError(
                error,
                QStringLiteral("item %1 needs a non-empty \"title\" and a \"body\" string").arg(i));
            return std::nullopt;
        }
        QByteArray utf8 = body.toString().toUtf8();
        snippets.push_back(
            {title.toString(), SecureBytes(reinterpret_cast<const std::uint8_t *>(utf8.constData()),
                                           static_cast<std::size_t>(utf8.size()))});
        secureWipe(utf8.data(), static_cast<std::size_t>(utf8.size()));
    }
    return snippets;
}

QByteArray serialize(const std::vector<PlainSnippet> &snippets)
{
    QJsonArray items;
    for (const PlainSnippet &snippet : snippets) {
        QJsonObject item;
        item.insert(QLatin1String("title"), snippet.title);
        item.insert(QLatin1String("body"),
                    QString::fromUtf8(reinterpret_cast<const char *>(snippet.body.data()),
                                      static_cast<qsizetype>(snippet.body.size())));
        items.append(item);
    }
    return QJsonDocument(items).toJson(QJsonDocument::Compact);
}

} // namespace usnip::jsonimport
