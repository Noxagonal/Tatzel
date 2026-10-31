#!/usr/bin/env python3
"""Build the static documentation independently of the C++ application toolchain."""

import shutil
import subprocess
import sys
from pathlib import Path


def main() -> int:
    """@brief Check contracts and generate the static Doxygen site.
    @return Zero on success or the failing validation/build exit code.
    @pre Python 3.10 or newer and Doxygen 1.9.8 or newer are installed.
    @post Successful output is available under build/docs/html.
    """
    root = Path(__file__).resolve().parents[1]
    check = subprocess.run([sys.executable, str(root / "tools/check_documentation.py")], cwd=root)
    if check.returncode:
        return check.returncode
    doxygen = shutil.which("doxygen")
    if doxygen is None:
        print("Install Doxygen 1.9.8 or newer to generate the documentation site.", file=sys.stderr)
        return 1
    (root / "build/docs").mkdir(parents=True, exist_ok=True)
    return subprocess.run([doxygen, "Doxyfile"], cwd=root).returncode


if __name__ == "__main__":
    raise SystemExit(main())
