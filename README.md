# μSnip

A minimal macOS menu bar app for pasting secrets such as database passwords from a shortcut.

- Press ⌥⌘V, pick a title, and μSnip pastes the snippet into the app you were using.
- Snippets are encrypted on disk; the key stays in your login keychain.
- The copy is marked as concealed, so clipboard history apps can ignore it, and it is removed
  after 10 seconds, putting back whatever was on the clipboard before.

No third-party dependencies beyond Qt. See [docs/design.md](docs/design.md) for how it works
and [docs/roadmap.md](docs/roadmap.md) for what is done. μSnip is under development.

## Requirements

- macOS 13 or later
- Command Line Tools (`xcode-select --install`); Xcode itself is not needed
- Homebrew: `brew install cmake ninja qt`

## Build

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH="$(brew --prefix qt)" \
      -DUSNIP_CODESIGN_IDENTITY="uSnip Local"
cmake --build build
ctest --test-dir build --output-on-failure
open build/uSnip.app
```

To bundle Qt into the app (so a Homebrew Qt upgrade cannot break it):

```sh
cmake --build build --target deploy
```

## Signing with a self-signed certificate

macOS ties the Accessibility permission and the Keychain item's access list to the app's code
signature. Without a stable signature, every rebuild looks like a new app, and macOS asks for
both again. A self-signed certificate fixes that and costs nothing:

1. Open Keychain Access, then Keychain Access > Certificate Assistant > Create a Certificate…
2. Name: `uSnip Local`. Identity Type: Self Signed Root. Certificate Type: Code Signing.
3. Pass the name to CMake as `USNIP_CODESIGN_IDENTITY` (see Build). The app is signed after
   every build.

Notarization is not needed for an app you build yourself.

## First launch

- macOS asks for **Accessibility** permission (System Settings > Privacy & Security >
  Accessibility). It is needed to send Cmd+V. Without it, μSnip only copies.
- The encryption key is created in your login keychain. If you rebuild with a different
  signature, macOS asks whether μSnip may read it again; choose Always Allow.
- Data lives in `~/Library/Application Support/uSnip/snippets.bin`.

## Adding snippets

Until the editor exists, use **Import JSON…** in the menu bar menu with a file like:

```json
[
  {"title": "staging basic auth", "body": "user:password"},
  {"title": "prod mysql", "body": "s3cret"}
]
```

The file is plain text. μSnip offers to move it to the Trash after importing.

## License

Not chosen yet (GPL planned).
