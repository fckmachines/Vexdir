# Vexdir

A minimal, single-pane, **read-only** terminal file navigator inspired by classic FAR-style interfaces.

**Version:** 0.1.0 | **Language:** C++17 | **UI:** FTXUI 7.0.3

## Features

- Navigate directories using the keyboard
- Directories first, then files (case-insensitive names)
- Displays file sizes and the current path
- Refresh the directory listing on demand
- No copy, move, rename, delete, or edit commands
- Linux and Windows terminal support

## Controls

| Key | Action |
| --- | --- |
| Up / Down | Move selection |
| PageUp / PageDown | Move 10 items |
| Home / End | First / last item |
| Enter | Open selected directory |
| Backspace / Left | Parent directory |
| F5 / R | Refresh |
| Esc / Q | Exit |

Regular files are displayed but are not opened in v0.1.0.

## Build

Requirements: CMake 3.20+, Git, C++17 compiler, internet access at configure time to fetch FTXUI.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Run `./build/vexdir` on Linux, or `build\\Release\\vexdir.exe` when using Visual Studio on Windows.

GitHub Actions builds Linux x86-64 and Windows x64 on every push to `main`. On the initial successful build, it publishes release `v0.1.0` with platform ZIP files.

## Notes

"Read-only" refers to available Vexdir operations. The program uses filesystem metadata reads and directory enumeration only; it does not enforce OS-level permission restrictions.

FTXUI is fetched at the pinned tag `v7.0.3`. Third-party dependency is not vendored.

## License

MIT.
