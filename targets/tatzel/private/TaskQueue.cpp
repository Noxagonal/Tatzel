module;

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <exception>
#include <functional>
#include <memory>
#include <mutex>
#include <source_location>
#include <thread>
#include <utility>
#include <vector>

module UI.TaskQueue;

import UI.Diagnostics;
import UI.Logging;

/// @brief Owns bounded pending work and joins workers after submission closes.
struct [[gnu::visibility( "hidden" )]] tatzel::TaskQueue::Implementation
{
	/// @brief Owns one callback and the location from which it was submitted.
	struct Task
	{
		std::move_only_function<void()> callback;
		std::source_location origin;
	};

	std::mutex mutex;
	std::condition_variable ready;
	std::deque<std::unique_ptr<Task>> pending;
	std::vector<std::jthread> workers;
	std::mutex shutdown_mutex;
	std::size_t capacity;
	bool accepting = true;
	static thread_local Implementation* current;

	/// @brief Starts workers and safely joins any workers started before a construction failure.
	/// @param options Positive worker and capacity limits.
	/// @pre Limits have been validated.
	/// @post Workers wait for submitted tasks, or construction has failed without stranded threads.
	explicit Implementation( TaskQueueOptions options ) :
		capacity{ options.capacity }
	{
		try
		{
			workers.reserve( options.worker_count );
			for( std::size_t index = 0; index < options.worker_count; ++index )
				workers.emplace_back( [ this ] { Run(); } );
		}
		catch( ... )
		{
			{
				auto lock = std::lock_guard{ mutex };
				accepting = false;
			}
			ready.notify_all();
			workers.clear();
			throw;
		}
	}

	/// @brief Executes queued work and contains each application failure outside synchronization locks.
	/// @pre This state outlives the worker.
	/// @post Returns only after submission closes and the pending queue is empty.
	auto Run() -> void
	{
		current = this;
		for( ;; )
		{
			auto task = std::unique_ptr<Task>{};
			{
				auto lock = std::unique_lock{ mutex };
				ready.wait( lock, [ this ] { return !accepting || !pending.empty(); } );
				if( pending.empty() ) break;
				task = std::move( pending.front() );
				pending.pop_front();
			}

			try
			{
				auto boundary = CallbackScope{ "Background task" };
				task->callback();
			}
			catch( ... )
			{
				LogException( std::current_exception(), { {}, "background_task", {}, {}, task->origin } );
			}
		}
		current = nullptr;
	}
};

thread_local tatzel::TaskQueue::Implementation* tatzel::TaskQueue::Implementation::current = nullptr;

tatzel::TaskQueue::TaskQueue( TaskQueueOptions options )
{
	if( !options.worker_count || !options.capacity ) throw Exception{ "Task queue requires nonzero worker and capacity limits" };
	implementation = std::make_unique<Implementation>( options );
}

tatzel::TaskQueue::~TaskQueue() { Shutdown(); }

auto tatzel::TaskQueue::Post(
	std::move_only_function<void()> callback,
	std::source_location origin
) -> TaskQueueResult
{
	if( !callback ) throw Exception{ "A background task must not be empty" };
	auto task = std::make_unique<Implementation::Task>( std::move( callback ), origin );
	{
		auto lock = std::lock_guard{ implementation->mutex };
		if( !implementation->accepting ) return TaskQueueResult::Closed;
		if( implementation->pending.size() >= implementation->capacity ) return TaskQueueResult::QueueFull;
		implementation->pending.push_back( std::move( task ) );
	}
	implementation->ready.notify_one();
	return TaskQueueResult::Accepted;
}

auto tatzel::TaskQueue::Shutdown() -> void
{
	if( Implementation::current == implementation.get() ) throw Exception{ "A task queue worker cannot join its own queue" };
	auto shutdown_lock = std::lock_guard{ implementation->shutdown_mutex };
	{
		auto lock = std::lock_guard{ implementation->mutex };
		implementation->accepting = false;
	}
	implementation->ready.notify_all();
	for( auto& worker : implementation->workers )
		if( worker.joinable() ) worker.join();
}
