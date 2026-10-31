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
changes, and SetOnSubmit for standalone Enter submission. Within a Form, Enter
submits the whole form instead of invoking the field callback. The browser sends
current values before submission. IME composition is deferred until completion;
repeated edits matching the current logical value do not invoke the input
callback twice. Checkbox change callbacks receive
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

## Rows and columns

Row and Column receive a child builder and optional LayoutOptions. Their
callbacks may also receive the layout pointer, just like Container. Row flows
horizontally and Column vertically; child ownership remains logical.

```cpp
ui.Row( []( auto row ) {
	row.Text( "First" );
	row.Text( "Second" );
}, { .gap = tatzel::ui::Spacing::Small,
	.alignment = tatzel::ui::Alignment::Center, .wrap = true } );
```

SetGap, SetAlignment, SetJustification, and SetWrap update arrangement without
rebuilding children. Spacing offers None, Small, Medium, and Large. Alignment
controls the cross axis; Justification controls the main axis. Default layouts
stretch along the cross axis, start along the main axis, use Medium spacing,
and do not wrap. The adapter maps these semantic choices to CSS; application
code does not need flexbox strings. The local framework stylesheet keeps direct
child margins and Pico's full-width action styling from overriding layout gaps
and row arrangement. Child sizing/grow policies and responsive
breakpoint APIs are not yet provided.

## Selectors

Select takes a label, owned SelectOption choices, an optional initial ID, and an
optional change callback. Each choice has a unique nonempty id, a visible label,
and a disabled flag. Labels may repeat; application values and callbacks always
use IDs. Empty value means no selection. SetPlaceholder changes the visible
empty-choice prompt.

SetValue selects an enabled ID or empty and rejects unavailable values.
SetOptions replaces the choices, preserving an enabled selected ID when it
still exists and returning to no selection otherwise. Duplicate or empty IDs
are rejected before existing state changes. Choice replacement and selected
state reach the native selector as one operation, preserving editor identity.
Labels remain literal text. Options are native control internals without
independent logical identities or event sources.

## Multiline editing

TextArea shares TextInput's value, callback, read-only, placeholder, and validation
API. SetRows controls its positive visible row count, initially four. Enter
inserts a newline; Ctrl/Cmd+Enter submits the enclosing Form or invokes the
standalone SetOnSubmit callback. Composition Enter does not submit. Explicit
matching value updates preserve the caret, as with TextInput.

## Forms and validation

Form accepts an optional accessible label, a content builder, and an optional
successful-submission callback. The content callback may receive a ui::Form
pointer to configure callbacks after creating fields. Fields can be nested in
rows, columns, containers, and cards; nested logical forms are rejected.

```cpp
ui.Form( "Contact", [](
	auto fields,
	tatzel::ui::Form* form
) {
	auto* name = fields.TextInput( "Name" );
	name->SetRequired( true );
	name->SetValidator( []( std::string_view value ) -> std::string {
		return value.size() > 100 ? "Name is too long." : "";
	} );
	fields.SubmitButton( "Save" );
	form->SetOnSubmit( [ name ] {
		// Use the validated logical name here.
	} );
} );
```

SubmitButton uses the enclosing Form's submission behavior. An ordinary Button
remains an action button and does not submit. Single-line Enter and multiline
Ctrl/Cmd+Enter submit the form once; standalone editor callbacks retain their
previous behavior outside forms.

A browser submission includes current values for every enabled field, including
values changed without an input/change event. The server checks the entire
snapshot for membership, source identity, duplicates, missing fields, value
types, and available choices before changing any field. Accepted values are
stored without edit callbacks or value echoes; validation runs after all values
are available. Read-only fields retain their authoritative server values.
Disabled fields are omitted and do not validate. Form::SetDisabled disables its
whole subtree while preserving individual field flags and blocks server-side
editing/submission; modal dismissal remains available.

TextInput, TextArea, Select, and Checkbox inherit common field validation.
SetRequired rejects empty text/selection and unchecked required checkboxes.
Text is not trimmed automatically. SetValidator accepts a typed callback:
string_view for text/selection and bool for checkboxes. Return an owned error
string or empty for success. Required checks run first. Validators are
synchronous and must not mutate values or structure. Cross-field checks can
capture other field pointers because the complete snapshot has been applied.

Validate updates every field's error before returning success/failure. Failed
submission displays errors and invokes SetOnInvalid's callback when present;
it does not invoke SetOnSubmit. SetError also supports application-supplied
errors. Accepted edits or changed programmatic values remove stale field errors.
Errors have dedicated parts and accessible descriptions, so updating them does
not replace editors or their focus.

Form::Submit validates current server values and invokes the corresponding
callback; it does not fetch browser edits still in flight. GetFields exposes
borrowed descendants for framework integration, and HasErrors reports current
enabled-field errors without running validators. Validation exceptions still
reach the transport's handler_failed boundary. Async validation, file uploads,
and multi-select fields are outside this initial form API.

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
logical editor or its enclosing Form and do not implicitly close a dialog.

The [feature status](status.md) lists controls that remain planned.
