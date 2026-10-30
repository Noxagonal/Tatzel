# Browser protocol

All application messages are UTF-8 JSON with an op string and a data field.
Browser event data is an object. Server DOM operation data is an array.
Transport status and error data are objects.

DOM part IDs are exact browser node IDs. Logical control IDs are internal to
the server's ownership tree. A browser event always identifies a concrete part;
the server resolves that part to its logical owner.

## Opening a page

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
| remove_element_part | part_id | Raw DOM removal; no logical deletion API. |
| set_attribute | part_id, attribute, attribute_value | ClientUpdater method. |
| remove_attribute | part_id, attribute | ClientUpdater method. |
| set_text | part_id, text | Used by logical heading/button setters. |
| set_modal_open | part_id, open | Raw dialog operation; logical Modal remains planned. |
| set_on_click | part_id | Marks a concrete node for delegated click handling. |
| set_on_change | part_id | Framework plumbing; no built-in input control. |
| set_on_submit | part_id | Framework plumbing; no built-in form control. |
| add_class | part_id, class_name | Browser handler only. |
| remove_class | part_id, class_name | Browser handler only. |
| set_value | part_id, value | Browser handler only; value is a string. |

All identifier and text fields are strings. open is boolean. classes is an
array of strings. A DOM command is queued on the transport; it does not guarantee
the browser has applied it when the C++ method returns.

## Browser events

```json
{"op":"on_click","data":{"part_id":"ui-button|base"}}
{"op":"on_change","data":{"part_id":"ui-input|base","value":"edited"}}
{"op":"on_submit","data":{"part_id":"ui-form|base"}}
```

The current button path supports on_click. Change and submit routing are
available to framework controls implementing the corresponding logical
interfaces. Checkbox changes send a boolean value; the server normalizes it to
the string true or false for Changeable::InvokeOnChange().

Events require an initialized logical page. Unknown parts or controls lacking
the requested event interface are ignored. Part IDs are looked up in stored
DOM-part records, never treated as logical IDs or parsed by delimiter.

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
