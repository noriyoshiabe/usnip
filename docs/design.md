# μSnip design

μSnip pastes secrets such as database passwords and Basic auth credentials from a shortcut,
without leaving them in plain text on disk or in clipboard history.
It replaces unencrypted snippets in clipboard managers such as Clipy.

Web site passwords are out of scope: they belong in the macOS Passwords app.
μSnip is a personal tool with a deliberately small feature set. It is built from source
(no notarized binary) and has no dependency outside macOS and Qt.

## Requirements

| # | Requirement | How |
| --- | --- | --- |
| 1 | A shortcut shows the list of snippet titles | Carbon `RegisterEventHotKey`, a Qt tool window |
| 2 | Choosing one copies it and pastes it | `NSPasteboard`, then Cmd+V via `CGEventPost` |
| 3 | Clipboard history apps ignore the copy | `org.nspasteboard.ConcealedType` and `TransientType` |
| 4 | The copy is removed after 5 seconds | Clear if `changeCount` is unchanged, then restore the previous contents |
| 5 | Snippets are encrypted | AES-256-CBC + HMAC-SHA256, key in the Keychain |
| 6 | Snippets can be added and edited | Qt editor window, opened after owner authentication |

Pasting never asks for authentication: speed is the point. The build uses the Command Line
Tools, CMake and Qt; Xcode is not needed.

## Threat model

What is protected is the snippet data on disk and the plain text left on the pasteboard.
A process running as the same user must not get plain text just by reading files.

| Threat | Countermeasure | Covered |
| --- | --- | --- |
| An AI agent or other process reads the data file | The file holds only ciphertext | Yes |
| Another process reads the key from the Keychain | The item's access list trusts only μSnip's signature; others trigger a prompt | Yes, if the prompt is denied |
| Backups or cloud sync copy the file | The key stays in the local login keychain, not iCloud | Yes |
| Clipboard history apps store the secret | ConcealedType marker | Apps that follow nspasteboard.org |
| The secret stays on the pasteboard | Cleared after 5 seconds | Yes |
| Malware reads the pasteboard within those 5 seconds | None | No |
| Keyloggers, root compromise, someone using the unlocked Mac | None | Out of scope |

Anyone who can use the unlocked Mac can paste snippets. That is the accepted price of
not authenticating on paste.

## Architecture

Qt (C++) owns the UI and the data file. Everything macOS specific lives in a few
Objective-C++ files under `src/platform/`, which expose plain C++ headers so AppKit types never
reach the Qt side.

```
src/app       main.cpp, PopupWindow, PasteController (later: EditorWindow, Settings)
src/core      SecureBytes, Crypto, SnippetStore, JsonImport, ExportFile   -- unit tested
src/platform  GlobalHotkey.mm  -> Carbon
              Pasteboard.mm    -> AppKit NSPasteboard
              Paste.mm         -> NSWorkspace, CGEvent, Accessibility
              Keychain.mm      -> Security.framework
              Auth.mm          -> LocalAuthentication
```

`src/core` has no UI and no Objective-C. It depends on QtCore and CommonCrypto only, and
`tests/` covers it.

## Encryption and key management

Snippets are encrypted with a random 256-bit master key stored in the Keychain.
There is no master password.

**Cipher.** Only CommonCrypto (part of macOS) is used; no third-party library is linked.
CryptoKit, which offers AES-GCM, is Swift only, and CommonCrypto has no public GCM API, so the
construction is AES-256-CBC + HMAC-SHA256, Encrypt-then-MAC (`src/core/Crypto.h`):

- Two subkeys are derived from the master key: `enc = HMAC-SHA256(master, "uSnip enc v1")` and
  `mac = HMAC-SHA256(master, "uSnip mac v1")`.
- A fresh 16-byte IV comes from `CCRandomGenerateBytes` for every seal.
- Sealed bytes are `IV || ciphertext || MAC`, where
  `MAC = HMAC(mac, len(aad) as uint64 BE || aad || IV || ciphertext)`.
  The associated data binds a ciphertext to its context (file header, snippet id).
- The MAC is checked with `timingsafe_bcmp` before anything is decrypted.
- Keys derived from a passphrase (export files only) use `CCKeyDerivationPBKDF`,
  PBKDF2-HMAC-SHA256, 600,000 iterations by default.

**The key.** On first launch 32 random bytes are stored as a generic password item
(service `io.github.noriyoshiabe.usnip`, account `master-key`) in the login keychain.
The item is not synchronizable. Its default access list trusts the creating app's code
signature, so μSnip reads it without a prompt and other apps get one. The data protection
keychain would need an entitlement and a paid developer account, so it is not used.
A new key is only created when the item does not exist; a denied prompt never replaces it.
The item's partition list holds the creating build's `cdhash`, because a self-signed app has no
team ID. Each rebuild therefore asks once more, and Always Allow adds the new hash. Other
processes still get the prompt.

**The data file.** `~/Library/Application Support/uSnip/snippets.bin`, mode 0600, written
atomically (`QSaveFile`).

- Layout: `"USNP" | version (1 byte) | seal(aad = those 5 bytes, json)`.
- The whole file is encrypted, because titles such as "production DB" are information too.
- JSON: `{"version": 1, "items": [{"id", "title", "body"}]}` where `body` is
  `base64(seal(aad = "body:" + id, plaintext))`.
- Bodies are sealed a second time so that only titles are plain text in memory while the app
  runs. A body is decrypted only at the moment it is pasted, into a `SecureBytes` that wipes
  itself, and goes from there to `NSPasteboard` without passing through `QString`.

Losing the Keychain item makes the data unreadable. The passphrase-protected export below is
the backup.

## Paste flow

Target: the paste completes about 0.1 s after a snippet is chosen.

1. **Shortcut.** `RegisterEventHotKey` delivers it; no permission is needed.
   The default is ⇧⌘B, the snippet shortcut of Clipy, which μSnip replaces. It hides the
   bookmarks bar shortcut of Safari and Chrome. `RegisterEventHotKey` does not refuse a
   shortcut another app has registered, so a clash goes unnoticed.
2. **Remember the target.** `NSWorkspace.frontmostApplication` is recorded.
3. **Show the list** near the mouse cursor, kept inside the screen that holds the cursor.
   - Typing filters, Up/Down move, Return chooses, Esc closes.
   - While the filter is empty, 1–9 choose the first nine rows directly.
4. **Decrypt and copy.** Every item on the pasteboard is saved with all of its types.
   The chosen body is decrypted and written with the ConcealedType and TransientType markers,
   and the resulting `changeCount` is recorded. If our previous secret is still on the
   pasteboard (two pastes within 5 seconds), the earlier saved contents are kept instead of
   saving the secret.
5. **Return and paste.** The popup closes, the recorded app is activated, and after a short
   delay (50 ms) Cmd+V is posted with `CGEventPost`.
   Without Accessibility permission μSnip only copies.
6. **Clear after 5 seconds.** If `changeCount` is unchanged, the pasteboard is cleared and the
   saved contents are written back. If it changed, the user copied something else; the saved
   contents are dropped. On quit, a pending restore happens immediately. Restored items keep
   their original types, including another app's ConcealedType.

Whether Clipy honors ConcealedType has not been verified yet. If it does not, Clipy has to go or
μSnip has to be excluded in its settings. The restored contents may appear in Clipy's history
once more; they were there already.

## Editor and authentication

The editor shows plain text, so opening it requires owner authentication.

- **Entry point.** A menu bar icon (`QSystemTrayIcon`, drawn "μ") with Edit…, Settings…, Quit.
  `LSUIElement` keeps μSnip out of the Dock.
- **Authentication.** `LAPolicyDeviceOwnerAuthentication`: Touch ID when available, otherwise an
  Apple Watch or the login password.
- **Layout.** Titles on the left (add, delete, drag to reorder); title and body on the right.
- **Body.** Masked by default and shown only while a Show button is held, for screen sharing.
- **Save and close.** Every save re-encrypts the file. Closing the editor, or 2 minutes without
  input, discards the plain text; the next open authenticates again.
- **Settings.** Only the shortcut and the clear delay, stored with `QSettings`.

## Import and export

Run from the editor's menu, which is already authenticated.

| Action | What it does |
| --- | --- |
| Export | Asks for a passphrase twice and writes every snippet to a `.usnip` file. It does not use the Keychain key, so another Mac can read it. |
| Import `.usnip` | Reads a `.usnip` file with its passphrase. For moving to a new Mac or recovering a lost key. |
| Import JSON | Reads plain JSON `[{"title": "...", "body": "..."}]`. Data from Clipy is converted into this format by the user. |

- Imports append after the existing snippets and never replace them. The count and titles are
  shown for confirmation first.
- `.usnip` layout: `"USNX" | version (1) | salt (16) | iterations (uint32 BE) | seal(json)`,
  where the keys come from PBKDF2 over the passphrase and the header is the associated data.
  The payload is the same JSON exchange format. Storing the iteration count lets it grow later
  without breaking old files; imports refuse counts above 10,000,000.
- The JSON file is plain text. After importing, μSnip offers to move it to the Trash.
  The user deletes the Clipy snippets and any intermediate files by hand.

Until the editor exists, Import JSON… sits in the menu bar menu so the paste flow can be tried.

## Build, signing and permissions

- CMake with `LANGUAGES CXX OBJCXX`, `MACOSX_BUNDLE`, Qt 6 from Homebrew.
- Frameworks: AppKit, ApplicationServices, Carbon, LocalAuthentication, Security.
  CommonCrypto is part of libSystem and needs no extra linking.
- `cmake --build build --target deploy` runs `macdeployqt`, so Homebrew Qt upgrades cannot
  break the app, then signs again.
- **Signing.** Accessibility permission follows the code signature's designated requirement.
  An ad-hoc signature changes on every build, which means granting it again after each
  rebuild; signing with the same self-signed certificate keeps it (`USNIP_CODESIGN_IDENTITY`,
  see the README). Keychain access cannot be kept that way (see Encryption and key
  management). An Apple Development certificate would keep both, but μSnip must not depend
  on a paid developer account.
- **Accessibility.** Requested with `AXIsProcessTrustedWithOptions` on launch.
- **Notarization.** Not needed: a locally built app has no quarantine attribute. If μSnip is
  published, users build it themselves.
