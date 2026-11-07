# Installed Tatzel sample

This is a separate CMake project. It uses only `find_package(Tatzel)` and
`Tatzel::Tatzel`; it does not include Tatzel's checkout or fetch the framework.
It demonstrates a counter, text input, logical callbacks, and per-browser state, and background work returned through Dispatcher.

First install Tatzel using `docs/package.md` in the checkout. This sample is also
installed under `share/tatzel/examples/installed`; the installed package guide is
`share/doc/Tatzel/package.md` in the same prefix.

Copy this directory outside the installation and run the commands from the copy.
Set `CMAKE_PREFIX_PATH` to the installation root containing `lib` and `share`.

```sh
cmake -S . -B build -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="/absolute/path/to/tatzel-install"
cmake --build build --parallel 2
./build/TatzelInstalledExample
```

Open http://localhost:8080/. Each browser connection has its own counter and name.
The sample listens on port 8080, so stop other Tatzel examples first.

`tatzel_deploy_runtime` copies the framework runtime to `build/tatzel-runtime`
on every build. It also copies `libTatzel.so.0` next to the executable. Move the executable,
that library, and the runtime directory together to deploy them;
they can run from an unrelated working directory. Linux executable discovery
uses `/proc/self/exe`, so launching through a symlink also finds the real
executable's adjacent runtime. OpenSSL and the C/C++ system runtimes remain deployment requirements.
CMake 4.0+, Clang 23+, and Ninja are the documented baseline. Your selected
compiler is used without automatic tool installation or a compiler-version gate.

The optional first argument selects an explicit runtime root:

```sh
./build/TatzelInstalledExample /absolute/path/to/custom-runtime
```

Alternatively set `TATZEL_RESOURCE_DIRECTORY`. The explicit argument takes
precedence. A custom runtime has the same `data` and `www` layout as the deployed
runtime; the package guide documents the corresponding
`AppOptions` paths for your own application.
