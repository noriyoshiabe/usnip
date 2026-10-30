# Coding style

`.clang-format` encodes the formatting rules. Run `clang-format -i` on every file you touch
(`.h`, `.cpp`, `.mm`).

## Formatting

- C++20. 4-space indent, 100 columns.
- The brace style is Stroustrup.
  Function bodies open `{` on a new line, in and out of a class.
  Other blocks open `{` on the same line.
  `else` and `catch` start a new line.
- Every `if`, `for` and `while` body has braces, even a single statement.

```cpp
bool SnippetStore::update(std::size_t index, const QString &title, const SecureBytes &body)
{
    if (index >= m_snippets.size()) {
        return false;
    }
    else {
        ...
    }
}
```

## Naming

- Types `PascalCase`, functions and variables `camelCase`, members `m_name`,
  constants `kName`, namespaces lower case (`usnip::crypto`).
- Files are named after their main type. Objective-C++ files use `.mm`.

## Structure

- `src/core` is plain C++ on QtCore and CommonCrypto: no widgets, no Objective-C.
  New logic goes here when it can, with tests in `tests/`.
- `src/platform` wraps macOS APIs. Headers are plain C++ (no Objective-C types, no Carbon
  headers); `.mm` files compile with ARC.
- `src/app` is the Qt UI and glue. It talks to macOS only through `src/platform`.
- Errors are return values (`bool`, `std::optional`), not exceptions. The only exception is
  a failing system random number generator, which aborts the operation.

## Handling secrets

- Snippet bodies, keys and passphrases live in `SecureBytes`, which wipes itself.
  Do not copy them into `QString`, `QByteArray` or `std::string` unless an API leaves no
  choice, and wipe such copies (`secureWipe`) when possible.
- Never log, print or put a title or body into an error message, assertion or crash report.
- Decrypt as late as possible and drop the plain text right after use.
- Do not add third-party libraries. Use Qt and what ships with macOS.

## Tests

- Qt Test, one executable per test class, registered in `tests/CMakeLists.txt`.
- Any change to `src/core` comes with a test. File formats need round-trip and tamper tests.
