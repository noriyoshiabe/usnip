# μSnip

A minimal macOS menu bar app for pasting secrets such as database passwords from a shortcut.

- Press ⇧⌘B, pick a title, and μSnip pastes the snippet into the app you were using.
- Snippets are encrypted on disk; the key stays in your login keychain.
- The copy is marked as concealed, so clipboard history apps can ignore it, and it is removed
  after 5 seconds, putting back whatever was on the clipboard before.

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
open build/deploy/uSnip.app
```

`build/uSnip.app` keeps using Homebrew's Qt, so later builds do not disturb the bundled copy.

## Signing with a self-signed certificate

macOS ties the Accessibility permission to the app's code signature. With the linker's ad-hoc
signature, every rebuild looks like a new app and needs the permission again. A self-signed
certificate keeps it across rebuilds and costs nothing:

1. Open Keychain Access, then Keychain Access > Certificate Assistant > Create a Certificate…
2. Name: `uSnip Local`. Identity Type: Self Signed Root. Certificate Type: Code Signing.
3. Pass the name to CMake as `USNIP_CODESIGN_IDENTITY` (see Build). The app is signed after
   every build. `security find-identity -p codesigning` lists the certificate as
   `CSSMERR_TP_NOT_TRUSTED`; that does not stop codesign.

The Keychain is different. A certificate without an Apple team ID cannot keep the keychain
item's access: macOS records the build's code hash, so the first launch after each rebuild
asks whether μSnip may read its key. Choose Always Allow.

Notarization is not needed for an app you build yourself.

## First launch

- macOS asks for **Accessibility** permission (System Settings > Privacy & Security >
  Accessibility). It is needed to send Cmd+V. Without it, μSnip only copies.
- The encryption key is created in your login keychain. After each rebuild, macOS asks
  whether μSnip may read it again; choose Always Allow (see Signing).
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
