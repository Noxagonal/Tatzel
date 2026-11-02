# Posting work to a logical page

Get a Dispatcher from a live PageBuilder on its owning event loop. The returned
handle is copyable and can be captured by a database completion callback or
transferred to a worker. It retains neither the page nor its controls.

```cpp
auto dispatcher = ui.GetDispatcher();
auto status = ui.Text( "Ready" )->GetHandle();

// Inside a worker or database completion callback:
auto posted = dispatcher.Post( [ status, message = std::move( message ) ] {
    if( auto* text = status.Get() ) text->SetText( message );
} );
```

Use owned data and checked element handles in posted callbacks. Resolve a handle
only inside the UI callback, on the same page's owning loop. A handle from a
different page is not safe to resolve here. Logical getters, setters, and builder
operations all remain confined to the owning loop. Capturing a raw control
pointer or application object does not extend its lifetime.

Post accepts a move-only callable, including captures owning a unique_ptr. It
queues the callback and never invokes it inline. When called on the owning loop,
the posted callback starts after the current synchronous callback returns. When
called by a worker, execution can begin before that worker returns from Post.
Prepare all captured data before submission and do not access transferred data
afterward.

## Submission and execution

| Result | Meaning |
| --- | --- |
| DispatchResult::Accepted | The callback entered the queue. Page or event-loop closure can still cancel it before it starts. |
| DispatchResult::Closed | The handle is empty, the session closed, or its event loop exited. |
| DispatchResult::QueueFull | This page has reached its pending-callback capacity. |

Empty callbacks throw tatzel::Exception. Allocation and backend scheduling can
also throw; Post is not a noexcept API. Rejected callbacks and their captures
are released on the submitting thread. Accepted captures are normally released
on the UI loop, after execution or cancellation. Capture destructors must not
throw. Queue locks are never held while callbacks or their capture destructors
run, including rejection and cancellation.

Callbacks execute sequentially on the same loop as browser handlers. FIFO order
means queue insertion order; concurrent producers establish that order when
submitting. Browser messages and other event-loop sources follow the backend's
scheduling. Acceptance says nothing about browser events still in transit, and
changing logical state does not wait for browser acknowledgement.

Session queues hold at most 1024 pending callbacks and process at most 32 per
drain. Framework integration can select different nonzero limits using
DispatcherOwner and DispatchOptions. A zero-delay timer separates further
batches so an indefinitely replenished page queue yields to event-loop I/O.
These are callback-count limits, not payload-byte limits. Callbacks should remain
short; background operations belong in a separate executor.

## Lifetime and shutdown

Disconnect explicitly closes the dispatch queue before releasing registry
ownership. Pending callbacks are discarded and future submissions return Closed.
A callback already executing finishes normally. A removed control's checked
handle resolves to nullptr even while its page remains connected.

The framework retains shared session ownership only while invoking a dispatched
callback. An ElementDispatchScope protects removed controls through that callback
and exception unwinding, exactly as it does for browser handlers. Removal still
invalidates handles immediately.

One submission gate per event loop coordinates workers with loop exit. The quit
hook closes that gate before the backend can destroy its loop, then cancels the
queues. Session registry cleanup also runs on the owning loop. A surviving
Dispatcher is safe to call after loop destruction and returns Closed. Queued
callbacks cannot migrate into a new connection or a newly created loop.

IsCurrentThread reports true only on an open dispatcher's owning thread. It is a
diagnostic check, not a lock or lifetime lease. Applications remain responsible
for their background executor's cancellation, result ordering, and shutdown;
closing the page cancels UI callbacks rather than stopping the operation that
produces them. A later result can supersede an earlier one if background jobs
complete out of order; use application request identities when that matters.

## Failure reporting

Dispatched callbacks establish their own CallbackScope. An exception is reported
with the route, dispatch operation, submission location, and available exception
details. Traced exceptions stop at the dispatched application invocation;
ordinary STL exceptions retain their type and message. Submission location is
metadata, not an invented cross-thread stack trace.

A failed callback does not prevent later callbacks from executing. Changes made
before the failure remain in effect. The ordered update scope flushes those
changes before a recoverable handler_failed browser alert. Later callbacks and
browser actions remain usable. There is no rollback.

Run the example at /dispatcher to submit a background form save, remove its
result before completion, close the page during a save, and exercise a dispatched
failure followed by successful recovery.

## Guarding asynchronous field results

Capture a field's GetEditRevision on its owning loop before submitting work.
Capture its checked handle and the owned revision/value in the worker; resolve
the handle only inside Dispatcher::Post. Use TrySetValue (or TrySetChecked) with
the captured revision in that completion. It returns false when browser edits,
server setters, choice changes, or removal have superseded the captured state.
Ordinary wire updates separately protect newer browser edits still in transit.

/protocol demonstrates delayed normalization with this policy. The token owns
no control and does not make raw pointers safe across threads. For results that
change multiple controls or domain records, retain application-level request
identities and validate the complete operation before mutating any of them.
