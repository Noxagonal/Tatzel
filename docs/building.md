# Build and run

## Requirements

The project declares these build requirements:

- CMake 4.0 or newer.
- A compiler accepting C++26 mode and usable named C++ modules; Clang 23+ is the documented baseline.
  The caller selects the compiler; there is no compiler-version gate or tool bootstrap.
- A CMake generator supporting module dependency scanning; use Ninja.
- Node.js 24 or newer and npm.
- Git and network access for CMake's pinned Drogon dependency and npm packages.
- SQLite development files for the default-enabled workspace sample (`libsqlite3-dev` on Ubuntu).
- OpenSSL development files, a PIC static zlib library, UUID development
  files for configure-time probes, and make for the private upstream UUID build.
  JsonCpp is fetched and built privately; consumers do not need backend development packages.
- A working std::stacktrace implementation, including its link support library.
  CMake probes native linkage,
  stdc++exp, and stdc++_libbacktrace and reports unsupported toolchains at configure time.

Drogon is fetched at v1.9.13 and JsonCpp at 1.9.6. TypeScript is installed from package-lock.json.
The Node package exists to build browser assets and is private.

Use a compatible compiler/standard-library ABI for the package and consumer.
Compiler versions alone do not establish binary compatibility.
Other platforms and compiler combinations still need verification. The build
uses explicit includes rather than a shared precompiled header, avoiding the
PCH/module definition conflicts found with this toolchain.

## Configure and build

Run from the repository root with your selected compiler:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target TatzelDevelopment
```

CMake installs the locked npm dependencies, compiles browser code from
targets/typescript_build/src into www/app, and builds the shared Tatzel library
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

From the repository root:

```sh
./build/targets/development/TatzelDevelopment
```

Open http://localhost:8080/ for a list of links to all showcases.
Open http://localhost:8080/controls for a per-client counter with a logical
heading, nested container, and button, plus a profile card demonstrating
text edits, checkbox changes, explicit reset values, and a modal preview.
Open http://localhost:8080/forms for the row/column layout and validated form
example, including choice updates and multiline editing.
Open http://localhost:8080/elements for numeric input, radio groups, native links,
and images, including coordinated validation and explicit resets.
Open http://localhost:8080/lifecycle for dynamic creation, removal, child
clearing, checked references, modal disposal, and a callback throwing a traced exception.
Startup and shutdown messages use the shared INFO logger. See
[logging and exception traces](diagnostics.md) for verbosity and file-output configuration.
Open http://localhost:8080/dispatcher for background form completion, removal
before completion, disconnection cancellation, and dispatched error recovery.
Open http://localhost:8080/protocol for batched construction, guarded delayed
normalization, authoritative reset, and recoverable failure; /failed-page
demonstrates terminal initialization failure.
See [posting work to a logical page](dispatcher.md) for the API and lifetime rules.

Main.cpp handles startup and shutdown; individual showcase builders and their
shared route/index list live under targets/development/private/showcases.

The checkout contains these source assets and browser build outputs:

| Path | Purpose |
| --- | --- |
| data/drogon_config.json | Drogon configuration. |
| data/template.html | Initial browser document. |
| www | Static document root. |
| www/app/main.js | Generated browser entry module. |
| www/framework/layout.css | Local semantic row/column layout rules. |
| www/framework/components.css | Local control/theme tokens and responsive component styling. |

The default listener binds 0.0.0.0:8080. The default template loads local layout, component-theme, and connection styles;
it does not need an external stylesheet service.
Framework runtime assets are staged under build/runtime. App defaults also discover
an adjacent tatzel-runtime directory on Linux; tatzel_deploy_runtime creates this
for the development executable. See [package installation and usage](package.md)
for independent applications, resource configuration, and deployment.

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
node tests/browser/elements-smoke.mjs ./build/targets/development/TatzelDevelopment
node tests/browser/removal-smoke.mjs ./build/targets/development/TatzelDevelopment
node tests/browser/errors-smoke.mjs ./build/targets/development/TatzelDevelopment
node tests/browser/dispatcher-smoke.mjs ./build/targets/development/TatzelDevelopment
node tests/browser/reliability-smoke.mjs ./build/targets/development/TatzelDevelopment
npx playwright install chromium
node tests/browser/reliability-ui.mjs ./build/targets/development/TatzelDevelopment
```

These checks include concurrent dispatcher producers, shutdown races, logging/exception reports, and an independently optimized
diagnostic-module target with NDEBUG and debug symbols, even in a Debug build.

The Clang/Linux build can also instrument the logical removal and dispatcher tests:

```sh
cmake -S . -B build -DTATZEL_BUILD_TESTS=ON -DTATZEL_BUILD_SANITIZER_TESTS=ON
cmake --build build --target TatzelRemovalSanitizerTests TatzelDispatcherSanitizerTests
ctest --test-dir build -R 'Tatzel(Removal|Dispatcher)SanitizerTests' --output-on-failure
```

These targets compile the actual logical and dispatcher C++ modules with
AddressSanitizer and UndefinedBehaviorSanitizer. The dispatcher check links the
real Trantor event loop through Drogon. The instrumentation requires
the compiler's sanitizer runtime from the selected compiler toolchain. CI
runs with leak detection enabled. In restricted executors where LeakSanitizer
cannot inspect /proc, ASAN_OPTIONS=detect_leaks=0 can run address/undefined checks
while omitting leak scanning; that does not replace the CI leak check.

Use the [CMake package guide](package.md) to install Tatzel and build a separate
application with find_package(Tatzel). Its sample is also installed with the package.

## Persistent application sample

Build and run `TatzelWorkspace` to use the project/task manager. It has its own
local stylesheet and template, persistent SQLite data, and SPA navigation.
See the [workspace guide](workspace.md). Set `TATZEL_BUILD_WORKSPACE=OFF` to omit
this executable and its SQLite development dependency.

The reliability checks cover batch limits/order and failure flushing, stale field
and form identities, delayed server frames with live browser edits/caret,
composition cancellation, readiness gating, recoverable errors, and fatal reload.
Legacy control smoke suites flatten batches through a test-only ProtocolSocket;
reliability-smoke uses unmodified native WebSocket frames to verify framing.

Open http://localhost:8080/components for independent filter/editor instances,
confirmation outcomes, asynchronous save failures, authoritative loads, and scoped
theme switching. See [components and themes](components.md).

```sh
node tests/browser/components-ui.mjs ./build/targets/development/TatzelDevelopment
```

Open http://localhost:8080/tables in TatzelDevelopment for sortable rows,
native selection, incremental add/update/delete, editable control cells, and
loading/empty states. See [tables and stable rows](tables.md).

```sh
node tests/browser/tables-ui.mjs ./build/targets/development/TatzelDevelopment
```
