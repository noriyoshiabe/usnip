# AGENTS.md

μSnip is a minimal macOS menu bar app that pastes encrypted snippets (Qt 6, C++20, Objective-C++).

- What and why: [docs/design.md](docs/design.md)
- What to do next: [docs/roadmap.md](docs/roadmap.md). Work phase by phase and tick items as they land.
- How to write code: [docs/coding-style.md](docs/coding-style.md) (Stroustrup braces, secret handling)
- How to build, sign and run: [README.md](README.md)

Before committing:

```sh
cmake --build build && ctest --test-dir build --output-on-failure
clang-format -i <changed files>
```

Never put snippet titles, bodies or keys in logs, test output or commit messages.
Do not add third-party dependencies.
