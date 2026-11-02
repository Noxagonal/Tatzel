# Browser protocol

All application messages are UTF-8 JSON with an op string and a data field.
Browser event data is an object. Server DOM operation data is an array.
Transport status and error data are objects. Version 2 carries all DOM operations
inside ordered update_batch frames; raw DOM commands are not top-level frames.

DOM part IDs are exact browser node IDs. Logical control IDs are internal to
the server's ownership tree. A browser event always identifies a concrete part;
the server resolves that part to its logical owner.

## Opening a page

The pinned Drogon 1.9.13 server accepts an upgrade as the first request on a TCP
connection. Non-browser clients should establish a fresh connection for the
upgrade. The integration test closes its HTTP health probes before opening a
WebSocket because Node can otherwise reuse the HTTP connection.

The server acknowledges the new WebSocket transport:

```json
{"op":"connected","data":{"message":"Connected","protocol_version":2}}
```

The browser independently sends its location after the socket opens:

```json
{"op":"connect","data":{"protocol_version":2,"route_path":"/","query":"","hash":""}}
```

route_path selects the registered page factory. query remains reserved. Optional
hash must be empty or hash-prefixed and at most 2048 bytes; it is delivered to
the page navigation handler after the factory returns. Invalid initial hashes
are rejected before creating the logical page. Only one successful
page initialization is allowed per connection. connected acknowledges the
transport, not completion of the logical page build. Both peers require version
2; incompatible clients receive a fatal protocol_mismatch error. Deploy the
server and compiled browser assets together.

After successful factory construction, initial navigation, deferred removals,
and update flushing, the server sends:

```json
{"op":"page_ready","data":{"sequence":3}}
```

The browser enables interaction only after it has applied every batch through
that sequence. Construction failures send fatal page_failed and never page_ready.

## Ordered update batches

```json
{"op":"update_batch","data":{"sequence":1,"operations":[
  {"op":"set_text","data":[{"part_id":"ui-heading|base","text":"Updated"}]}
]}}
```

Sequences start at one per connection and increase by exactly one. A batch has
1–128 operations and at most 1 MiB of UTF-8 serialized JSON, including framing.
Counters are integers from zero through 9007199254740991 (batch sequences start
at one). Nested batches and status/error frames inside batches are invalid.
The browser validates the entire frame before applying commands in order.
A sequence gap, malformed command, or DOM application failure blocks interaction
and offers reload. DOM application is not transactional: a runtime failure may
leave a partial batch, so the browser cannot continue that session.

Each page build, browser event, and dispatched callback automatically opens an
update scope. Nested scopes flush only at the outer boundary. Limits partition
large scopes into multiple ordered frames, and oversized array payloads split
between complete parts. One indivisible payload exceeding the limit closes the
sender. There is no command reordering, deduplication, or rollback. Creation
precedes use and removal retains its original place in the stream. Outside a
scope, a raw sender operation flushes immediately.

Scopes also flush completed mutations during callback exception unwinding.
Errors and page_ready are barriers: all prior operations flush before the
control frame. Closing a session discards pending operations and future writes.
Readiness acknowledges initial delivery order; it is not a per-batch browser
acknowledgment, retransmission facility, or reconnect protocol.

## DOM creation

An operation inside a batch:

```json
{
  "op": "create_element_part",
  "data": [{
    "part_id": "ui-example|base",
    "parent_id": "root|main",
    "tag": "h1",
    "classes": [],
    "attributes": {},
    "text_content": "Hello"
  }]
}
```

Each part creates one concrete browser node. Parent parts must already exist or
appear earlier in the same array. root|main is supplied by the HTML template
and represented by the server's logical root; it is not created again.

attributes is an object of string values. text_content uses plain DOM text,
rather than HTML parsing. Inline event-handler attributes are rejected by both
creation and attribute-update handlers. This is a protocol rule, not a complete
HTML sanitization layer; adapters must still choose appropriate tags and values.

## Server DOM operations

Every row below uses a data array of the listed payload objects.

| Operation | Payload fields | Server support |
| --- | --- | --- |
| create_element_part | part_id, parent_id, tag, classes, attributes, text_content | ClientUpdater method. |
| remove_element_part | part_id | Used for logical subtree roots; raw sender calls do not change logical ownership. |
| set_attribute | part_id, attribute, attribute_value | ClientUpdater method. |
| remove_attribute | part_id, attribute | ClientUpdater method. |
| set_text | part_id, text | Used by logical text, label, and title setters. |
| set_modal_open | part_id, open | Used by the logical Modal adapter. |
| set_on_click | part_id | Marks a concrete node for delegated click handling. |
| set_on_change | part_id | Also supplied as an initial attribute by input/checkbox adapters. |
| set_on_submit | part_id | Also supplied as an initial attribute for TextInput Enter handling. |
| add_class | part_id, class_name | Browser handler only. |
| remove_class | part_id, class_name | Browser handler only. |
| set_value | part_id, value, epoch, base_epoch, client_revision, replace | ClientUpdater method; value is a string, including true/false for checkboxes. |
| set_radio_options | part_id, value, options, epoch, base_epoch, client_revision, replace | Replaces native radio choices while retaining the registered group and label. |
| set_select_options | part_id, placeholder, value, options, epoch, base_epoch, client_revision, replace | Replaces native option choices and selection while retaining the select editor. |

All identifier and text fields are strings. open is boolean. classes is an
array of strings. Native select options contain value/label strings and a
disabled boolean; values must be unique and nonempty. The empty selection is
represented by a separate placeholder option. A DOM command is queued on the transport; it does not guarantee
the browser has applied it when the C++ method returns.

## Browser events

```json
{"op":"on_click","data":{"part_id":"ui-button|base"}}
{"op":"on_input","data":{"part_id":"ui-input|editor","value":"editing","epoch":0,"revision":1}}
{"op":"on_change","data":{"part_id":"ui-input|editor","value":"edited","epoch":0,"revision":2}}
{"op":"on_submit","data":{"part_id":"ui-input|editor"}}
{"op":"on_dismiss","data":{"part_id":"ui-modal|base"}}
```

Action buttons support on_click. TextInput, TextArea, and NumberInput accept on_input and
on_change with string values. Standalone editor submission sends the latest
input first. Within a Form, keyboard submission targets the whole form. Select
and RadioGroup on_change values must name enabled option IDs or be empty. Checkbox
on_change values must be boolean; the transport normalizes them to true/false
strings for the framework interface. Modal accepts on_dismiss from its dialog
or close-button part. Browser edits are accepted before callbacks without
echoing set_value operations; explicit server setters send guarded value updates.

Events require an initialized logical page. Unknown parts or controls lacking
the requested event interface are ignored. Existing parts must also carry the
adapter-selected data-ui-on-click, data-ui-on-input, data-ui-on-change,
data-ui-on-submit, or data-ui-on-dismiss marker for that operation. Part IDs are
looked up in stored DOM-part records, never treated as logical IDs or parsed by delimiter.

## Subtree removal

The existing remove_element_part operation carries the rendered roots of the
logical forest selected for removal. Server ownership and handles are already
invalidated before this operation is queued. Descendant browser nodes, including
native Select option nodes, disappear with their containing root. Extra DOM
roots are included when their stored parent lies outside the removed forest.

The browser closes any open dialogs inside removed roots with dismissal echoes
suppressed, removes the nodes, and restores surviving focus. Application close
callbacks are not generated by disposal. The server does not wait for a browser
acknowledgement before releasing retired C++ objects. Late individual events
referencing detached parts are ignored by normal identity lookup.

## Form submissions

```json
{
  "op": "on_submit",
  "data": {
    "part_id": "ui-form|base",
    "fields": [
      {"part_id": "ui-name|editor", "value": "Current text", "epoch": 0, "revision": 1},
      {"part_id": "ui-consent|editor", "value": true, "epoch": 0, "revision": 1}
    ]
  }
}
```

Every enabled descendant field must appear exactly once. Disabled fields are
omitted. Source parts must carry the adapter's data-ui-form-field marker;
labels and error parts cannot submit values. Unknown or foreign fields,
duplicates, missing fields, incorrect types, and unavailable choices reject
the entire snapshot with invalid_message before applying values. Every field
requires safe epoch/revision metadata. A snapshot containing an obsolete field
generation or revision is rejected atomically with stale_form; review current
values and submit again. Read-only
values are not overwritten from browser data.

Within a logical form, on_submit must target the form itself with its complete
snapshot. Direct standalone-style submissions to descendant editors are rejected.

Once accepted, editable values are applied without input/change callbacks or
set_value echoes. Required and custom validation then runs on all fields.
Normal validation failure is represented by field error text and aria-invalid
updates, rather than a transport error. Only valid forms invoke their submit
callback. The default adapter uses novalidate so the logical validators handle
all form submissions consistently.

The adapter initializes Select choices after queuing the editor's DOM creation.
The set_select_options operation builds native option children with plain text;
those children do not have logical event-source IDs. Replacement options and
the selected value are carried together.

Radio groups use set_radio_options with the same options/value schema, without
a placeholder. The editor is a fieldset containing a legend and a dedicated
options container. Native radios are browser-owned children with a shared,
group-local name. Changes and snapshots use the registered fieldset part ID and
one selected ID (empty when unchecked); individual radio nodes are not event
sources. Explicit set_value on that fieldset replaces checked state without
rebuilding choices. Disabled native choices cannot emit normal browser changes;
the server also rejects their IDs.

NumberInput uses decimal string values for input, change, and snapshots. Empty
remains empty. A native badInput flag is encoded as NaN so an invalid draft
cannot become an accepted optional empty value. The server retains malformed or
out-of-range drafts for field validation and accepts only finite numbers through
the application setter. Initial numeric state is sent with set_value after DOM
creation, keeping the native step base at minimum or zero.

## Field edit revisions

Fields start at epoch 0, browser revision 0. Native input/change events advance
the field's revision, including composition drafts that are not yet sent.
Snapshots also detect silent changes such as autofill. Individual input/change
messages include epoch/revision. Old epochs and regressing revisions are ignored;
future epochs, unsafe counters, or reusing a counter for a different value are
invalid. Duplicate events do not invoke callbacks again.

An ordinary server setter includes its current epoch and the last accepted
browser counter as client_revision, with replace false and base_epoch equal to
epoch. The browser applies it only if its own counter still matches and it is
not composing. Matching text is not reassigned, preserving the caret.
This protects edits still in transit; it does not automatically establish which
of several asynchronous jobs is current. Capture GetEditRevision before starting
work and use TrySetValue/TrySetChecked on the owning loop when it completes.
See [logical controls](logical-controls.md) for that application contract.

An intentional reset uses ValueUpdatePolicy::Replace. It starts a fresh epoch,
sets replace true, carries the old base_epoch, and resets browser/server edit
counters to zero. It works even when the logical value already matches. Late
input from the superseded epoch cannot undo it. Active composition is cancelled;
a late composition end restores the replacement instead of submitting old text.

Replacing native choices starts a fresh epoch with replace false. If the browser
has a newer choice still enabled in the new options (including empty selection),
it preserves it, rebases it as revision 1 in the new epoch, and sends one change.
Otherwise it uses the server selection at revision 0. Old-generation choice
messages cannot restore removed choices. Choice operations retain the editor
node and surviving focus.

## Errors and validation

```json
{"op":"error","data":{"code":"invalid_message","message":"Expected an op string and a data object","fatal":false}}
```

Current error codes are session_expired, invalid_json, invalid_message,
already_connected, page_not_found, unknown_operation, handler_failed, page_failed,
page_not_ready, protocol_mismatch, and stale_form.

The server validates envelopes and event field types before dispatch. Callback
exceptions are logged with event context on the server and reported as handler_failed.
Traced tatzel::Exception values retain a callback-bounded pre-unwind stack;
ordinary exceptions retain their available type/message. See [diagnostics](diagnostics.md).
A failed
page factory can leave a partial page and requires a reload.

The browser exposes connecting, initializing, ready, disconnected, and failed
phases. The root is inert until ready and after a terminal failure. Recoverable
errors display a dismissible alert and leave ready pages usable. Fatal errors,
transport closure, offline state, send failures, and a 15-second initialization
timeout offer reload. A failed/disconnected page cannot resume automatically;
reload creates a fresh logical page and no browser actions are replayed.

Templates can supply `#connection-status` and data-reload-label. Without one, the
runtime creates a safe text-only accessible status surface before the root.
The standard templates load framework/connection.css. Browser integrations can
import getConnectionState/onConnectionStateChanged from connection-state.js or
listen for tatzel:connection-state CustomEvents. Observer exceptions are contained;
unsubscribe releases a listener. Module snapshots are immutable. Initial page
readiness and recovery behavior are the same for the development and Workspace
applications.

## Same-page navigation

Browser hash changes send `{"op":"on_navigate","data":{"hash":"#/tasks"}}`.
This requires an initialized page and an empty or hash-prefixed string of at
most 2048 bytes. Invalid values receive invalid_message. The page's current
navigation handler runs under callback/removal scopes.

Programmatic PageBuilder navigation sends
`{"op":"set_location","data":{"hash":"#/tasks"}}` and invokes the handler on
the server. The browser uses history.pushState only for a changed hash, without
echoing an event. Native hash links and Back/Forward generate the browser event.
An empty hash retains the registered path and query. This operation cannot select
another server route or navigate to another origin.
