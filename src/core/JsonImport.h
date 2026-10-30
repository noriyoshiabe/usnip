#pragma once

#include "Snippet.h"

#include <QByteArray>
#include <QString>

#include <optional>
#include <vector>

// The plain JSON exchange format: [{"title": "...", "body": "..."}, ...].
// It is what the user converts other tools' data into, and also the payload of an export file.
namespace usnip::jsonimport {

std::optional<std::vector<PlainSnippet>> parse(const QByteArray &json, QString *error = nullptr);

QByteArray serialize(const std::vector<PlainSnippet> &snippets);

} // namespace usnip::jsonimport
