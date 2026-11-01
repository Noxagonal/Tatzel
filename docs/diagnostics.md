# Logging and exception traces

Import UI for the application API, or UI.Logging and UI.Diagnostics directly.
Tatzel uses Drogon's existing Trantor logging backend. Log levels and output
destinations are shared with Drogon; there is no second logging service.

```cpp
tatzel::Log( tatzel::LogLevel::Info, "Application initialized" );
```

Log preserves the application's logging source location. Configure the shared
threshold after constructing App and before Run with SetLogLevel, or use app.log in
data/drogon_config.json. The example config uses INFO and an empty log_path for
console output. DEBUG adds registered-route details. Tatzel emits starting,
listening, and stopped messages; the listening message is queued after Drogon's
listener-start phase. These messages remain enabled in release builds when the
runtime threshold permits them. LogLevel has no process-aborting fatal severity.

IsLogEnabled lets an application skip expensive message construction. Arguments
to Log are evaluated before the call, even when its level is filtered. Configure
logging before worker threads start; concurrent threshold changes are unsupported.
Drogon's existing app.log options also configure file output and rotation; a
file destination replaces the default console destination rather than adding a
second Tatzel output sink. Installed backend output/flush callbacks must not throw.
Info and higher messages flush stdout after emission so the default console
sink is visible promptly even when its output is piped. File flushing remains
owned by the configured Drogon backend.

## Capturing an application failure

```cpp
ui.Button( "Save", [] {
	throw tatzel::Exception{ "The save operation failed" };
} );
```

Exception derives from std::runtime_error. Construct it at the throw expression
so its std::source_location and std::stacktrace::current describe the failing
application code before stack unwinding. Constructing it earlier records that
earlier location. A bare rethrow preserves the original diagnostic object.
All explicit C++ throws in Tatzel use this type, including builder/lifecycle
guards, control validation, adapter checks, and utility/application errors.
Recoverable application catches should catch tatzel::Exception rather than
the previous std::logic_error or std::invalid_argument types.

Tatzel establishes CallbackScope around button, editor, selector, checkbox,
modal-close, form, validator, page-builder, and child-builder invocations. The
scope records an entry stack on the current thread. Exception snapshots the
innermost scope's name and compares the stacks' common outer suffix using native
frame identities. Default reports end at that invocation, omitting the transport,
event loop, and thread-start callers beyond it. Symbol names are not used to find
the boundary, so application namespace names do not affect trimming. The report
also hides recognized standard-library callable-dispatch wrappers using their
resolved source headers and symbols. Unrecognized wrappers remain visible;
the raw capture always retains them for inspection.

Scopes nest and restore correctly during exception unwinding. Exception owns
its diagnostic metadata, so reporting after callback/receiver destruction is
safe. Both scope and exception captures are bounded to 128 frames. Capturing an
entry stack costs work even for a successful callback; this is an initial
diagnostic implementation whose overhead should be measured for busy workloads.

Stack capture, descriptions, and source-line resolution are best effort. Inlining,
optimization, stripped symbols, native depth limits, or unavailable stack support
can reduce detail. Use Debug or RelWithDebInfo and preserve the executable's debug
symbols for useful file/line output. If a callback boundary cannot be identified,
the report retains the explicit construction location and callback name while
omitting raw framework frames. GetStackTrace exposes the bounded raw capture for
manual inspection; GetVisibleFrameCount identifies the normal display prefix.
Long native function descriptions are abbreviated in console output; unsupported
C++ mangled names are labelled unresolved. Complete descriptions remain available
through the raw entries.
An exception constructed outside any CallbackScope exposes its entire bounded
trace because there is no application invocation boundary to trim against.

Ordinary std::exception values still get their dynamic type and what() logged.
They do not acquire an original throw stack automatically. Capturing a fresh
stack in a catch block would describe the catch site instead. Nonstandard thrown
values are reported without inventing a message or throw trace. Nested causes
are inspected up to eight levels. Standard-library exceptions are not converted
to Tatzel exceptions and do not need trace/location metadata to be reported.

## Server reports

WebSocket dispatch captures route, operation, logical element ID, and browser
part ID before invoking application code. Those owned IDs remain available when
the callback removes its own receiver. Caught failures are reported with a unique
process-local error ID on a single report block, distinct Reported at and Thrown at
locations, and available callback/trace metadata. A report is emitted as one
backend message with one timestamp/severity header. Ordinary exceptions show
their type and message while omitting unavailable original location/trace fields.
Browser field values are not
included in this context. Explicit application exception messages remain the
application's responsibility.

LogException accepts std::exception_ptr for manual reporting at application
boundaries. Formatting failures fall back to a minimal stderr diagnostic. A
failed WebSocket error response is logged separately and the affected connection
is closed on a best-effort basis. The browser still receives only the generic
handler_failed envelope; stack traces stay in server logs. It currently logs that
envelope to its console without displaying a notification.

WebSocket connection/message/close handlers and registered HTTP handlers report
escaping standard and nonstandard exceptions. A failed HTTP template read returns
a generic 500 response; response-delivery failures are reported separately.
The example main function reports exceptions escaping application setup or Run
and returns a failure status. Library callers own the corresponding outer catch
when they write their own main. Locally handled exceptions are reported only when
that catch calls LogException; the retired-builder example does so explicitly.

An event failure leaves preceding logical changes applied, finishes deferred
destruction, and allows later events. A failed page factory can still leave an
incomplete page that requires reload. Diagnostics do not
make failed assertions, memory corruption, or throwing noexcept/destructor code
recoverable.

Open /lifecycle and press Remove then throw to inspect a traced application
helper and its button callback. Try retired builder logs a locally handled
tatzel::Exception and updates its status without sending a browser failure.
Throw standard exception exercises std::vector::at: its std::out_of_range type
and message appear in the server log without a fabricated throw trace/location.
