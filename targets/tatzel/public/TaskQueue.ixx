/// @file
/// @brief Owned background callbacks with bounded submission and draining shutdown.
module;

#include <cstddef>
#include <functional>
#include <memory>
#include <source_location>

export module UI.TaskQueue;

namespace tatzel {

/// @brief Result of submitting a callback, rather than the result of executing it.
export enum class TaskQueueResult
{
	Accepted, ///< The queue owns the task and will run it before draining shutdown completes.
	Closed, ///< Shutdown has started; the callback will not execute.
	QueueFull, ///< The pending-task limit has been reached; the callback will not execute.
};

/// @brief Worker and pending-storage limits for a background queue.
export struct TaskQueueOptions
{
	std::size_t worker_count = 1; ///< Positive number of worker threads.
	std::size_t capacity = 1024; ///< Positive pending-task limit, excluding running tasks.
};

/// @brief Owns worker threads and FIFO pending callbacks without retaining any UI page.
/// @details Post and Shutdown may be called concurrently. Tasks run outside queue
/// locks, and multiple workers may finish tasks out of order. UI work must return
/// through Dispatcher. Destruction drains accepted tasks and joins all workers.
/// Destroy this queue only after its callers stop and from a non-worker thread.
export class TaskQueue
{
public:
	/// @brief Starts background workers with the requested limits.
	/// @param options Positive worker and pending-task counts.
	/// @pre None.
	/// @post The queue accepts callbacks; no application work has run yet.
	/// @throws tatzel::Exception If either limit is zero.
	TaskQueue( TaskQueueOptions options = {} );

	/// @brief Drains accepted callbacks and releases joined worker threads.
	/// @pre No caller uses this object concurrently; destruction is outside its workers.
	/// @post Every accepted callback has finished and its captures have been released.
	~TaskQueue();

	/// @brief Prevents duplicate ownership of workers.
	/// @param other Source queue.
	/// @pre None.
	/// @post Copy construction is unavailable.
	TaskQueue( const TaskQueue& other ) = delete;

	/// @brief Keeps the queue at a stable address for its workers and callers.
	/// @param other Source queue.
	/// @pre None.
	/// @post Move construction is unavailable.
	TaskQueue( TaskQueue&& other ) = delete;

	/// @brief Prevents replacement of a running queue.
	/// @param other Source queue.
	/// @return No value; assignment is unavailable.
	/// @pre None.
	/// @post Copy assignment is unavailable.
	auto operator=( const TaskQueue& other ) -> TaskQueue& = delete;

	/// @brief Prevents relocating worker ownership.
	/// @param other Source queue.
	/// @return No value; assignment is unavailable.
	/// @pre None.
	/// @post Move assignment is unavailable.
	auto operator=( TaskQueue&& other ) -> TaskQueue& = delete;

	/// @brief Offers an owned callback without waiting for queue capacity or task completion.
	/// @param callback Nonempty callback with thread-transferable owned captures.
	/// @param origin Submission location recorded when a task fails.
	/// @return Accepted, Closed, or QueueFull; rejected captures are released before return.
	/// @pre Capture destructors do not throw; this queue remains alive throughout the call.
	/// @post Accepted work runs once on a worker; failures are logged and other work continues.
	/// @throws tatzel::Exception If callback is empty.
	auto Post(
		std::move_only_function<void()> callback,
		std::source_location origin = std::source_location::current()
	) -> TaskQueueResult;

	/// @brief Stops submission, drains accepted work, and joins workers; repeated calls are harmless.
	/// @pre Called outside this queue's workers; accepted tasks eventually finish.
	/// @post Submission returns Closed and every accepted task and capture has completed.
	/// @throws tatzel::Exception If called from one of this queue's workers.
	auto Shutdown() -> void;

private:
	/// @brief Owns synchronization and worker state behind the installed interface.
	struct Implementation;
	std::unique_ptr<Implementation> implementation;
};

}
