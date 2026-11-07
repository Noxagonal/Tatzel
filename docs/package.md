# Install and use the CMake package

Tatzel installs a compiled, versioned shared library and its public C++ module
interfaces. Applications use `find_package(Tatzel CONFIG REQUIRED)`, link
`Tatzel::Tatzel`, and `import UI;`. They generate their own module metadata from
the installed interfaces; they do not rebuild Tatzel's private implementation.

## Build contract

The documented baseline is Linux, CMake 4.0+, Ninja 1.11+, Clang 23+, and C++26.
CMake uses the compiler selected by the caller, including `CXX` or
`CMAKE_CXX_COMPILER`. Tatzel does not reject a compiler just because its version
is older, select a different compiler, or install build tools. Usable named
modules and a working standard-library `std::stacktrace` implementation are
required. CMake probes the stacktrace support library when building Tatzel.

Use a compatible compiler, target platform, standard library, and ABI when
building the package and its applications. Regenerating module metadata does
not make a prebuilt C++ library portable across incompatible ABIs.

The default build embeds static Drogon 1.9.13, its Trantor submodule, JsonCpp
1.9.6, zlib, and UUID support in Tatzel. Backend headers, targets, and private
module sources are not installed. OpenSSL and the normal C/C++ system runtimes
remain shared deployment dependencies. Database, Redis, YAML, Brotli, and c-ares
integrations are disabled. SQLite belongs only to the optional Workspace sample.

Preparing the package requires Git/network access, OpenSSL development files,
make, a PIC static zlib library, UUID development files for configure-time API
probes, and Node.js 24+/npm to build the browser runtime. UUID is built privately
from util-linux 2.40.2 with PIC. No Autotools installation is needed because the
upstream release includes its configure script. Missing native build dependencies
must be supplied by the packager. Consuming the installed package requires no Node/npm, backend headers,
Drogon/Trantor/JsonCpp installation, or database development package.

## Build and install Tatzel

Use your existing compiler and tools. From a Tatzel checkout:

```sh
cmake -S . -B build/package -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DTATZEL_BUILD_DEVELOPMENT=OFF -DTATZEL_BUILD_WORKSPACE=OFF \
  -DCMAKE_INSTALL_PREFIX="$PWD/install/tatzel"
cmake --build build/package --parallel 2
cmake --install build/package
```

To choose a compiler explicitly, add `-DCMAKE_CXX_COMPILER=clang++` to the initial
configure command, substituting the compiler already available on your system.
Build before installing so the matching browser assets are present.

`TATZEL_USE_SYSTEM_DROGON=ON` is an alternative for packagers deliberately using
an installed compatible Drogon package. That choice may retain its shared
runtime dependencies; the default bundled-dependency checks apply to the default
build. An embedding project can also supply its own `Drogon::Drogon` target.

| Installed location | Contents |
| --- | --- |
| lib/libTatzel.so* | Versioned shared library, SONAME libTatzel.so.0. |
| include/tatzel | Public .ixx interfaces, including definitions required by templates. |
| lib/cmake/Tatzel | Imported target, module metadata, version/configuration files, deployment helper. |
| share/tatzel/runtime | Matching browser JavaScript, framework CSS/images, HTML template, optional backend configuration. |
| share/tatzel/examples/installed | Independent sample project and README. |
| share/doc/Tatzel | This guide, license, and dependency notices. |

The library directory follows GNUInstallDirs and can be lib64 or another
configured path. You may relocate the entire installation before configuring
an application. Reconfigure an existing application if its package prefix moves.

## Use the installed sample

Copy `share/tatzel/examples/installed` from the installation into another working
directory. The copied project has no dependency on the framework checkout:

```sh
cmake -S /path/to/copied/sample -B sample-build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH=/absolute/path/to/tatzel-install
cmake --build sample-build --parallel 2
./sample-build/TatzelInstalledExample
```

Open http://localhost:8080/. Try the counter, greeting, and background-task button.
Each browser has separate logical state. The background callback returns its UI
update through the page's Dispatcher and checked element handle.

For your own application:

```cmake
cmake_minimum_required(VERSION 4.0)
project(MyApplication LANGUAGES CXX)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Tatzel 0.1 CONFIG REQUIRED)
add_executable(MyApplication Main.cpp)
target_link_libraries(MyApplication PRIVATE Tatzel::Tatzel)
tatzel_deploy_runtime(MyApplication)
```

The imported target propagates the C++26 requirement. Your `Main.cpp` can import
UI, construct `tatzel::App`, register pages, and call Run, as the sample does.

## Deploy the executable, library, and browser runtime

`tatzel_deploy_runtime` copies the shared library under its SONAME next to the
executable and refreshes its `tatzel-runtime` directory on every build, including
asset-only changes. On Linux it adds an executable-relative library search path.

Move these together:

- The application executable.
- `libTatzel.so.0` from the executable's build directory.
- The complete adjacent `tatzel-runtime` directory.

The deployed application can run from an unrelated working directory after the
original installation is unavailable. The destination still needs compatible
OpenSSL and C/C++ system runtimes. Alternatively, manage the library and its
search path through your normal system packaging/deployment process.

Deploy the browser runtime from the same Tatzel installation as the library.
Protocol versions must match; do not combine assets from another checkout.

## Application options

`AppOptions` owns listener address, port, server thread count, log verbosity, and
resource paths. Normal startup needs no backend-specific configuration file:

```cpp
auto options = tatzel::AppOptions{};
options.listen_address = "127.0.0.1";
options.port = 8080;
options.thread_count = 2;
options.log_level = tatzel::LogLevel::Info;
auto app = tatzel::App{ std::move( options ) };
```

Resource discovery selects the first available source in this order:

1. A nonempty `AppOptions::resource_directory`.
2. A nonempty `TATZEL_RESOURCE_DIRECTORY` environment variable.
3. An adjacent `tatzel-runtime` directory, resolved from `/proc/self/exe` on Linux.
4. The configured framework build runtime as a development fallback.

An explicitly selected missing directory is an error. `template_path` defaults
to `data/template.html`; `document_root` defaults to `www`. Relative paths resolve
against the selected resource directory. `configuration_path` is empty by
default and optionally loads advanced Drogon configuration. Explicit Tatzel
listener/thread/log settings take precedence for the settings they control.
The existing App(configuration_path, template_path) constructor remains available
for applications using explicit legacy file paths.

## Package verification

After building with `TATZEL_BUILD_TESTS=ON`, run:

```sh
ctest --test-dir build/package --output-on-failure
python3 tools/check_package.py --build-directory build/package \
  --work-directory /tmp/tatzel-package-check \
  --compiler /absolute/path/to/the/selected/compiler
```

Use a new work directory. The package check verifies hidden native dependencies,
installed public interfaces, a relocated prefix containing spaces, a separate
consumer, dynamic linkage without private-source compilation, asset refresh,
and deployment after the installed library/runtime directories become
unavailable. HTTP/WebSocket checks exercise UI actions, background work, resource
selection, and explicit-path failure.
