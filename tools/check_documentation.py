#!/usr/bin/env python3
"""Check lightweight Doxygen contracts and local authored-document links.

Doxygen remains the API parser. This check enforces the additional pre/post
contract convention for the project's named declaration style.
"""

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CPP_DOCUMENTATION = r"(?m)^[ \t]*///(?!<)[^\n]*(?:\n[ \t]*///(?!<)[^\n]*)*"


def split_parameters(text: str) -> list[str]:
    """@brief Split function parameters without splitting nested template arguments.
    @param text Text inside a function's outer parameter parentheses.
    @return Individual parameter declarations.
    @pre text has balanced delimiters.
    @post Input text is unchanged.
    """
    depth = 0
    start = 0
    result = []
    for index, character in enumerate(text):
        if character in "<([{":
            depth += 1
        elif character in ">)]}":
            depth -= 1
        elif character == "," and depth == 0:
            result.append(text[start:index].strip())
            start = index + 1
    result.append(text[start:].strip())
    return [parameter for parameter in result if parameter and parameter != "void"]


def parameter_text(source: str, opening: int) -> str:
    """@brief Read the balanced parameter list after a function name.
    @param source Entire source file.
    @param opening Index of the opening parenthesis.
    @return Text inside the outer parameter parentheses.
    @pre opening identifies a balanced function parameter list.
    @post Source text is unchanged.
    """
    depth = 1
    for index in range(opening + 1, len(source)):
        depth += (source[index] == "(") - (source[index] == ")")
        if depth == 0:
            return source[opening + 1:index]
    raise ValueError("Unbalanced function parameter list")


def check_contracts(path: Path) -> list[str]:
    """@brief Check documented C++ declarations or named TypeScript functions.
    @param path Source file to inspect.
    @return Human-readable contract violations.
    @pre path is a readable .ixx or .ts source file.
    @post Source files are unchanged.
    """
    source = path.read_text()
    errors = []
    documentation = CPP_DOCUMENTATION if path.suffix == ".ixx" else r"/\*\*[\s\S]*?\*/"
    comments = list(re.finditer(documentation, source))
    classes = re.findall(r"\bclass\s+(\w+)", re.sub(r"/\*[\s\S]*?\*/|//[^\n]*", "", source))
    if path.suffix == ".ts":
        pattern = r"^\s*(?:export\s+)?function\s+(\w+)\s*\("
    else:
        constructors = "|".join(re.escape(name) for name in classes) or r"(?!)"
        pattern = (r"^\s*(?:export\s+)?(?:(?:static|virtual|inline|explicit|constexpr)\s+)*"
                   r"(?:(?:auto|void)\s+((?:operator\s*[^(\n]+)|[\w:]+)"
                   r"|(~?(?:" + constructors + r")))\s*\(")
    for match in re.finditer(pattern, source, re.M):
        preceding = [comment for comment in comments if comment.end() <= match.start()]
        comment = preceding[-1] if preceding else None
        line = source.count("\n", 0, match.start()) + 1
        label = f"{path.relative_to(ROOT)}:{line}"
        if comment is None:
            errors.append(f"{label}: missing Doxygen contract")
            continue
        intervening = source[comment.end():match.start()]
        template_declarations = intervening
        intervening = re.sub(r"\btemplate\s*<[^>]*>", "", intervening)
        intervening = re.sub(r"^\s*(?:export\s*|requires[^\n]*)$", "", intervening, flags=re.M)
        if intervening.strip():
            errors.append(f"{label}: missing adjacent Doxygen contract")
            continue
        contract = comment.group()
        for tag in ("brief", "pre", "post"):
            if not re.search(r"@" + tag + r"\b", contract):
                errors.append(f"{label}: missing @{tag}")
        opening = source.find("(", match.start())
        arguments = parameter_text(source, opening)
        parameters = split_parameters(arguments)
        if path.suffix == ".ixx" and len(parameters) > 1:
            parameter_lines = [line.strip().removesuffix(",") for line in arguments.splitlines() if line.strip()]
            if (parameter_lines != parameters or not arguments.lstrip(" \t").startswith("\n")
                    or not re.search(r"\n[ \t]*$", arguments)):
                errors.append(f"{label}: use one parameter per line in multi-parameter signatures")
        closing = opening + len(arguments) + 1
        signature_tail = re.split(r"[;{]", source[closing + 1:], maxsplit=1)[0]
        returns_value = ("->" in signature_tail and not re.search(r"->\s*void\b", signature_tail))
        if path.suffix == ".ts":
            returns_value = not re.match(r"\s*:\s*void\b", signature_tail)
        if returns_value and not re.search(r"@returns?\b", contract):
            errors.append(f"{label}: missing @return")
        template_names = []
        for template in re.findall(r"template\s*<([^>]+)>", template_declarations):
            template_names += [re.findall(r"\b\w+\b", item)[-1] for item in split_parameters(template)]
        documented_templates = re.findall(r"@tparam\s+(\w+)", contract)
        if sorted(template_names) != sorted(documented_templates):
            errors.append(f"{label}: template parameters {template_names} differ from @tparam entries {documented_templates}")
        names = []
        for parameter in parameters:
            declaration = parameter.split("=", 1)[0]
            if path.suffix == ".ts":
                names.append(declaration.split(":", 1)[0].strip())
            else:
                names.append(re.findall(r"\b\w+\b", declaration)[-1])
        documented = re.findall(r"@param(?:\[[^\]]+\])?\s+(\w+)", contract)
        if sorted(names) != sorted(documented):
            errors.append(f"{label}: parameters {names} differ from @param entries {documented}")
    return errors


def check_cpp_documentation_layout(path: Path) -> list[str]:
    """@brief Enforce the C++ Doxygen comment marker and one-tag-per-line style.
    @param path C++ source or Doxygen namespace documentation to inspect.
    @return Human-readable documentation layout violations.
    @pre path is readable and contains C++-style documentation.
    @post Source files are unchanged.
    """
    errors = []
    for number, line in enumerate(path.read_text().splitlines(), 1):
        label = f"{path.relative_to(ROOT)}:{number}"
        if re.match(r"\s*/\*\*", line):
            errors.append(f"{label}: use /// Doxygen comment blocks in C++")
        if re.match(r"\s*///", line) and len(re.findall(r"(?<!\w)@[A-Za-z]\w*", line)) > 1:
            errors.append(f"{label}: put each Doxygen tag on its own /// line")
    return errors


def check_links(path: Path) -> list[str]:
    """@brief Check relative Markdown links in an authored documentation file.
    @param path Markdown document to inspect.
    @return Missing local link targets.
    @pre path is a readable Markdown file.
    @post Documents and targets are unchanged.
    """
    errors = []
    source = re.sub(r"```[\s\S]*?```", "", path.read_text())
    for match in re.finditer(r"\[[^\]]*\]\(([^)]+)\)", source):
        target = match[1].split("#", 1)[0].strip("<>")
        if not target or re.match(r"[a-z]+:", target):
            continue
        if not (path.parent / target).exists():
            errors.append(f"{path.relative_to(ROOT)}: missing local link {target}")
    return errors


def main() -> int:
    """@brief Validate contracts and authored documentation links.
    @return Zero on success, one if violations are found.
    @pre Run from a checkout containing the documented source layout.
    @post Diagnostics have been printed; repository files are unchanged.
    """
    sources = sorted((ROOT / "targets/tatzel/public").rglob("*.ixx"))
    sources += sorted((ROOT / "targets/typescript_build/src").rglob("*.ts"))
    cpp_sources = sorted((ROOT / "targets").rglob("*.ixx"))
    cpp_sources += sorted((ROOT / "targets").rglob("*.cpp"))
    cpp_sources += sorted((ROOT / "tests/cpp").glob("*.cpp"))
    cpp_sources += sorted((ROOT / "docs").glob("*.dox"))
    documents = [ROOT / "README.MD", ROOT / "CODING_CONVENTIONS.md"]
    documents += sorted((ROOT / "docs").glob("*.md"))
    errors = [error for path in sources for error in check_contracts(path)]
    errors += [error for path in cpp_sources for error in check_cpp_documentation_layout(path)]
    errors += [error for path in documents for error in check_links(path)]
    if errors:
        print("\n".join(errors), file=sys.stderr)
        return 1
    print(f"Documentation contracts and links passed: {len(sources)} source files, {len(documents)} documents")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
