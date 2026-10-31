# Working with logical controls

## Register a logical page

Import UI and register a route with App::RegisterPage(). The page factory receives
a scoped PageBuilder and runs separately for every browser session.

The canonical example below is included directly from the executable source:

@include{lineno} Main.cpp

Keeping this example in source means the application build also checks its C++
API usage.

## Control lifetime

Builder methods return borrowed pointers to logical controls. The client page
owns the objects, so callbacks on the same page may capture these pointers.
Do not retain or invoke them after that page is destroyed.

The example creates its counter inside the page factory and captures shared
ownership in the button callback. Each connected client gets its own count.

## Explicit setters

Use Heading::GetText() and Heading::SetText(), or Button::GetText() and
Button::SetText(). Setters copy their input and notify the adapter. The server
queues the resulting DOM operation; the browser applies it asynchronously.

HeadingStyle expresses a semantic heading level. HTML tag selection belongs to
the adapter.

## Scoped logical children

Container receives a callback with a builder scoped to the new logical container.
The callback may optionally receive a pointer to that parent control:

```cpp
ui.Container( [](
	auto children,
	tatzel::ui::Container* container
) {
	children.Heading( "Inside a logical container" );
} );
```

A copied scoped builder keeps the same logical parent. It can be captured for
later use by that page's callbacks, but it does not extend the page lifetime.

## Events

Button callbacks run on the owning WebSocket event loop. A browser click sends
a concrete DOM part ID. Framework routing resolves that part to its logical
button before calling application code.

TextInput exposes SetOnInput for changed browser edits, SetOnChange for committed
changes, and SetOnSubmit for Enter. The browser sends its latest value before
submitting. IME composition is deferred until completion; duplicate final edit
values do not invoke the input callback twice. Checkbox change callbacks receive
bool. Accepted edits update logical state before callbacks and do not echo value
replacements back to the browser.

Only exact adapter-registered source parts can emit an event. An input's label
belongs to the same logical control but cannot send input or change events.
Disabled controls ignore browser changes; read-only text inputs ignore edits
while retaining submission behavior.

## Text and paragraphs

Text and Paragraph own plain text and expose GetText and SetText. The default
adapter preserves whitespace and line breaks. Strings containing markup remain
literal text; these controls do not render HTML. Text is inline and Paragraph
has paragraph semantics.

## Cards and content areas

Card accepts a title, a content builder, and optionally an actions builder:

```cpp
ui.Card( "Account", []( auto content ) {
	content.Paragraph( "Account details" );
}, []( auto actions ) {
	actions.Button( "Save", [] {} );
} );
```

Both scopes belong to the same logical card. The adapter chooses separate DOM
insertion areas, including when a nested container is created in the actions
scope. Each callback may optionally receive a ui::Card pointer as its second
argument. SetTitle updates only the heading; an empty title hides its area
without replacing children. Omitting the actions callback hides the footer.
Copying an actions builder preserves its insertion slot for later additions.
Scope(card) creates a fresh content scope.

## Text input and checkbox state

TextInput takes a label, an optional initial value, and an optional committed
change callback. SetPlaceholder, SetDisabled, and SetReadOnly update presentation
without resetting the value. SetValue explicitly replaces text in the browser.
Checkbox takes a label, an initial bool, and an optional bool change callback;
SetChecked explicitly replaces its browser state.

Input callback text views remain valid throughout that invocation, including
when the callback changes the control's value or replaces its own callback.
These views must not be retained after the callback returns. Programmatic value
changes do not invoke input/change callbacks.

Explicit server replacements may supersede an edit already in flight. There is
no revision-based conflict resolution between independently initiated server
updates and newer browser edits.

## Modal dialogs

Modal takes an accessible title and a scoped content builder, optionally
receiving the ui::Modal pointer. It starts closed. Build its desired content,
then call Open; Close closes it, and SetTitle updates its heading. The default
adapter uses a native modal dialog with an accessible title and close button.
The browser manages focus containment and restores focus when the dialog closes.

Escape, backdrop clicks, and the close button request dismissal through the
server. Closing via native browser dialog behavior also synchronizes logical
state. Repeated close requests are harmless. SetOnClose runs once per logical
open-to-closed transition, after state and browser updates are queued; its
callback may reopen the modal. TextInput Enter submissions are handled by the
logical editor and do not implicitly close a dialog.

The [feature status](status.md) lists controls that remain planned.
