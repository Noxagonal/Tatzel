# Feature status and limitations

Tatzel remains a proof of concept. This page describes the current source rather
than earlier experiments.

| Area | Current behavior |
| --- | --- |
| Logical heading | Creation, semantic heading levels, GetText(), and synchronized SetText(). |
| Logical button | Creation, label setters, replaceable click callback, and part-to-logical event routing. |
| Logical container | Nested logical builders and an optional parent pointer in child callbacks. |
| Text and paragraph | Owned plain text, preserved line breaks, and synchronized SetText(). |
| Card | Optional title, scoped content/actions builders, and synchronized title changes. |
| Text input / area | Shared value, label, placeholder, disabled/read-only, required/custom validation, and input/change callbacks. Single-line Enter submits; multiline Enter inserts a newline and Ctrl/Cmd+Enter submits. |
| Number input | Optional finite numeric values, invalid draft detection, bounds/step validation, numeric edit callbacks, and inherited form/read-only/disabled support. |
| Radio group | Native keyboard-accessible choices, stable IDs, disabled options, replaceable choices, and one field per form snapshot. |
| Link | Owned plain text and URL, same-context/new-tab navigation, safe scheme validation, and opener isolation. |
| Image | Owned resource/alternative text, optional intrinsic dimensions, lazy loading hint, and synchronized setters. |
| Checkbox | Accessible label, boolean state, disabled/required policies, custom validation, explicit setters, and typed change callbacks. |
| Row / Column | Scoped child builders with semantic spacing, alignment, distribution, and wrapping setters. |
| Select | Owned stable option IDs, replaceable choices, unavailable choices, no-selection prompt, validation, and typed selection callbacks. |
| Form | Complete typed field snapshots, required/custom validation and field errors, submit buttons, callbacks, read-only/disabled policies, and nested-form rejection. |
| Modal | Closed-by-default scoped content, native modal focus handling, Open/Close, and Escape/backdrop/close-button/native-close synchronization. |
| Element lifecycle | Remove, all/slot-specific ClearChildren, immediate handle invalidation, scoped deferred destruction, expired-builder rejection, and browser dialog/focus cleanup. |
| Per-client state | A separate logical tree and page-factory invocation for each connection. |
| UI dispatcher | Weak, thread-safe Post handles; move-only callbacks; bounded FIFO queues; fair batch draining; disconnect and event-loop shutdown cancellation; per-callback removal and diagnostic scopes. |
| Static assets | HTML template plus compiled browser modules served through Drogon. |
| DOM protocol | Validated op/data envelopes; DOM commands carry arrays of part payloads. |
| Error reporting | Uniform explicit tatzel::Exception throws; standard/third-party exception reports; HTTP/WebSocket boundaries; contextual report blocks and traces trimmed at application callback invocations. |
| Logging | Runtime-configurable Tatzel API over Drogon's backend, startup/shutdown messages, and existing file/rotation configuration. |
| Documentation | Authored Markdown guides, Doxygen contracts, and a static HTML build. |

## Planned controls

The Label, Span, HorizontalRule, Header, and Footer modules are
reserved and currently export no logical class or builder.
Add each logical type, adapter mapping, builder method, documentation, and
behavior check together.

The browser also understands lower-level class operations; these do not imply
an additional application-facing styling API. ClientUpdater's raw DOM removal
remains a low-level DOM operation; application code uses logical Remove or ClearChildren.

## Remaining limitations

- Explicit server value replacements can supersede newer browser edits; there is no conflict-resolution protocol.
- No automatic reconnect, page-state restoration, or client-side navigation.
- No rollback of a partially built logical page after a factory throws; reload to retry.
- Original throw traces require tatzel::Exception; native stack capture and symbol resolution are best effort.
- Applications embedding the library supply their own outer startup catch; the development example demonstrates it.
- No DOM diff engine or automatic mutation batching.
- Checked handles resolve only on the owning event loop; arbitrary raw-pointer captures remain caller-managed.
- Dispatcher callbacks must remain short; background executors, task cancellation, stale-result policies, and asynchronous validation remain application responsibilities.
- No built-in authentication or authorization layer.
- No table, chart, or reusable component abstraction.
- Forms validate synchronously; asynchronous validators, file inputs, and multiple selection are not supported.
- Runtime paths depend on the repository working directory.
- No install/export package; compatibility coverage is currently one CI toolchain.
- CI covers Ubuntu 24.04 with Clang 18.1.3, plus docs and browser checks;
  other compiler/platform combinations remain unverified.

Current contracts are documented in the API reference. Future work should update
this status page when it makes a complete application-facing path usable.
