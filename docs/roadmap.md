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

- [ ] Focus returns to the original app and Cmd+V lands there. Try Terminal, iTerm2, a browser
      address bar, a browser text field and a Qt app. Tune `kPasteDelay` in
      `src/app/PasteController.cpp`.
- [ ] The popup takes keyboard focus while another app is active (`Qt::Popup` + `activateSelf`).
      If not, switch to a `Qt::Tool` window that hides on deactivation.
- [ ] Clipy (and any other history app in use) ignores copies with ConcealedType.
- [ ] Restoring the previous pasteboard contents works for text, images and files.
- [ ] With a self-signed certificate, Accessibility permission and the Keychain item survive
      a rebuild without new prompts. Also check after `deploy`.
- [ ] `⌥⌘V` does not clash with anything in daily use.

## Phase 2: Daily use without the editor

- [ ] Show an error in the popup when there are no snippets, pointing to Import JSON…
- [ ] Remember the last chosen snippet and preselect it
- [ ] Menu bar menu: "Clear clipboard now"
- [ ] Log nothing that contains titles or bodies (audit)

## Phase 3: Editor

- [ ] `EditorWindow`: list on the left (add, delete, drag to reorder), title and body on the right
- [ ] Open only after `platform::authenticateOwner`; discard plain text on close or after 2 minutes idle
- [ ] Body masked, shown only while Show is held
- [ ] Save re-encrypts the whole file
- [ ] Move Import JSON… into the editor's menu

## Phase 4: Settings, export and import

- [ ] Settings window: shortcut recorder, clear delay (`QSettings`)
- [ ] Export to `.usnip` with a passphrase entered twice
- [ ] Import `.usnip`, showing count and titles before appending
- [ ] Offer to trash a plain JSON file after import (already in the menu version)

## Phase 5: Publish

- [ ] README: screenshots, full build and signing walkthrough
- [ ] Choose and add the license (GPL planned)
- [ ] Tag 1.0
