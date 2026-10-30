#pragma once

#include "Crypto.h"
#include "SecureBytes.h"

#include <QString>

namespace usnip {

// A stored snippet. The title stays in memory as plain text so the popup can list it;
// the body stays sealed until the moment it is pasted.
struct Snippet {
    QString id;
    QString title;
    crypto::Bytes sealedBody;
};

// A decrypted snippet, used only for editing, import and export.
struct PlainSnippet {
    QString title;
    SecureBytes body;
};

} // namespace usnip
