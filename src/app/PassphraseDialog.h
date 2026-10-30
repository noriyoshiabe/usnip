#pragma once

#include "core/SecureBytes.h"

#include <QString>

#include <optional>

class QWidget;

namespace usnip {

enum class PassphraseMode {
    // Asks twice and accepts only matching entries, so a typo cannot lock the export away.
    Create,
    Enter,
};

// The passphrase as UTF-8, or nullopt when cancelled. The line edits hold it in a QString,
// which cannot be wiped; the dialog is deleted right after, which is the best Qt allows.
std::optional<SecureBytes> askPassphrase(QWidget *parent, const QString &title,
                                         const QString &message, PassphraseMode mode);

} // namespace usnip
