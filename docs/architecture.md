# Architecture and ownership

## Logical controls and concrete DOM parts

An application creates logical controls such as a heading, button, or container.
A logical control expresses purpose and state: heading level, label text, child
controls, or a callback. It does not choose its HTML tag.

An element adapter owns that mapping decision. The default adapter turns a
logical heading into an HTML heading, a button into a button node, and a container
into a div. Compound controls own several DOM-part descriptions: a text input
owns a wrapper, label, and editor; a card owns heading, content, and action
parts. Card children retain one logical parent while semantic Content and
Actions slots select their adapter-owned insertion areas.

Tatzel stores these descriptions on the server. The browser owns the live DOM
nodes. Native selector options are internal browser nodes built from the
adapter's typed choice command; the select editor remains their logical event
source. Changing a DOM-part description alone does not send a browser message;
the adapter must also ask ClientUpdater to send the corresponding operation.

The application-facing namespace is tatzel::ui and its module family is
UI.Element.*. Concrete DOM descriptions use tatzel::dom and UI.DOM.*.

## Request and update flow

1. App registers an HTTP route and a logical page factory.
2. An HTTP request receives data/template.html.
3. The browser opens /ws and sends connect with its route_path.
4. The server creates a client-owned logical tree and invokes the page factory once.
5. Builders create logical controls; the adapter assigns DOM parts; ClientUpdater
   queues DOM creation messages.
6. The sender flushes bounded ordered batches and acknowledges page readiness.
   The browser applies those batches, enables interaction, and sends revisioned
   input events with their exact part_id.
7. The server resolves each part ID to its logical owner, invokes the control's
   callback, and sends any resulting DOM updates.

Logical IDs identify controls. Part IDs identify browser nodes. The default
adapter constructs IDs such as ui-uuid|base, but event routing searches the
stored part records rather than parsing that spelling.

## Explicit updates

A logical setter changes owned semantic state and calls its bound change handler.
The default adapter updates the corresponding DOM-part text and queues a
set_text message. Browser visibility follows asynchronously.

FormField centralizes label, disabled/required policy, error state, and typed
validation for built-in fields. A Form coordinates descendant fields through
logical ownership, including fields nested in layouts. A complete browser
snapshot is checked before accepting values; validators run after all values
are available. Application callbacks never receive a partially checked snapshot.

Application code uses methods such as Heading::SetText(), TextInput::SetValue(),
Checkbox::SetChecked(), and Modal::Open().
There is no generic property wrapper and direct assignment to private control
state is unavailable.

Accepted browser edits update logical state before application callbacks. They
do not echo value replacements. Adapters track explicit server-value revisions
so label, placeholder, or disabled-state updates cannot reset active typing.
A programmatic SetValue or SetChecked change carries a browser revision baseline
and preserves newer browser input. Intentional resets use ValueUpdatePolicy::Replace
to invalidate old-generation edits. Captured EditRevision tokens guard asynchronous
logical field results. Matching text values are left untouched to preserve the caret.
See the [protocol](protocol.md) for the revision and choice-rebasing rules.

A ClientUpdateScope surrounds page builds, browser messages, and dispatched
callbacks, outside the logical removal scope so deferred removals join the same
ordered stream. Nested updates flush at the outer boundary, on size limits, or
before error/readiness control frames. Completed mutations flush on exceptions;
there is no rollback. The browser blocks a session after framing/DOM failures
and offers a fresh page through reload.

## Ownership

App is the single application owner and cannot be copied or moved. It owns the
registered page factories. Registration finishes before Run() starts.

A ClientConnection owns its DOM sender, adapter, logical tree, and builder
context. Registry lookups return shared session handles so adding or removing
another connection cannot invalidate an in-flight lookup.

The logical root owns its logical children with unique_ptr. Controls remain at
stable addresses until removal or page destruction. PageBuilder is a borrowed
scope with a checked parent; copying it preserves the logical parent and
semantic child slot but does not retain the client page.

DOM parts are assigned once and contain owned strings. Borrowed views into a
control or part expire when that object is destroyed, and can be invalidated by
mutating the underlying string or collection.

## Threading and lifecycle

Drogon invokes a connection's handlers on that connection's event loop. Logical
control creation and mutation must remain on that loop. The connection registry
mutex only protects registry access.

PageBuilder::GetDispatcher returns a weak submission handle for the owning
ClientConnection. Workers post owned completions through its bounded queue;
callbacks execute on that connection's loop inside the existing logical removal
and diagnostic scopes. Queue locks are released before application code or
capture destructors run. See [posting work](dispatcher.md) for ordering,
rejection, cancellation, and shutdown contracts.

Page callbacks should capture per-client state by ownership when necessary.
Shared application state needs its own synchronization across connections.

Logical removal detaches ownership and invalidates the subtree's weak handle
tokens immediately. ClientElementTree owns a short retired-object queue. Nested
ElementDispatchScope guards hold that queue until the outermost page dispatch,
child-builder callback, application callback, or synchronization operation ends.
The queue then drains synchronously, including on exceptions and removals caused
by captured-resource destruction. Outside a surrounding scope, Remove finishes
destruction before returning. Weak handles retain no control or page ownership.

The tree computes rendered removal roots from the selected forest's stored
part and parent IDs. A DOM part is a removal root when its parent lies outside
that forest. This covers compound and separately rendered parts without parsing
ID strings or requiring every control to have exactly one rendered root.
Adapters must keep unrelated logical subtrees out of a control's rendered roots;
separately rendered parts may live under a logical ancestor's DOM parts.

Closing a WebSocket invalidates all active handles before releasing controls.
It removes its registry entry and releases the page services
when retained handlers finish. Dispatch queues close explicitly on disconnect,
and one gate per loop prevents scheduling through a destroyed loop. Loop-exit
hooks cancel pending work and release registry sessions on their owning loops.
Reconnect, state restoration, and rollback after a failing page factory are not implemented.
Reload the browser to create a fresh page.

## Application composition

The [workspace sample](workspace.md) composes existing logical controls into a
persistent application. Its database worker and domain model belong to the sample
executable. PageBuilder hash-navigation callbacks replace central content while
keeping the shell, page context, and dispatcher alive. Appearance names are owned
logical metadata synchronized independently of value handlers; the default
adapter places them on each control's base part as `data-ui-appearance`.
