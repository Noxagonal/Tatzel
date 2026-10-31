# Browser protocol

All application messages are UTF-8 JSON with an op string and a data field.
Browser event data is an object. Server DOM operation data is an array.
Transport status and error data are objects.

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
{"op":"connected","data":{"message":"Connected"}}
```

The browser independently sends its location after the socket opens:

```json
{"op":"connect","data":{"route_path":"/","query":"","hash":""}}
```

route_path selects the registered page factory. query and hash are currently
reserved browser context and are not passed to the factory. Only one successful
page initialization is allowed per connection. connected acknowledges the
transport, not completion of the logical page build.

## DOM creation

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
| set_value | part_id, value | ClientUpdater method; value is a string, including true/false for checkboxes. |
| set_select_options | part_id, placeholder, value, options | Replaces native option choices and selection while retaining the select editor. |

All identifier and text fields are strings. open is boolean. classes is an
array of strings. Native select options contain value/label strings and a
disabled boolean; values must be unique and nonempty. The empty selection is
represented by a separate placeholder option. A DOM command is queued on the transport; it does not guarantee
the browser has applied it when the C++ method returns.

## Browser events

```json
{"op":"on_click","data":{"part_id":"ui-button|base"}}
{"op":"on_input","data":{"part_id":"ui-input|editor","value":"editing"}}
{"op":"on_change","data":{"part_id":"ui-input|editor","value":"edited"}}
{"op":"on_submit","data":{"part_id":"ui-input|editor"}}
{"op":"on_dismiss","data":{"part_id":"ui-modal|base"}}
```

Action buttons support on_click. TextInput and TextArea accept on_input and
on_change with string values. Standalone editor submission sends the latest
input first. Within a Form, keyboard submission targets the whole form. Select
on_change values must name enabled option IDs or be empty. Checkbox
on_change values must be boolean; the transport normalizes them to true/false
strings for the framework interface. Modal accepts on_dismiss from its dialog
or close-button part. Browser edits are accepted before callbacks without
echoing set_value operations; explicit server setters send value replacements.

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
      {"part_id": "ui-name|editor", "value": "Current text"},
      {"part_id": "ui-consent|editor", "value": true}
    ]
  }
}
```

Every enabled descendant field must appear exactly once. Disabled fields are
omitted. Source parts must carry the adapter's data-ui-form-field marker;
labels and error parts cannot submit values. Unknown or foreign fields,
duplicates, missing fields, incorrect types, and unavailable choices reject
the entire snapshot with invalid_message before applying values. Read-only
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

## Errors and validation

```json
{"op":"error","data":{"code":"invalid_message","message":"Expected an op string and a data object"}}
```

Current error codes are session_expired, invalid_json, invalid_message,
already_connected, page_not_found, unknown_operation, and handler_failed.

The server validates envelopes and event field types before dispatch. Callback
exceptions are logged on the server and reported as handler_failed. A failed
page factory can leave a partial page and requires a reload.

The browser validates server envelopes and operation payloads before applying
DOM changes. It logs structured server errors. This protocol is experimental
and currently has no version-negotiation mechanism.
