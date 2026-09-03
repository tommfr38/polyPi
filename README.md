# polyPi

Compute π to millions (web) or billions (desktop) of digits, fast, with a
black/green "counting" visualization.

Two apps, one shared math core:

- **`core/`** — the actual math: Chudnovsky binary splitting on top of GMP,
  written in portable C (`pi_engine.c`). Verified against known reference
  digits of π. Used, unmodified, by both apps below.
- **`web/`** — a browser app. The core is compiled to WebAssembly and runs in
  a Web Worker, capped at 1,000,000 digits (finishes in a couple seconds).
  Single-threaded by design — see "Why the web is capped" below.
- **`desktop/`** — a native C++ app (SDL2 + OpenGL + Dear ImGui) for macOS and
  Windows. No digit cap beyond available RAM/time; a real speed slider picks
  how many threads to split the computation across. Includes a GitHub-releases
  based update checker.

## Why the web is capped

Binary-splitting Chudnovsky parallelizes beautifully across threads, and the
desktop app uses that for real (`desktop/src/pi_worker.cpp` splits the term
range across N `std::thread`s and merges the results). Browsers only get
real multi-threaded WASM behind `SharedArrayBuffer` + cross-origin-isolation
headers, which most static hosting doesn't give you — so the web build stays
single-threaded and the UI locks the speed slider with an explanation,
pointing at the desktop app instead of pretending to offer something it can't
deliver.

## Build & run: web

The compiled WASM (`web/wasm/pi.js` + `pi.wasm`) is committed, so you can just
serve the `web/` directory statically:

```bash
python3 -m http.server 8743 --directory web
```

then open `http://localhost:8743`.

To rebuild the WASM after changing `core/pi_engine.c`:

```bash
brew install emscripten
# GMP has to be cross-compiled for wasm32 once:
curl -sL -o /tmp/gmp.tar.xz https://ftp.gnu.org/gnu/gmp/gmp-6.3.0.tar.xz
mkdir -p tools/wasm_build && tar -xf /tmp/gmp.tar.xz -C tools/wasm_build
cd tools/wasm_build/gmp-6.3.0
source "$(brew --prefix emscripten)/share/emscripten/emsdk_env.sh" 2>/dev/null || \
  source /opt/homebrew/share/emscripten/emsdk_env.sh
emconfigure ./configure --disable-assembly --enable-cxx=no --host=none-none-none CC_FOR_BUILD=clang
emmake make -j8
cd ../../../web
./build_wasm.sh
```

## Build & run: desktop (macOS)

```bash
brew install cmake gmp sdl2
cd desktop
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j8
./build/polyPi
```

## Build & run: desktop (Windows)

Untested by hand here (no Windows machine in this environment) — this is the
path the GitHub Actions workflow (`.github/workflows/build.yml`) uses, and CI
is the first real verification of it:

```powershell
vcpkg install gmp:x64-windows sdl2:x64-windows curl:x64-windows
cmake -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" -A x64
cmake --build build --config Release
```

## Publishing releases / enabling update checks

`desktop/src/config.h` has `POLYPI_GITHUB_REPO` — set it to `owner/repo` once
this is pushed to GitHub, so the in-app "Check for updates" button (and the
automatic startup check) can hit the real releases API. Until then it fails
gracefully with "Couldn't check for updates."

`web/app.js` has `DESKTOP_DOWNLOAD_URL` and `SOURCE_CODE_URL` constants at the
top — point those at the GitHub releases page and repo once published.

## Project layout

```
core/            shared pi-computation engine (C, GMP)
  pi_engine.c/h  binary-splitting Chudnovsky, verified against reference digits
  wasm_bridge.c  thin export surface for the WASM build
web/             browser app (HTML/CSS/JS + WASM)
desktop/         native app (C++, SDL2 + OpenGL + Dear ImGui)
  src/
    pi_worker    multi-threaded compute orchestration
    particles    the pi-glyph particle field
    updater      GitHub-releases update check
    theme        black/green ImGui style
tools/           dev-only: algorithm verification harness, wasm build scratch
```
