# Background task queue

Import UI or UI.TaskQueue to use `tatzel::TaskQueue`. It owns worker threads and
ordinary move-only callbacks independently of any browser page. The default is
one worker and at most 1024 pending callbacks; TaskQueueOptions configures both
limits. Pending callbacks are dequeued in FIFO order, but multiple workers may
complete tasks out of order.

Post returns TaskQueueResult::Accepted, Closed, or QueueFull. Check rejection
before presenting work as started. Captures of rejected callbacks are released
before Post returns. Empty callbacks and zero limits throw tatzel::Exception.

Callbacks run and release their captures outside queue locks. An exception is
reported through LogException with the submission location and a background-task
callback boundary; remaining tasks continue. Capture owned data for background
work. Use the existing Dispatcher and ElementHandle to return results to the UI;
resolving handles or changing controls directly on a worker is invalid.

```cpp
auto background = tatzel::TaskQueue{ { .worker_count = 2, .capacity = 128 } };
auto dispatcher = ui.GetDispatcher();
auto status = ui.Text( "Ready" )->GetHandle();
const auto submitted = background.Post( [ dispatcher, status ] {
    auto message = std::string{ "Background work finished" };
    dispatcher.Post( [ status, message = std::move( message ) ] {
        if( auto* text = status.Get() ) text->SetText( message );
    } );
} );
if( submitted != tatzel::TaskQueueResult::Accepted )
    if( auto* text = status.Get() ) text->SetText( "Queue busy; retry" );
```

Declare the application-owned queue outside page callbacks and keep it alive
until its callers stop. Page closures do not cancel background computation;
Dispatcher rejects results for a closed page and does not retain the page.

Shutdown stops new submissions, drains all accepted callbacks, releases their
captures, and joins workers. Concurrent or repeated Shutdown calls are safe.
Destruction performs the same draining shutdown. Tasks must eventually finish;
shutdown does not forcibly interrupt running work. Call Shutdown and destroy the
queue from a non-worker thread. Calling Shutdown from one of its own tasks
throws instead of attempting to join itself.

This initial queue has no task results, coroutine suspension/resumption,
priorities, or cancellation API. The fuller thread pool and coroutine-capable
Task design is tracked in [issue #29](https://github.com/Noxagonal/Tatzel/issues/29).
