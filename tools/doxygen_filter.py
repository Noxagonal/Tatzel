#!/usr/bin/env python3
"""Normalize C++ module syntax for Doxygen while preserving source line numbers."""

import re
import sys
from pathlib import Path


def filter_source(source: str) -> str:
    """@brief Remove module declarations/imports and export keywords for Doxygen.
    @param source Original C++ module source.
    @return Parser-compatible text with the same number of lines.
    @pre source is a Tatzel module interface.
    @post The original source string is unchanged.
    """
    result = []
    for line in source.splitlines(keepends=True):
        if re.match(r"^\s*(?:export\s+)?(?:module|import)\b.*;", line):
            result.append("\n" if line.endswith("\n") else "")
        elif re.match(r"^\s*export\s*$", line):
            result.append("\n" if line.endswith("\n") else "")
        else:
            result.append(re.sub(r"^(\s*)export\s+", r"\1", line))
    return "".join(result)


if __name__ == "__main__":
    sys.stdout.write(filter_source(Path(sys.argv[1]).read_text()))
