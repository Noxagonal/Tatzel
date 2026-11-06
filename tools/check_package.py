#!/usr/bin/env python3
"""Install, relocate, and exercise a consumer without the framework checkout."""

import argparse
import hashlib
import os
from pathlib import Path
import shutil
import subprocess


def run(arguments: list[str], **options) -> None:
    """@brief Execute one visible validation step and fail on a nonzero exit.
    @param arguments Exact command arguments, never shell-expanded.
    @param options Additional subprocess options.
    @pre Commands are trusted development tools.
    @post The command has completed successfully or raised an error.
    """
    print("+ " + " ".join(map(str, arguments)), flush=True)
    subprocess.run(arguments, check=True, **options)


def hashes(directory: Path) -> dict[str, str]:
    """@brief Read a complete runtime file manifest.
    @param directory Existing runtime directory.
    @return Relative file paths mapped to content hashes.
    @pre Files are readable and no build is mutating them.
    @post Files are unchanged.
    """
    return {str(path.relative_to(directory)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in directory.rglob("*") if path.is_file()}


def main() -> None:
    """@brief Verify the documented installation and independent consumer workflow.
    @pre A complete framework build and its dependencies are available.
    @post A relocated consumer has passed the real HTTP/WebSocket package checks.
    """
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-directory", required=True, type=Path)
    parser.add_argument("--work-directory", required=True, type=Path)
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--dependency-prefix", action="append", default=[])
    parser.add_argument("--parallel", default="2")
    options = parser.parse_args()
    repository = Path(__file__).resolve().parents[1]
    build = options.build_directory.resolve()
    work = options.work_directory.resolve()
    if work.exists():
        raise SystemExit("Use a new work directory; the package check never deletes existing work")
    if work.is_relative_to(repository) or work.is_relative_to(build):
        raise SystemExit("The package check work directory must be outside the checkout and build")
    work.mkdir(parents=True)
    original = work / "original install"
    prefix = work / "relocated package"
    source = work / "consumer source"
    consumer_build = work / "consumer build"
    cmake = shutil.which("cmake")
    if cmake is None:
        raise SystemExit("CMake is required")

    run([cmake, "--install", str(build), "--prefix", str(original)])
    original.rename(prefix)
    examples = list(prefix.glob("**/tatzel/examples/installed/CMakeLists.txt"))
    runtimes = list(prefix.glob("**/tatzel/runtime/data/template.html"))
    if len(examples) != 1 or len(runtimes) != 1:
        raise SystemExit("The installed package must include exactly one sample and runtime")
    runtime = runtimes[0].parents[1]
    shutil.copytree(examples[0].parent, source)
    package_paths = ";".join([str(prefix), *options.dependency_prefix])
    run([cmake, "-S", str(source), "-B", str(consumer_build), "-G", "Ninja",
         "-DCMAKE_CXX_COMPILER=" + options.compiler, "-DCMAKE_BUILD_TYPE=Release",
         "-DCMAKE_PREFIX_PATH=" + package_paths, "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
         "-DCMAKE_FIND_USE_PACKAGE_REGISTRY=OFF", "-DCMAKE_FIND_USE_SYSTEM_PACKAGE_REGISTRY=OFF"])
    for forbidden in (repository, build, original):
        if str(forbidden) in (consumer_build / "build.ninja").read_text():
            raise SystemExit("Consumer build refers to a checkout, original install, or framework build: " + str(forbidden))
    if "NPM_EXECUTABLE" in (consumer_build / "CMakeCache.txt").read_text():
        raise SystemExit("An installed-package consumer must not configure the browser toolchain")
    run([cmake, "--build", str(consumer_build), "--parallel", options.parallel])
    deployed_runtime = consumer_build / "tatzel-runtime"
    if hashes(runtime) != hashes(deployed_runtime):
        raise SystemExit("Deployed browser/template assets differ from the installed package")

    # An asset-only update must deploy even when no executable needs relinking.
    stylesheet = runtime / "www/framework/connection.css"
    stylesheet.write_text(stylesheet.read_text() + "\n/* package deployment refresh check */\n")
    run([cmake, "--build", str(consumer_build), "--parallel", options.parallel])
    if hashes(runtime) != hashes(deployed_runtime):
        raise SystemExit("Asset-only changes did not refresh the deployed runtime")

    deployed = work / "deployed application"
    deployed.mkdir()
    executable = deployed / "TatzelInstalledExample"
    shutil.copy2(consumer_build / "TatzelInstalledExample", executable)
    shutil.copytree(deployed_runtime, deployed / "tatzel-runtime")
    # The executable-adjacent copy must suffice even if the installed runtime goes away.
    runtime.rename(work / "unavailable installed runtime")
    empty = work / "empty working directory"
    empty.mkdir()
    environment = os.environ.copy()
    environment.pop("TATZEL_RESOURCE_DIRECTORY", None)
    run(["node", str(repository / "tests/browser/package-smoke.mjs"), str(executable), str(empty)], env=environment)
    print("Installed package passed: relocation, separate consumer, matching assets, independent runtime, and path overrides")


if __name__ == "__main__":
    main()
