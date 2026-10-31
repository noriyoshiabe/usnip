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
| 4 | The copy is removed after 5 seconds (configurable) | Clear if `changeCount` is unchanged, then restore the previous contents |
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
| The secret stays on the pasteboard | Cleared after 5 seconds (by default) | Yes |
| Malware reads the pasteboard within those seconds | None | No |
| Screen sharing or someone looking while the editor is open | None; do not open the editor while sharing | No |
| Keyloggers, root compromise, someone using the unlocked Mac | None | Out of scope |

Anyone who can use the unlocked Mac can paste snippets. That is the accepted price of
not authenticating on paste.

## Architecture

Qt (C++) owns the UI and the data file. Everything macOS specific lives in a few
Objective-C++ files under `src/platform/`, which expose plain C++ headers so AppKit types never
reach the Qt side.

```
src/app       main.cpp, PopupWindow, PasteController, EditorWindow, PassphraseDialog,
              SettingsWindow, ShortcutEdit, Settings
src/core      SecureBytes, Crypto, SnippetStore, JsonImport, ExportFile   -- unit tested
src/platform  GlobalHotkey.mm  -> Carbon
              Pasteboard.mm    -> AppKit NSPasteboard
              Paste.mm         -> NSWorkspace, CGEvent, Accessibility
              Keychain.mm      -> Security.framework
              Auth.mm          -> LocalAuthentication
              StatusItem.mm    -> AppKit NSStatusItem (menu bar icon)
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
   The default is ⇧⌘B, the snippet shortcut of Clipy, which μSnip replaces; Settings… changes
   it. It hides the bookmarks bar shortcut of Safari and Chrome. `RegisterEventHotKey` does not
   refuse a shortcut another app has registered, so a clash goes unnoticed.
2. **Remember the target.** `NSWorkspace.frontmostApplication` is recorded.
3. **Show the filter** near the mouse cursor, kept inside the screen that holds the cursor, or
   at the centre of that screen (Settings…). Centred, the tallest list is centred, so the
   filter does not move as matches appear.
   - No title is shown until something is typed, like Spotlight. Work and private snippets
     live side by side, and a full list would put all of them on screen at once.
   - Typing filters: every space-separated word must appear in the title (substring, case
     insensitive, any order). The list grows downwards with the matches.
     No match shows nothing.
   - Up/Down move, Return chooses, ⌘1–⌘9 choose the first nine matches directly, Esc closes.
   - With no snippets at all, a line points to the editor (Edit…).
4. **Decrypt and copy.** Every item on the pasteboard is saved with all of its types.
   The chosen body is decrypted and written with the ConcealedType and TransientType markers,
   and the resulting `changeCount` is recorded. If our previous secret is still on the
   pasteboard (two pastes within the clear delay), the earlier saved contents are kept instead
   of saving the secret.
5. **Return and paste.** The popup closes, the recorded app is activated, and after a short
   delay (50 ms) Cmd+V is posted with `CGEventPost`.
   Without Accessibility permission μSnip only copies.
6. **Clear after a delay**, 5 seconds by default (1 to 60 in Settings…). If `changeCount` is
   unchanged, the pasteboard is cleared and the saved contents are written back. If it changed,
   the user copied something else; the saved contents are dropped. On quit, a pending restore
   happens immediately. Restored items keep their original types, including another app's
   ConcealedType.

Whether Clipy honors ConcealedType has not been verified yet. If it does not, Clipy has to go or
μSnip has to be excluded in its settings. The restored contents may appear in Clipy's history
once more; they were there already.

## Editor and authentication

The editor shows plain text, so opening it requires owner authentication.

- **Entry point.** A menu bar icon (`NSStatusItem`, drawn "μS", centred on the ink) with Edit…,
  Settings…, Quit. Not `QSystemTrayIcon`: in Qt 6.11 it reads `clickCount` from
  `NSApp.currentEvent` without checking the event type. On macOS 27 the menu opens from a scene
  action, so after typing in the editor that event is a key event and AppKit aborts the app.
  `LSUIElement` keeps μSnip out of the Dock.
- **Authentication.** `LAPolicyDeviceOwnerAuthentication`: Touch ID when available, otherwise an
  Apple Watch or the login password.
- **Layout.** Titles on the left (add, delete, drag to reorder); title and body on the right.
  The list takes ⌘-click and ⇧-click, so several snippets (say, a repeated import) are deleted
  with one confirmation. The right side shows the current one.
  A ⋯ button under the list holds Export to .usnip…, Import .usnip… and Import JSON….
  An agent app may get no menu bar of its own, so the window carries its menu.
- **Body.** Shown in plain text. Only the selected snippet is decrypted.
  The body is not masked: authentication keeps other people out, and screen sharing is handled
  by not opening the editor while sharing (see Threat model). A mask would have meant two
  editors, one line masked and several lines shown on demand, for little gain.
- **Save.** Changes are saved as they are made, with no Save button: add, delete and reorder at
  once, title and body edits when another snippet is selected, on ⌘S, on close and on quit.
  Every save re-encrypts the file.
- **Close.** Closing the editor, or 2 minutes without input, saves and deletes the window; the
  next open authenticates again. The widgets hold the shown body in a `QString`, which cannot be
  wiped, so plain text may stay in freed memory after closing. Accepted: it is one snippet, and
  only after the owner opened it.
- **Titles.** An empty title becomes "Untitled" when saved: it would be invisible in the popup,
  and imports refuse it.

## Settings

Only the shortcut, the clear delay and where the popup appears. Settings… in the menu bar menu
opens them without authentication, since none reveals a snippet. Changes apply at once and are
stored with `QSettings` in `~/Library/Preferences/io.github.noriyoshiabe.uSnip.plist`; invalid
values fall back to the defaults.

- **Shortcut recorder.** Click the field and type the combination; Esc keeps the old one.
  The key is stored as its virtual key code (`QKeyEvent::nativeVirtualKey`), which is what
  `RegisterEventHotKey` takes, and shown with the character of the current keyboard layout
  (`UCKeyTranslate`). A Qt key would depend on the layout and on Shift (⇧1 arrives as `!`).
  Command, Control or Option is required; Shift alone would take a typed character.
- While recording, the global shortcut is unregistered. Otherwise typing the current shortcut
  would open the popup instead of reaching the field.
- Another app's shortcut cannot be detected (see Paste flow), so no clash is warned about.
  A shortcut macOS refuses outright is reported and the previous one is kept.

## Import and export

Run from the editor's menu, which is already authenticated.

| Action | What it does |
| --- | --- |
| Export | Asks for a passphrase twice and writes every snippet to a `.usnip` file. It does not use the Keychain key, so another Mac can read it. |
| Import `.usnip` | Reads a `.usnip` file with its passphrase. For moving to a new Mac or recovering a lost key. |
| Import JSON | Reads plain JSON `[{"title": "...", "body": "..."}]`. Data from Clipy is converted into this format by the user. |

- Imports append after the existing snippets and never replace them. The count and titles are
  shown for confirmation first (the first 15 titles, then how many more).
- Export asks for the destination first, then the passphrase, so cancelling the file dialog
  costs no typing. The passphrase must be typed twice: a typo would lock the backup away.
- Import checks the header before asking for a passphrase, so another kind of file is reported
  as such. A wrong passphrase asks again; a tampered file cannot be told apart from it.
- `.usnip` layout: `"USNX" | version (1) | salt (16) | iterations (uint32 BE) | seal(json)`,
  where the keys come from PBKDF2 over the passphrase and the header is the associated data.
  The payload is the same JSON exchange format. Storing the iteration count lets it grow later
  without breaking old files; imports refuse counts above 10,000,000.
- The JSON file is plain text. After importing, μSnip offers to move it to the Trash.
  The user deletes the Clipy snippets and any intermediate files by hand.

## Build, signing and permissions

- CMake with `LANGUAGES CXX OBJCXX`, `MACOSX_BUNDLE`, Qt 6 from Homebrew.
- Frameworks: AppKit, ApplicationServices, Carbon, LocalAuthentication, Security.
  CommonCrypto is part of libSystem and needs no extra linking.
- `cmake --build build --target deploy` copies the app to `build/deploy/` and runs
  `macdeployqt` on the copy, so Homebrew Qt upgrades cannot break it, then signs again.
  The copy keeps later builds from relinking a bundle that already holds Qt.
- **Signing.** Accessibility permission follows the code signature's designated requirement.
  An ad-hoc signature changes on every build, which means granting it again after each
  rebuild; signing with the same self-signed certificate keeps it (`USNIP_CODESIGN_IDENTITY`,
  see the README). Keychain access cannot be kept that way (see Encryption and key
  management). An Apple Development certificate would keep both, but μSnip must not depend
  on a paid developer account.
- **Accessibility.** Requested with `AXIsProcessTrustedWithOptions` on launch.
- **Notarization.** Not needed: a locally built app has no quarantine attribute. If μSnip is
  published, users build it themselves.
