# Vexdir

A minimal single-pane, **read-only** terminal directory navigator inspired by classic FAR-style interfaces.

**Version:** 0.1.1 | **Language:** C++17 | **Dependencies:** none (standard C++ and operating-system console APIs only).

## Features

- Read-only directory navigation and file listing
- Directory-first name sorting, file size display, current path, error reporting
- Windows x64 and Linux x86-64 terminal support
- No copy, move, rename, delete, edit, or file opening operations
- On Windows, MSVC CRT is linked statically using `/MT`; `msvcp140.dll` is not required

## Keys

| Key | Action |
| --- | --- |
| Up / Down | Move selection |
| PageUp / PageDown | Move a page |
| Home / End | First / last entry |
| Enter / Right | Enter directory |
| Backspace / Left | Parent directory |
| F5 / R | Refresh |
| Esc / Q | Exit |

## Build

Requires CMake 3.20+ and a C++17 compiler; does not download dependencies.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel
```

Linux binary: `build/vexdir`. Windows Visual Studio binary: `build/Release/vexdir.exe`.

The GitHub Actions workflow compiles both versions and publishes uncompressed executable files (`vexdir` and `vexdir.exe`) in release `v0.1.1`. It additionally checks the Windows PE dependency list for dynamically loaded Microsoft C/C++ runtimes.

## Terminal requirements

An interactive terminal with ANSI/VT sequences enabled is required. On Windows, use Windows Terminal, PowerShell, or a compatible modern console host.

The read-only feature limits operations within Vexdir; OS permissions remain the ultimate security boundary.

## License

MIT.
