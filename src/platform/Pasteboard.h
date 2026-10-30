#pragma once

#include "core/SecureBytes.h"

#include <memory>

namespace usnip::platform {

// The general pasteboard, as used for one paste: save what is there, put the secret on it
// marked as concealed, then put the saved contents back if nothing else was copied meanwhile.
class Pasteboard {
public:
    Pasteboard();
    ~Pasteboard();
    Pasteboard(const Pasteboard &) = delete;
    Pasteboard &operator=(const Pasteboard &) = delete;

    // Saves every item with all of its types. If our own secret is still on the pasteboard
    // from a previous paste, the earlier saved contents are kept instead.
    void saveCurrent();

    // Writes UTF-8 text with the org.nspasteboard.ConcealedType and TransientType markers.
    bool writeConcealed(const SecureBytes &utf8);

    // If the pasteboard still holds our secret, clears it and restores the saved items.
    // If the user copied something else, only drops the saved items.
    // Returns true when it restored.
    bool restoreIfUnchanged();

    bool hasPendingRestore() const;

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};

} // namespace usnip::platform
