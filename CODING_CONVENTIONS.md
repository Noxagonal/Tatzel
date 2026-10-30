# Coding conventions

Tatzel lets application code describe a web interface using logical C++ controls.
The framework translates those controls into DOM parts and protocol messages;
the browser owns the live DOM and returns input events.

Prefer code whose names, types, ownership, and control flow explain its intent.
Comments should explain contracts, lifetime constraints, and decisions that the
code cannot communicate on its own. Avoid comments that merely repeat statements.

## Architectural boundaries

| Concern | C++ module family | Namespace | Responsibility |
| --- | --- | --- | --- |
| Application API | UI, UI.App, UI.PageBuilder | tatzel | Register pages and build logical control trees. |
| Logical controls | UI.Element.* | tatzel::ui | Store semantic control state and expose explicit getters, setters, and callbacks. |
| Logical event interfaces | UI.Element.Interface.* | tatzel::ui::interface | Describe events accepted by a logical control. |
| DOM representation | UI.DOM.* | tatzel::dom | Describe concrete browser nodes, tags, attributes, classes, and text. |
| Adapter | UI.ElementAdapter.* | tatzel | Map logical state to DOM parts and synchronize logical changes. |
| Browser runtime | TypeScript in targets/typescript_build/src | Browser modules | Apply DOM operations and send concrete part IDs back to the server. |

A logical element and a DOM element are different identities. One logical
control may have multiple DOM parts. Use element_id for a logical ID and part_id
for a browser node ID. Resolve a part ID to its logical owner before dispatching
an application callback. Never infer ownership by splitting an ID string.

HTML tags and CSS choices belong in adapters. Application code should use
ui::HeadingStyle rather than constructing an h1 tag. DOM accessors on a logical
control are framework integration APIs; normal application code uses setters
such as Heading::SetText().

## Names and layout

- C++ classes, enums, concepts, and template type parameters use PascalCase.
- C++ functions use PascalCase and trailing return types: auto GetText() -> std::string_view.
  Overrides of third-party interfaces keep the names required by that interface.
- Local variables, parameters, data members, and namespaces use snake_case.
- Acronyms in C++ names stay uppercase: GetID(), FindElementByPartID().
- Browser TypeScript functions and variables use camelCase; wire fields use snake_case.
- CMake helper functions use snake_case; helper variables follow existing uppercase conventions.
- C++ interface files use .ixx and implementation files use .cpp, with matching PascalCase stems.
- Public logical files live in public/element; DOM files live in public/dom.
- Implementation files live under private, including adapter implementations.
- Use tabs for indentation in C++ and TypeScript. Put C++ braces on their own lines.
- Keep functions short and prefer descriptive names to abbreviations or explanatory noise.
- Include each standard header directly when its declarations are used; do not depend on the PCH.

The module umbrella remains UI. Logical controls are exported as
UI.Element.Heading, UI.Element.Button, and so on. UI.DOM is reserved for concrete
DOM representations. Keep imports and exports consistent when moving a file.

## Ownership and mutation

Use unique_ptr for owned logical children and borrowed pointers or references
for scoped access. Use shared_ptr only when lifetime must be retained across a
boundary, such as a registry lookup or per-page state captured by callbacks.

Logical controls have stable addresses. Do not copy or move controls after
construction. Builder handles borrow their client page; capturing a builder or
control pointer does not extend that page's lifetime.

Own stored strings. Accept string_view for transient input, then copy it into
persistent state. A returned string_view or span is borrowed; document which
mutations invalidate it.

Mutate a client's logical state on its owning WebSocket event loop. A registry
mutex protects registry entries; it does not make logical controls thread-safe.
Do not invoke application callbacks while holding the registry mutex.

Expose explicit state-changing methods. A setter updates logical state, then
notifies the adapter. The adapter updates concrete part state and sends DOM
operations. Avoid generic property wrappers and hidden assignment side effects.

## Required API documentation

Every named function, constructor, destructor, operator, and class must have
Doxygen documentation. This includes internal helpers and explicitly defaulted
or deleted special members. Document a function at its authoritative declaration;
do not duplicate that contract at the out-of-line definition. Document standalone
implementation helpers at their definitions.

Every function contract must include:

- `@brief`: the operation's purpose and observable behavior.
- `@param` for every declared parameter, using its exact name. Explain ownership,
  nullability, units, and lifetime where relevant.
- `@tparam` for every template parameter.
- `@pre`: required state and caller obligations. Write None. when no special
  preconditions exist.
- `@post`: the state guaranteed after normal completion. State whether work is
  queued rather than already visible in the browser.
- `@return` or `@returns` for non-void results, including ownership and failure values.
- `@throws` for exceptions that callers are expected to handle.

Name parameters in deleted and defaulted declarations so their contracts can
reference them. For a deleted operation, state that it is unavailable; its
preconditions do not imply it can be called.

Class documentation describes its role, ownership, lifetime, and threading
constraints. Document enum values and public data members. Small getters may use
a compact one-line contract when it remains readable. Lambdas used as local
callbacks do not need separate API blocks; explain non-obvious capture lifetimes.

```cpp
/**
 * @brief Replaces a heading's logical text and notifies its adapter.
 * @param text New heading text, copied before returning.
 * @pre Called on the owning page's event loop.
 * @post The logical text is owned and the bound adapter has queued its DOM update.
 */
auto SetText( std::string_view text ) -> void;
```

Contracts describe actual behavior. Assertions can enforce internal
preconditions, while externally supplied messages require runtime validation.
Do not claim guarantees the implementation does not provide.

## Errors, tests, and documentation changes

Validate browser messages before accessing their fields. Use the documented
op/data envelope for success and error messages. Keep application exceptions
inside a transport boundary and report useful diagnostics on the server.

Test behavior at boundaries: logical-to-DOM mapping, event ownership, string
lifetimes, and protocol payloads. Avoid tests that only restate an implementation.
A parser or flattened-core check does not replace a real C++ module build.

Update the guides, feature status, and API contracts in the same change as the
behavior they describe. Keep examples executable where possible. Remove obsolete
commented-out implementations; use a focused TODO and the feature-status guide
for deliberately unfinished controls.

Before submitting, run the documentation contract check, Doxygen, relevant
browser tests, and the full application build when its toolchain is available.
