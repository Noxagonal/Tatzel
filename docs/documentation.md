# Maintaining and publishing documentation

## One source set

Authored Markdown explains concepts, setup, workflows, current support, and
protocol behavior. Doxygen comments next to authoritative declarations describe
class and function contracts. The generated site combines both.

This keeps conceptual documentation readable in GitHub while avoiding a second
hand-maintained API reference. It also keeps the first publication independent
of an application framework or a JavaScript documentation toolchain.

The root CODING_CONVENTIONS.md is included in the site. Follow its requirements
for @brief, all parameters, template parameters, preconditions, postconditions,
return values, and expected exceptions.

## Build locally

Install Python 3.10 or newer and Doxygen 1.9.8 or newer, then run from the
repository root:

```sh
python3 tools/check_documentation.py
doxygen Doxyfile
python3 -m http.server 8081 --directory build/docs/html
```

Open http://localhost:8081/. Generated HTML is written to build/docs/html and
is ignored by Git. The build does not compile the application or fetch Drogon.

Doxygen warnings fail the documentation build. The lightweight contract check
also checks required @pre/@post tags, parameter names, and local documentation
links. It supplements Doxygen; it is not a C++ parser.

Doxygen sees .ixx files through a small input filter that removes module/import
syntax while preserving line numbers. The original module source remains the
build input and source listing.

The development example is included from its executable source using Doxygen's
include command, so the guide and the example stay aligned.

## Continuous validation

The repository workflow checks documentation contracts, generates the site,
builds and tests browser code, and uploads the generated HTML as an artifact.
It does not deploy the site or certify a complete C++26 module build.

Review the rendered artifact when changing navigation, examples, or API
structure. Keep feature status honest while the framework is incomplete.

## Publishing to docs.tatzel.dev

The publication unit is the contents of build/docs/html. It is a static site and
can be served by a static host with HTTPS.

When hosting is chosen:

1. Configure the host's custom domain as docs.tatzel.dev.
2. Add the DNS record required by that host.
3. Enable and verify the HTTPS certificate.
4. Publish the generated HTML directory at the domain root.
5. Check navigation, search, API links, and source references at the public URL.

Keep hosting credentials and deployment settings outside the documentation
sources. Add the actual deployment workflow after the hosting destination and
release branch policy are decided. No DNS changes or deployment are part of the
current documentation setup.
