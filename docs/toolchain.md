# Toolchain

Pinned tool versions for DSLab. `scripts/check-env.sh` reads the `emsdk:` line
below and fails if `em++ --version` reports a different version. CI will pin the
same version later.

emsdk: 6.0.13

## Emscripten

Installed on 2026-10-22 with `./emsdk install latest` and `./emsdk activate latest`
on Ubuntu 24.04.5 LTS, WSL 2, aarch64. On ARM64 Linux, `latest` can trail x64,
so always pin the number above rather than `latest`.

Exact first line of `em++ --version`:

    emcc (Emscripten gcc/clang-like replacement + linker emulating GNU ld) 6.0.13 (e13ee988ce3d87a738dd8f3661d02223308c412a)

- emsdk location: `~/tools/emsdk` (or `$EMSDK`), emsdk checkout `d7b4130` (2026-10-20)
- SDK: `sdk-releases-eb132757325583b787fe9414b09afea9442b72ad-64bit`
- Node bundled with emsdk: 24.19.0 (used by `em++` only; your shell uses nvm's Node)

To reproduce elsewhere (CI included):

    ./emsdk install 6.0.13
    ./emsdk activate 6.0.13

## Other tools (as found on 2026-10-22)

| Tool | Version |
| --- | --- |
| g++ | 13.3.0 |
| cmake | 3.28.3 |
| ninja | 1.11.1 |
| Node (nvm default) | 24.22.1 |
| nvm | 0.40.8 |
| Playwright Chromium | chromium-1248 |

## Pinned project dependencies (milestone 2, 2026-10-22)

Exact versions in `package.json`; `package-lock.json` pins the rest of the tree.

| Package | Version |
| --- | --- |
| react, react-dom | 19.3.1 |
| @types/react, @types/react-dom | 19.3.0 |
| vite | 8.3.5 |
| @vitejs/plugin-react | 6.1.2 |
| typescript | 7.0.2 |

Vendored C++ headers (details and SHA-256 in `engine/third_party/VERSION.md`):
nlohmann/json v3.12.0 and doctest v2.5.3.
