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

Drogon is fetched at v1.9.13. TypeScript is installed from package-lock.json.
The Node package exists to build browser assets and is private.

A specific C++26 compiler/version combination has not yet been certified by
repository CI. Browser and documentation checks do not establish that a named
module build works on every compiler. Select your installed module-capable
compiler explicitly when configuring.

## Configure and build

Run from the repository root. Replace clang++ with your selected compiler:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=clang++
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
logical heading, nested container, and button.

The runtime currently resolves these paths relative to the working directory:

| Path | Purpose |
| --- | --- |
| data/drogon_config.json | Drogon configuration. |
| data/template.html | Initial browser document. |
| www | Static document root. |
| www/app/main.js | Generated browser entry module. |

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
cmake --build build --target TatzelCoreTests
ctest --test-dir build --output-on-failure
```

There is no installable CMake package yet. The current executable and source
layout are intended for working within this checkout.
