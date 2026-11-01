# Build and run

## Requirements

The project declares these build requirements:

- CMake 4.0 or newer.
- A compiler accepting C++26 mode and usable named C++ modules.
- A CMake generator supporting module dependency scanning; use Ninja.
- Node.js 24 or newer and npm.
- Git and network access for CMake's pinned Drogon dependency and npm packages.
- JsonCpp development files and the platform dependencies required by Drogon.
  OpenSSL and zlib development packages enable the corresponding optional support.
- A working std::stacktrace implementation, including its link support library.
  The CI toolchain installs libstdc++-13-dev; CMake probes native linkage,
  stdc++exp, and stdc++_libbacktrace and reports unsupported toolchains at configure time.

Drogon is fetched at v1.9.13. TypeScript is installed from package-lock.json.
The Node package exists to build browser assets and is private.

The complete application and core checks pass in CI on Ubuntu 24.04 with
Clang 18.1.3, CMake 4.4.4, Ninja, and libstdc++. Select the compiler explicitly when configuring;
other platforms and compiler combinations still need verification. The build
uses explicit includes rather than a shared precompiled header, avoiding the
PCH/module definition conflicts found with this toolchain.

## Configure and build

Run from the repository root. Replace clang++-18 with your selected compiler:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++-18
cmake --build build --target TatzelDevelopment
```

CMake installs the locked npm dependencies, compiles browser code from
targets/typescript_build/src into www/app, and builds the static Tatzel library
and development executable.

TypeScript files and generated JavaScript outputs are build dependencies.
Changing a source file or deleting generated browser files requires regeneration
on the next build. Generated assets and dependency directories are ignored by Git.

For browser-only work:

```sh
npm ci
npm run build
npm run watch
```

## Run

Keep the working directory at the repository root:

```sh
./build/targets/development/TatzelDevelopment
```

Open http://localhost:8080/. The example builds a per-client counter with a
logical heading, nested container, and button, plus a profile card demonstrating
text edits, checkbox changes, explicit reset values, and a modal preview.
Open http://localhost:8080/forms for the row/column layout and validated form
example, including choice updates and multiline editing.
Open http://localhost:8080/lifecycle for dynamic creation, removal, child
clearing, checked references, modal disposal, and a callback throwing a traced exception.
Startup and shutdown messages use the shared INFO logger. See
[logging and exception traces](diagnostics.md) for verbosity and file-output configuration.

The runtime currently resolves these paths relative to the working directory:

| Path | Purpose |
| --- | --- |
| data/drogon_config.json | Drogon configuration. |
| data/template.html | Initial browser document. |
| www | Static document root. |
| www/app/main.js | Generated browser entry module. |
| www/framework/layout.css | Local layout rules that cooperate with Pico defaults. |

The default listener binds 0.0.0.0:8080. The template uses Pico CSS from a CDN,
so external stylesheet access is needed for its default presentation.
Runtime asset packaging and configurable resource roots remain future work.

## Checks

```sh
npm test
python3 tools/check_documentation.py
python3 tools/build_documentation.py
```

The documentation commands require Python 3.10 or newer and Doxygen 1.9.8 or
newer, and do not require compiling Tatzel. The browser tests use Node's built-in
TypeScript stripping to exercise runtime behavior; npm run build supplies the
TypeScript type check.

Core C++ behavior checks can be built with the application toolchain:

```sh
cmake -S . -B build -DTATZEL_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
node tests/browser/server-smoke.mjs ./build/targets/development/TatzelDevelopment
node tests/browser/forms-smoke.mjs ./build/targets/development/TatzelDevelopment
node tests/browser/removal-smoke.mjs ./build/targets/development/TatzelDevelopment
```

These checks include logging/exception reports and an independently optimized
diagnostic-module target with NDEBUG and debug symbols, even in a Debug build.

The Clang/Linux build can also instrument the logical removal tests:

```sh
cmake -S . -B build -DTATZEL_BUILD_TESTS=ON -DTATZEL_BUILD_SANITIZER_TESTS=ON
cmake --build build --target TatzelRemovalSanitizerTests
ctest --test-dir build -R TatzelRemovalSanitizerTests --output-on-failure
```

This target compiles the actual logical C++ modules with AddressSanitizer and
UndefinedBehaviorSanitizer, independently of the server dependency. It requires
the compiler's sanitizer runtime (libclang-rt-18-dev for the CI toolchain). CI
runs with leak detection enabled. In restricted executors where LeakSanitizer
cannot inspect /proc, ASAN_OPTIONS=detect_leaks=0 can run address/undefined checks
while omitting leak scanning; that does not replace the CI leak check.

There is no installable CMake package yet. The current executable and source
layout are intended for working within this checkout.
