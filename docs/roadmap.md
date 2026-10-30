# Roadmap

Each phase ends with the app usable for what it covers. Tick items as they land.
Design details are in [design.md](design.md).

## Phase 0: Scaffold (done)

- [x] Docs: design, roadmap, coding style, AGENTS.md
- [x] CMake project, app bundle, `deploy` target, optional signing
- [x] Core: `SecureBytes`, `Crypto`, `SnippetStore`, `JsonImport`, `ExportFile`, with unit tests
- [x] Platform layer: hotkey, pasteboard, paste, Keychain, owner authentication
- [x] Minimal app: menu bar icon, popup, paste flow, Import JSON… menu item
- [x] CI on a macOS runner: build and tests

## Phase 1: Verify the risky parts on a real Mac

These cannot be checked in CI. Record the results here.

- [x] Focus returns to the original app and Cmd+V lands there. Try Terminal, iTerm2, a browser
      address bar, a browser text field and a Qt app. Tune `kPasteDelay` in
      `src/app/PasteController.cpp`.
      2026-10-31, macOS 27.0.1: works in Terminal, the Chrome address bar and a page field,
      Safari and a minimal Qt 6.11 app, with `kPasteDelay` left at 50 ms. iTerm2 is not in
      use and was skipped. Esc returns to the original app as well.
- [x] The popup takes keyboard focus while another app is active (`Qt::Popup` + `activateSelf`).
      If not, switch to a `Qt::Tool` window that hides on deactivation.
      2026-10-31: the app was activated, but a `Qt::Popup` never became the key window, so
      typing went nowhere. Switched to `Qt::Tool`, hidden on `applicationStateChanged`, since
      Qt keeps reporting the window as active and visible after another app takes over.
- [x] Clipy (and any other history app in use) ignores copies with ConcealedType.
      2026-10-31, macOS 27.0.1, Clipy 1.4.0 (the only history app in use): a copy written by
      `Pasteboard::writeConcealed` did not reach Clipy's history; a plain copy right after it
      did. This holds even though Clipy's `kCPYPrefIgnoreConcealedPasteboardType` is 0.
- [x] Restoring the previous pasteboard contents works for text, images and files.
      2026-10-31, macOS 27.0.1: a harness linking `src/platform/Pasteboard.mm` restored every
      item with byte-identical types and data for plain text, HTML + RTF + text, an `NSImage`,
      PNG data, two file URLs, an empty pasteboard and a plain text copy made by another app.
      A copy made within the delay is kept, and two pastes in a row restore the original.
      Copies from Finder and from a browser image were not tried separately.
- [x] With a self-signed certificate, Accessibility permission and the Keychain item survive
      a rebuild without new prompts. Also check after `deploy`.
      2026-10-31, rebuilds and `deploy`: Accessibility survives. The Keychain asks again after every
      rebuild, because a certificate without a team ID puts the build's `cdhash` in the item's
      partition list. Accepted rather than depending on a paid Apple developer account; the
      README says so.
- [x] `⌥⌘V` does not clash with anything in daily use.
      2026-10-31: not taken by the system shortcuts or by Clipy (⇧⌘V, ⌃⌘V, ⇧⌘B, ⌃⌥⇧⌘V),
      but Finder uses it to move copied files. The default is now ⇧⌘B, Clipy's snippet
      shortcut, which μSnip replaces; Clipy's must be cleared meanwhile. `RegisterEventHotKey`
      succeeds even for a shortcut another app holds, so the "in use" check cannot be trusted.

## Phase 2: Daily use without the editor

- [ ] Show an error in the popup when there are no snippets, pointing to Import JSON…
      Waits for the popup UI discussion (filtering and the like).
- [x] Log nothing that contains titles or bodies (audit)
      2026-10-31: nothing in `src` logs. Dialogs and the tray show only paths, counts and
      fixed text; JSON import errors name the item index and Qt's parse error, never the
      content. The two exceptions (random generator, PBKDF2 failure) carry fixed text. Tests
      print values only on failure, and every value is made up.

## Phase 3: Editor

- [ ] `EditorWindow`: list on the left (add, delete, drag to reorder), title and body on the right
- [ ] Open only after `platform::authenticateOwner`; discard plain text on close or after 2 minutes idle
- [ ] Body masked, shown only while Show is held
- [ ] Save re-encrypts the whole file
- [ ] Move Import JSON… into the editor's menu

## Phase 4: Settings, export and import

- [ ] Settings window: shortcut recorder, clear delay (`QSettings`). Defaults ⇧⌘B and 5 s.
      Other apps' shortcuts cannot be detected (see phase 1), so do not promise a warning.
- [ ] Export to `.usnip` with a passphrase entered twice
- [ ] Import `.usnip`, showing count and titles before appending
- [ ] Offer to trash a plain JSON file after import (already in the menu version)

## Phase 5: Publish

- [ ] README: screenshots, full build and signing walkthrough
- [ ] Choose and add the license (GPL planned)
- [ ] Tag 1.0
