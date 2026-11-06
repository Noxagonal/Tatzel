# Install and use the CMake package

Tatzel 0.1 installs a CMake package exposing Tatzel::Tatzel. A separate application
finds that package, links the target, imports UI, and deploys the matching runtime
through one CMake helper. The installed sample demonstrates the complete workflow.

## Supported build contract

The CI configuration validates Ubuntu 24.04, Clang 18.1.3, libstdc++ 13, CMake
4.4.4, Ninja, and C++26 mode. CMake 4.0 is the declared minimum. Use a matching
compiler and standard library when preparing and consuming an installation.
Other compilers/platforms are not yet part of the package compatibility promise.
A working std::stacktrace implementation and its support library are required;
configuration checks this before building Tatzel.

The package installs module interface and implementation sources. Finding it
creates a normal library target that the consuming project compiles with its
own toolchain and build configuration. It does not import prebuilt module files
or link the framework build's archive. This costs one framework compilation in
each consumer build, and avoids depending on compiler-specific module artifacts.
Drogon and Trantor remain native binary dependencies and need a compatible ABI.

Preparing the package requires Node.js 24+ and npm to build the locked browser
sources. Consuming the installed package does not require Node.js or npm.
The browser output, framework CSS/images, default HTML template, and default
Drogon configuration are installed together. Server and browser protocol versions
must match; deploy the runtime from the same installation used by the application.

## Prepare and install

On Ubuntu 24.04, install the compiler and system development packages:

```sh
sudo apt-get update
sudo apt-get install clang-18 clang-tools-18 libstdc++-13-dev \
  libjsoncpp-dev libssl-dev zlib1g-dev uuid-dev python3-venv
python3 -m venv ~/tatzel-tools
. ~/tatzel-tools/bin/activate
python3 -m pip install 'cmake==4.4.4' ninja
```

Make Node.js 24 or newer and npm available on PATH as well. SQLite is needed
only when building Workspace; the following package-only build omits it.

From a Tatzel checkout:

```sh
cmake -S . -B build/package -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++-18 -DCMAKE_BUILD_TYPE=Release \
  -DTATZEL_BUILD_DEVELOPMENT=OFF -DTATZEL_BUILD_WORKSPACE=OFF \
  -DCMAKE_INSTALL_PREFIX="$PWD/install/tatzel"
cmake --build build/package --parallel 2
cmake --install build/package
```

By default this fetches pinned Drogon 1.9.13 and its Trantor dependency. Their
libraries, headers, and CMake packages are installed into the same prefix.
System dependencies such as OpenSSL, Jsoncpp, zlib, and UUID are not bundled;
keep their development packages available when building a consumer.
TATZEL_USE_SYSTEM_DROGON=ON selects an existing Drogon CMake package instead.
Build before installing so the compiled browser runtime is present.

The installation includes:

| Location | Contents |
| --- | --- |
| lib/cmake/Tatzel (or the configured library directory) | Package/version configuration and helper functions. |
| share/tatzel/source | Public module interfaces and private implementation sources. |
| share/tatzel/runtime | Matching data/template/configuration, www/app, and www/framework assets. |
| share/tatzel/examples/installed | Standalone sample project, with its own README. |
| share/doc/Tatzel | This guide, the framework license, and fetched dependency notices under licenses. |

The package configuration uses its installation location rather than the original
checkout/build paths. You may relocate the whole prefix before configuring a
consumer. After configuring a consumer, keep its package prefix available for
rebuilds, or reconfigure that consumer against the new location.

## Build the installed sample

Copy share/tatzel/examples/installed from the installation into a separate working
directory. With that directory as the current directory:

```sh
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++-18 \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH=/absolute/path/to/tatzel-install
cmake --build build --parallel 2
./build/TatzelInstalledExample
```

Open http://localhost:8080/. Increment the counter or enter a name and press Greet.
Each connection owns its state. The sample's optional first argument selects a
custom runtime directory; see its README.

## Use the target in your own project

```cmake
cmake_minimum_required(VERSION 4.0)
project(MyApplication LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 26)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Tatzel 0.1 CONFIG REQUIRED)
add_executable(MyApplication Main.cpp)
target_link_libraries(MyApplication PRIVATE Tatzel::Tatzel)
tatzel_deploy_runtime(MyApplication)
```

Select the language standard before creating application targets so CMake's module
scanner has an explicit CXX_STANDARD. The library also propagates its language
and module requirements. Application code can
include the standard headers it uses, then import UI and create tatzel::App.
No framework-private include directories or Drogon target are needed for the
sample. Applications directly calling Drogon APIs should link Drogon::Drogon too.
The original source target Tatzel remains available as well as Tatzel::Tatzel.
Source embedding with add_subdirectory/FetchContent defaults the development and
Workspace executables off when Tatzel is not the top-level project.

## Runtime discovery and deployment

AppOptions::resource_directory chooses the runtime root. With no explicit root,
App checks, in order:

1. A nonempty TATZEL_RESOURCE_DIRECTORY environment variable.
2. An adjacent tatzel-runtime directory next to the real executable on Linux.
3. The configured build or installed runtime directory compiled into the library.

An explicit root takes precedence over every automatic option. Relative explicit
roots use the working directory at App construction and are made absolute.
A missing selected root raises tatzel::Exception; it does not silently fall back.
The Linux adjacent-directory check uses /proc/self/exe. On other platforms,
provide a root explicitly or use the configured fallback; automatic executable
location is currently implemented only for Linux.

tatzel_deploy_runtime(MyApplication) copies the runtime beside that executable
whenever the executable target is built, including asset-only changes. Deploy
the executable together with its adjacent tatzel-runtime directory and required
system shared libraries. The framework can then run from any working directory,
without access to its installation's runtime directory. The helper copies files;
it does not remove application-added files already in that directory.

Customize resource locations in C++:

```cpp
auto app = tatzel::App{ tatzel::AppOptions{
    .resource_directory = "/opt/my-application/runtime",
    .configuration_path = "data/server.json",
    .template_path = "data/application.html",
    .document_root = "www"
} };
```

Each file/directory path can be absolute or relative to the selected root.
An empty configuration_path skips Drogon configuration loading. An empty
document_root preserves the static root from the Drogon configuration; by default
App overrides that root with the resolved runtime/www directory. A custom full
static root must also contain the matching app and framework assets. You may
copy your own assets into the deployed runtime/www directory without changing
framework files. Use GetResourceDirectory to locate application-specific files.

The legacy App(configuration_path, template_path) overload retains explicit
working-directory-relative arguments and the configuration's document root.
Workspace currently uses that overload and its own working-directory-relative
database path. Framework runtime discovery does not choose persistence locations
for applications.

## Verify installation and relocation

After a complete framework build, run:

```sh
python3 tools/check_package.py --build-directory build/package \
  --work-directory /tmp/tatzel-package-check --compiler clang++-18 --parallel 2
```

Choose a new work directory outside the checkout and framework build. The check
installs into a prefix containing spaces, relocates it, copies the installed
sample out, and configures/builds that sample through find_package. It checks
that the consumer build refers to neither the checkout nor framework build,
compares deployed runtime files with the installation, checks asset-only deployment
refreshes, moves the executable,
makes the installed runtime unavailable, and launches from an empty directory.
Actual HTTP/browser-asset requests and WebSocket interactions verify the counter,
text input, environment override, and explicit AppOptions precedence.
The same check runs in CI alongside the existing behavior and browser suites.
