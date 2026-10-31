<p align="center">
	<img src="assets/LogoReadme.svg" alt="Tatzel logo" width="160">
</p>

# Tatzel

Tatzel is an experimental C++ server-driven web UI framework inspired by NiceGUI.
Applications work with logical controls. Tatzel maps them to concrete DOM parts,
and a lightweight browser runtime displays them and returns input events.

The current prototype supports headings, buttons, nested containers, explicit
text setters, and per-client logical state. Other controls and production
lifecycle features remain unfinished.

Start with the [documentation](docs/index.md), the
[build and run guide](docs/building.md), and the
[coding conventions](CODING_CONVENTIONS.md).

The [development example](targets/development/private/Main.cpp) is the canonical
runnable example. A logical update looks like this:

```cpp
auto* heading = ui.Heading( "Waiting" );
ui.Button( "Update", [ heading ] {
	heading->SetText( "Updated from C++" );
} );
```

The documentation sources and Doxygen configuration are maintained here for a
future site at docs.tatzel.dev. See [documentation maintenance and publication](docs/documentation.md).

Tatzel uses the MIT license; see [LICENSE](LICENSE).
