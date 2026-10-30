# Feature status and limitations

Tatzel remains a proof of concept. This page describes the current source rather
than earlier experiments.

| Area | Current behavior |
| --- | --- |
| Logical heading | Creation, semantic heading levels, GetText(), and synchronized SetText(). |
| Logical button | Creation, label setters, replaceable click callback, and part-to-logical event routing. |
| Logical container | Nested logical builders and an optional parent pointer in child callbacks. |
| Per-client state | A separate logical tree and page-factory invocation for each connection. |
| Static assets | HTML template plus compiled browser modules served through Drogon. |
| DOM protocol | Validated op/data envelopes; DOM commands carry arrays of part payloads. |
| Error reporting | Structured transport errors and a callback exception boundary. |
| Documentation | Authored Markdown guides, Doxygen contracts, and a static HTML build. |

## Planned controls

The Label, Paragraph, Span, Link, Image, HorizontalRule, Card, Header, Footer,
and Modal modules are reserved and currently export no logical class or builder.
Add each logical type, adapter mapping, builder method, documentation, and
behavior check together.

The browser already understands some lower-level operations for dialogs, form
values, and classes. These handlers do not imply that corresponding logical
controls are available. ClientUpdater's raw DOM removal also does not implement
logical subtree deletion.

## Remaining limitations

- No automatic reconnect, page-state restoration, or client-side navigation.
- No rollback of a partially built logical page after a factory throws; reload to retry.
- No logical subtree removal API, DOM diff engine, or automatic mutation batching.
- No cross-thread logical UI scheduler.
- No built-in authentication or authorization layer.
- No supported input, form, table, chart, or reusable component abstraction.
- Runtime paths depend on the repository working directory.
- No install/export package or published compatibility matrix.
- Repository CI validates docs and browser code; a complete C++ module build
  still requires a suitable local toolchain.

Current contracts are documented in the API reference. Future work should update
this status page when it makes a complete application-facing path usable.
