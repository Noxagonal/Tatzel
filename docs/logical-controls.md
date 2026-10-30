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
ui.Container( []( auto children, tatzel::ui::Container* container ) {
	children.Heading( "Inside a logical container" );
} );
```

A copied scoped builder keeps the same logical parent. It can be captured for
later use by that page's callbacks, but it does not extend the page lifetime.

## Events

Button callbacks run on the owning WebSocket event loop. A browser click sends
a concrete DOM part ID. Framework routing resolves that part to its logical
button before calling application code.

Change and submit interfaces exist for framework development, but there are
currently no built-in logical input or form builders. See the
[feature status](status.md) before using planned controls.
