# Tatzel documentation

Tatzel is an experimental C++ framework for server-driven web interfaces.
Applications build logical controls in C++; adapters map those controls to
concrete DOM parts. The browser applies DOM messages and returns input events.

Start with the model, then build the example:

1. [Architecture and ownership](architecture.md)
2. [Build and run](building.md)
3. [Working with logical controls](logical-controls.md)
4. [Logging and exception traces](diagnostics.md)
5. [Posting work to a logical page](dispatcher.md)

For framework development:

- [Module and source layout](modules.md)
- [Browser protocol](protocol.md)
- [Feature status and limitations](status.md)
- [Coding conventions](../CODING_CONVENTIONS.md)
- [Maintaining and publishing documentation](documentation.md)
- The Classes and Files sections of the generated site provide the API reference.

The repository is the documentation source of truth. API contracts are maintained
next to the code and the guides explain how those APIs fit together. The static
output is prepared for eventual hosting at docs.tatzel.dev; deployment and DNS
are not configured by the documentation build.
