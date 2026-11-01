module;

#include <trantor/net/EventLoop.h>
#include <algorithm>
#include <cstddef>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <source_location>
#include <thread>
#include <utility>
#include <vector>

module UI.Dispatcher;

import UI.Diagnostics;
import UI.Logging;

namespace tatzel {

/// @brief Owned callback and submission metadata kept in a bounded queue slot.
struct DispatchTask
{
	std::move_only_function<void()> callback; ///< Callable and its transferred application captures.
	std::source_location origin; ///< Submission site, independent of the execution thread.
};

/// @brief Submission gate shared by all dispatcher queues on one event loop.
/// @details The quit hook closes the gate under the same lock used to schedule,
/// so a submitting worker cannot dereference a destroyed EventLoop. Queue
/// locks are acquired after this gate, and user captures are destroyed outside both.
class DispatcherRuntime
{
public:
	/// @brief Associates the scheduling gate with its owning loop and immutable thread identity.
	/// @param loop Event loop used until the loop-exit hook closes this gate.
	/// @pre Called on loop's thread.
	/// @post The gate is open with no registered queues.
	explicit DispatcherRuntime( trantor::EventLoop& loop ) :
		loop{ &loop },
		thread_id{ std::this_thread::get_id() }
	{}

	/// @brief Closes scheduling before loop destruction and cancels every registered queue.
	/// @pre Called by the owning loop's nonthrowing quit hook.
	/// @post No worker can dereference the loop through this runtime; pending captures are released.
	auto Close() noexcept -> void;

	std::mutex mutex; ///< Protects scheduling, closure, and queue registration.
	trantor::EventLoop* loop; ///< Borrowed until the quit hook clears it under mutex.
	const std::thread::id thread_id; ///< Immutable identity, safe to inspect after loop destruction.
	std::vector<std::weak_ptr<DispatcherState>> queues; ///< Non-owning queues to close on loop exit.
};

/// @brief Private queue state retained by its owner, never by application handles.
class DispatcherState : public std::enable_shared_from_this<DispatcherState>
{
public:
	/// @brief Allocates fixed queue slots before any concurrent submissions are possible.
	/// @param runtime Shared scheduling gate for the owning loop.
	/// @param invoke Page invocation boundary, retaining its page weakly.
	/// @param options Validated nonzero queue limits.
	/// @pre Called on the owning loop.
	/// @post Queue insertion and removal can transfer callback ownership without allocation under locks.
	DispatcherState(
		std::shared_ptr<DispatcherRuntime> runtime,
		std::function<void( std::move_only_function<void()>&, std::source_location )> invoke,
		DispatchOptions options
	) :
		runtime{ std::move( runtime ) },
		invoke{ std::move( invoke ) },
		queue{ options.capacity },
		batch_size{ options.batch_size }
	{}

	/// @brief Submits owned work while coordinating with closure and loop destruction.
	/// @param callback Nonempty callable transferred into a task before taking locks.
	/// @param origin Original submission site.
	/// @return Whether the callback was accepted or rejected by closure or capacity.
	/// @pre Captures can be transferred and destroyed on either submission or execution threads.
	/// @post Accepted work is queued; rejected or failed task captures are destroyed outside locks.
	auto Post(
		std::move_only_function<void()> callback,
		std::source_location origin
	) -> DispatchResult
	{
		auto task = std::make_unique<DispatchTask>( std::move( callback ), origin );
		auto gate = std::unique_lock{ runtime->mutex };
		auto lock = std::unique_lock{ mutex };
		if( closed || !runtime->loop ) return DispatchResult::Closed;
		if( count == queue.size() ) return DispatchResult::QueueFull;

		if( !scheduled )
		{
			// queueInLoop never invokes inline, so it cannot enter Drain under these locks.
			runtime->loop->queueInLoop( [ weak = weak_from_this() ] {
				if( auto state = weak.lock() ) state->Drain();
			} );
			scheduled = true;
		}
		queue[ ( head + count ) % queue.size() ] = std::move( task );
		++count;
		return DispatchResult::Accepted;
	}

	/// @brief Cancels queued work without holding a lock while capture destructors execute.
	/// @pre The caller respects DispatcherOwner's closure thread contract.
	/// @post The queue is permanently closed and every pending callback is released.
	auto Close() noexcept -> void
	{
		auto cancelled = std::vector<std::unique_ptr<DispatchTask>>{};
		{
			auto lock = std::lock_guard{ mutex };
			closed = true;
			scheduled = false;
			count = 0;
			queue.swap( cancelled );
		}
	}

	/// @brief Checks thread identity without dereferencing the underlying event loop.
	/// @return Whether this queue is open on the current thread.
	/// @pre None.
	/// @post Queue state is unchanged.
	auto IsCurrentThread() noexcept -> bool
	{
		auto gate = std::lock_guard{ runtime->mutex };
		auto lock = std::lock_guard{ mutex };
		return !closed && runtime->loop && runtime->thread_id == std::this_thread::get_id();
	}

private:
	/// @brief Runs a bounded snapshot and uses a timer to yield before further pending work.
	/// @pre Called on the owning loop; no other drain is executing for this queue.
	/// @post Every started callback is contained by an exception boundary; remaining work is rescheduled or cancelled.
	auto Drain() noexcept -> void
	{
		size_t remaining = 0;
		{
			auto lock = std::lock_guard{ mutex };
			remaining = std::min( count, batch_size );
		}

		while( remaining-- )
		{
			auto task = std::unique_ptr<DispatchTask>{};
			{
				auto gate = std::lock_guard{ runtime->mutex };
				auto lock = std::lock_guard{ mutex };
				if( closed || !runtime->loop || !runtime->loop->isRunning() ) break;
				task = std::move( queue[ head ] );
				head = ( head + 1 ) % queue.size();
				--count;
			}

			try
			{
				auto boundary = CallbackScope{ "Dispatched callback" };
				invoke( task->callback, task->origin );
			}
			catch( ... )
			{
				LogException( std::current_exception(), { {}, "dispatch", {}, {}, task->origin } );
			}
		}

		try
		{
			auto gate = std::lock_guard{ runtime->mutex };
			auto lock = std::lock_guard{ mutex };
			if( closed ) return;
			if( !runtime->loop || !runtime->loop->isRunning() )
			{
				// Close outside locks, including the destruction of pending captures.
			}
			else if( count == 0 )
			{
				scheduled = false;
				return;
			}
			else
			{
				// Trantor drains recursively queued functions in one pass. A timer
				// ensures another I/O poll occurs before the next bounded batch.
				runtime->loop->runAfter( 0.0, [ weak = weak_from_this() ] {
					if( auto state = weak.lock() ) state->Drain();
				} );
				return;
			}
		}
		catch( ... ) { LogException( std::current_exception(), { {}, "dispatch_schedule" } ); }
		Close();
	}

	std::shared_ptr<DispatcherRuntime> runtime;
	std::function<void( std::move_only_function<void()>&, std::source_location )> invoke;
	std::mutex mutex;
	std::vector<std::unique_ptr<DispatchTask>> queue;
	size_t head = 0;
	size_t count = 0;
	size_t batch_size;
	bool scheduled = false;
	bool closed = false;
};

auto DispatcherRuntime::Close() noexcept -> void
{
	auto pending = std::vector<std::weak_ptr<DispatcherState>>{};
	{
		auto lock = std::lock_guard{ mutex };
		loop = nullptr;
		queues.swap( pending );
	}
	for( auto& weak : pending )
		if( auto state = weak.lock() ) state->Close();
}

/// @brief Installs one shutdown gate per event-loop lifetime, independent of connection count.
/// @param loop Owning event loop.
/// @return Shared gate whose quit hook runs before the loop can be destroyed.
/// @pre Called on loop's thread.
/// @post The loop has exactly one dispatcher shutdown hook for this runtime.
auto GetDispatcherRuntime( trantor::EventLoop& loop ) -> std::shared_ptr<DispatcherRuntime>
{
	static thread_local std::weak_ptr<DispatcherRuntime> current;
	if( auto runtime = current.lock() )
	{
		auto lock = std::lock_guard{ runtime->mutex };
		if( runtime->loop == &loop ) return runtime;
	}
	auto runtime = std::make_shared<DispatcherRuntime>( loop );
	loop.runOnQuit( [ runtime ] { runtime->Close(); } );
	current = runtime;
	return runtime;
}

Dispatcher::Dispatcher( std::weak_ptr<DispatcherState> state ) :
	state{ std::move( state ) }
{}

auto Dispatcher::Post(
	std::move_only_function<void()> callback,
	std::source_location origin
) const -> DispatchResult
{
	if( !callback ) throw Exception{ "Dispatcher callback must not be empty" };
	auto retained = state.lock();
	return retained ? retained->Post( std::move( callback ), origin ) : DispatchResult::Closed;
}

auto Dispatcher::IsCurrentThread() const noexcept -> bool
{
	auto retained = state.lock();
	return retained && retained->IsCurrentThread();
}

DispatcherOwner::DispatcherOwner(
	trantor::EventLoop& loop,
	std::function<void( std::move_only_function<void()>&, std::source_location )> invoke,
	DispatchOptions options
)
{
	if( trantor::EventLoop::getEventLoopOfCurrentThread() != &loop || !invoke || !options.capacity || !options.batch_size )
		throw Exception{ "Dispatcher requires its owning loop, an invoker, and nonzero queue limits" };

	auto runtime = GetDispatcherRuntime( loop );
	state = std::make_shared<DispatcherState>( runtime, std::move( invoke ), options );
	auto lock = std::lock_guard{ runtime->mutex };
	std::erase_if( runtime->queues, []( const auto& queue ) { return queue.expired(); } );
	runtime->queues.push_back( state );
}

DispatcherOwner::~DispatcherOwner() { Close(); }

auto DispatcherOwner::GetDispatcher() const noexcept -> Dispatcher { return Dispatcher{ state }; }

auto DispatcherOwner::Close() noexcept -> void { state->Close(); }

}
