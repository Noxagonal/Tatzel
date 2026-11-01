/// @file
/// @brief Thread-safe weak handles for posting work to one logical page's event loop.
module;

#include <cstddef>
#include <functional>
#include <memory>
#include <source_location>

namespace trantor { class EventLoop; }

export module UI.Dispatcher;

namespace tatzel {

class DispatcherState;

/// @brief Outcome of submitting a callback, rather than its eventual execution.
export enum class DispatchResult
{
	Accepted, ///< Queued; closure can still cancel the callback before it starts.
	Closed, ///< The page or owning event loop has closed, or the handle is empty.
	QueueFull, ///< The page's pending-callback capacity has been reached.
};

/// @brief Queue limits selected by framework integration when a dispatcher is created.
export struct DispatchOptions
{
	size_t capacity = 1024; ///< Maximum pending callbacks, excluding the executing callback.
	size_t batch_size = 32; ///< Maximum callbacks per drain before yielding to event-loop I/O.
};

/// @brief Copyable non-owning handle for submitting short UI callbacks from any thread.
/// @details Captures must own their data or use checked references. Resolving an
/// element handle and reading or changing logical state requires the owning loop.
/// Different copies may be used concurrently; assigning the same handle object
/// concurrently requires synchronization. The handle retains no page or controls.
export class Dispatcher
{
public:
	/// @brief Creates an empty dispatcher handle.
	/// @pre None.
	/// @post Post returns Closed and IsCurrentThread returns false.
	Dispatcher() = default;

	/// @brief Queues an owned callback without invoking it inline on the submitting thread.
	/// @param callback Nonempty callable; accepted captures are released on execution or cancellation.
	/// @param origin Submission site, normally supplied by the language.
	/// @return Accepted, Closed, or QueueFull; acceptance does not guarantee execution.
	/// @pre Captures can be transferred between threads and their destruction does not throw.
	/// @post Accepted callbacks run sequentially in insertion order on the owning loop, unless cancelled by closure.
	/// @throws tatzel::Exception If callback is empty.
	/// @throws std::bad_alloc If allocating or scheduling accepted work fails.
	auto Post(
		std::move_only_function<void()> callback,
		std::source_location origin = std::source_location::current()
	) const -> DispatchResult;

	/// @brief Tests whether the caller is on this live dispatcher's owning thread.
	/// @return True only on the owning thread while submissions remain open.
	/// @pre None; may be called from any thread.
	/// @post No callback runs and no page or event-loop lifetime is retained.
	auto IsCurrentThread() const noexcept -> bool;

private:
	friend class DispatcherOwner;

	/// @brief Creates a weak handle for framework-owned queue state.
	/// @param state Non-owning dispatcher state.
	/// @pre state belongs to exactly one owning event loop.
	/// @post This handle retains no strong ownership.
	explicit Dispatcher( std::weak_ptr<DispatcherState> state );

	std::weak_ptr<DispatcherState> state;
};

/// @brief Framework service owning a dispatch queue and its page invocation boundary.
/// @details Construct and close on the owning event loop. Destruction must occur
/// on that loop or after it has stopped. Loop exit closes every queue before the
/// loop can be destroyed. Invokers must retain their page only during execution
/// and establish its logical removal scope. Application code uses Dispatcher.
export class DispatcherOwner
{
public:
	/// @brief Creates a bounded queue and associates it with an existing event loop.
	/// @param loop Owning loop, which outlives this service or closes it during loop exit.
	/// @param invoke Framework invoker receiving an owned callback and its submission site.
	/// @param options Nonzero capacity and batch size.
	/// @pre Called on loop's thread before loop exit; invoke is nonempty and its captures outlive this service.
	/// @post GetDispatcher returns an open weak handle and loop exit will cancel pending work.
	/// @throws tatzel::Exception If the invoker or limits are invalid, or loop is not the caller's current event loop.
	DispatcherOwner(
		trantor::EventLoop& loop,
		std::function<void( std::move_only_function<void()>&, std::source_location )> invoke,
		DispatchOptions options = {}
	);

	/// @brief Prevents duplicate ownership of a page's dispatch queue.
	/// @param other Source owner.
	/// @pre None.
	/// @post Copy construction is unavailable.
	DispatcherOwner( const DispatcherOwner& other ) = delete;

	/// @brief Keeps queue ownership attached to its session service.
	/// @param other Source owner.
	/// @pre None.
	/// @post Move construction is unavailable.
	DispatcherOwner( DispatcherOwner&& other ) = delete;

	/// @brief Cancels pending work before releasing the framework invoker.
	/// @pre Called on the owning loop or after it has stopped; capture destructors do not throw.
	/// @post Every retained Dispatcher rejects new work.
	~DispatcherOwner();

	/// @brief Prevents duplicate queue ownership.
	/// @param other Source owner.
	/// @return No value; the operation is deleted.
	/// @pre None.
	/// @post Copy assignment is unavailable.
	auto operator=( const DispatcherOwner& other ) -> DispatcherOwner& = delete;

	/// @brief Keeps queue ownership attached to its original service.
	/// @param other Source owner.
	/// @return No value; the operation is deleted.
	/// @pre None.
	/// @post Move assignment is unavailable.
	auto operator=( DispatcherOwner&& other ) -> DispatcherOwner& = delete;

	/// @brief Returns a transferable, non-owning submission handle.
	/// @return Copyable weak dispatcher handle.
	/// @pre This owner is alive and is not being destroyed concurrently.
	/// @post The queue and its owning page are not retained by the returned handle.
	auto GetDispatcher() const noexcept -> Dispatcher;

	/// @brief Permanently rejects submissions and releases all pending callbacks outside queue locks.
	/// @pre Called on the owning loop or after it has stopped; capture destructors do not throw.
	/// @post Pending callbacks are cancelled; a callback already executing is allowed to finish.
	auto Close() noexcept -> void;

private:
	std::shared_ptr<DispatcherState> state;
};

}
