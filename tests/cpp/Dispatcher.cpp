#include <trantor/net/EventLoop.h>
#include <trantor/utils/Logger.h>
#include <array>
#include <atomic>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <functional>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

import UI.Dispatcher;
import UI.Diagnostics;
import UI.Logging;
import UI.ClientElementTree;
import UI.Element.Text;

using tatzel::DispatchResult;

/// @brief Executes a queue callback for tests that do not own a logical page.
/// @param callback Owned callable under test.
/// @param origin Original submission site.
/// @pre Called on the queue's owning loop.
/// @post The callback has run or its exception propagates to the dispatcher boundary.
auto Invoke(
	std::move_only_function<void()>& callback,
	std::source_location origin
) -> void
{
	callback();
}

/// @brief Runs an observable action when its uniquely owned capture is released.
struct CaptureProbe
{
	std::function<void()> on_destroy;

	/// @brief Runs the capture-release probe.
	/// @pre on_destroy does not throw and its references remain alive.
	/// @post Release thread and lock reentrancy can be observed by the test.
	~CaptureProbe() { on_destroy(); }
};

/// @brief Text control measuring destruction at the dispatched removal boundary.
class TrackedText : public tatzel::ui::Text
{
public:
	/// @brief Creates a text control with an externally owned destruction counter.
	/// @param parent Owning logical parent.
	/// @param destroyed Counter incremented during destruction.
	/// @pre destroyed outlives this control.
	/// @post The control has a stable identity and owns its logical text.
	TrackedText(
		tatzel::ui::LogicalElement* parent,
		int& destroyed
	) :
		Text{ "tracked", parent, "Before" },
		destroyed{ destroyed }
	{}

	/// @brief Records the point at which removal releases the control.
	/// @pre destroyed remains alive.
	/// @post The counter has increased by one.
	~TrackedText() override { ++destroyed; }

private:
	int& destroyed;
};

/// @brief Checks deferred execution, FIFO ordering, move-only captures, and yielding to other loop work.
/// @pre No event loop exists on the current thread.
/// @post All accepted callbacks have run once on their owning thread.
auto CheckOrdering() -> void
{
	auto loop = trantor::EventLoop{};
	auto owner = tatzel::DispatcherOwner{ loop, Invoke, { .capacity = 16, .batch_size = 2 } };
	auto dispatcher = owner.GetDispatcher();
	auto order = std::vector<int>{};
	auto yielded = false;
	assert( dispatcher.IsCurrentThread() );

	auto producer = std::thread{ [ dispatcher, &order, &loop, &yielded ] {
		assert( !dispatcher.IsCurrentThread() );
		for( int value = 0; value < 6; ++value )
		{
			assert( dispatcher.Post( [ dispatcher, &order, &loop, &yielded, payload = std::make_unique<int>( value ) ] {
				assert( dispatcher.IsCurrentThread() );
				if( *payload == 0 ) loop.runAfter( 0.0, [ &yielded ] { yielded = true; } );
				order.push_back( *payload );
			} ) == DispatchResult::Accepted );
		}
	} };
	producer.join();
	assert( order.empty() );

	assert( dispatcher.Post( [ & ] {
		order.push_back( 6 );
		assert( dispatcher.Post( [ & ] { assert( yielded ); order.push_back( 7 ); loop.quit(); } ) == DispatchResult::Accepted );
		assert( order.back() == 6 );
	} ) == DispatchResult::Accepted );
	loop.queueInLoop( [ & ] { assert( order == std::vector<int>( { 0, 1 } ) ); } );
	loop.runAfter( 5.0, [] { std::abort(); } );
	loop.loop();
	assert( order == std::vector<int>( { 0, 1, 2, 3, 4, 5, 6, 7 } ) );
	assert( dispatcher.Post( [] {} ) == DispatchResult::Closed );
}

/// @brief Checks queue construction rejects invalid limits, empty invokers, and the wrong thread.
/// @pre No event loop exists on the current thread.
/// @post No invalid queue has registered work or acquired ownership of the loop.
auto CheckValidation() -> void
{
	auto loop = trantor::EventLoop{};
	try { tatzel::DispatcherOwner owner{ loop, Invoke, { .capacity = 0 } }; std::abort(); }
	catch( const tatzel::Exception& ) {}
	try { tatzel::DispatcherOwner owner{ loop, Invoke, { .batch_size = 0 } }; std::abort(); }
	catch( const tatzel::Exception& ) {}
	try { tatzel::DispatcherOwner owner{ loop, {} }; std::abort(); }
	catch( const tatzel::Exception& ) {}
	auto worker = std::thread{ [ &loop ] {
		try { tatzel::DispatcherOwner owner{ loop, Invoke }; std::abort(); }
		catch( const tatzel::Exception& ) {}
	} };
	worker.join();
	loop.queueInLoop( [ & ] { loop.quit(); } );
	loop.loop();
}

/// @brief Checks bounded capacity and capture destruction outside submission and cancellation locks.
/// @pre No event loop exists on the current thread.
/// @post Rejected and cancelled captures have been released exactly once without running their callbacks.
auto CheckCancellation() -> void
{
	auto loop = trantor::EventLoop{};
	auto owner = tatzel::DispatcherOwner{ loop, Invoke, { .capacity = 2 } };
	auto dispatcher = owner.GetDispatcher();
	auto released = 0;
	for( int index = 0; index < 2; ++index )
	{
		auto capture = std::make_unique<CaptureProbe>();
		capture->on_destroy = [ & ] {
			assert( !dispatcher.IsCurrentThread() );
			assert( dispatcher.Post( [] {} ) == DispatchResult::Closed );
			++released;
		};
		assert( dispatcher.Post( [ capture = std::move( capture ) ] { std::abort(); } ) == DispatchResult::Accepted );
	}
	auto capture = std::make_unique<CaptureProbe>();
	capture->on_destroy = [ & ] { assert( dispatcher.IsCurrentThread() ); ++released; };
	assert( dispatcher.Post( [ capture = std::move( capture ) ] {} ) == DispatchResult::QueueFull );
	assert( released == 1 );
	owner.Close();
	owner.Close();
	assert( released == 3 );
	loop.queueInLoop( [ & ] { loop.quit(); } );
	loop.loop();
}

/// @brief Checks logical removal, exception unwinding, submission metadata, and failure isolation.
/// @param output Buffer collecting dispatcher diagnostics on this thread.
/// @pre No event loop exists on the current thread; the test logger uses output.
/// @post Removed controls are released after callbacks and later work survives both traced and STL failures.
auto CheckFailures( std::string& output ) -> void
{
	auto loop = trantor::EventLoop{};
	auto tree = std::make_shared<tatzel::ClientElementTree>();
	auto destroyed = 0;
	auto* text = tree->root.InsertChild( std::make_unique<TrackedText>( &tree->root, destroyed ) );
	auto handle = text->GetHandle();
	auto owner = tatzel::DispatcherOwner{ loop, [ weak = std::weak_ptr{ tree } ](
		std::move_only_function<void()>& callback,
		std::source_location origin
	) {
		if( auto retained = weak.lock() )
		{
			auto scope = retained->root.BeginDispatch();
			callback();
		}
	} };
	auto dispatcher = owner.GetDispatcher();
	assert( dispatcher.Post( [ & ] {
		text->Remove();
		assert( !handle && destroyed == 0 );
		throw tatzel::Exception{ "Dispatched removal failed" };
	} ) == DispatchResult::Accepted );
	assert( dispatcher.Post( [] {
		auto values = std::vector<int>{};
		static_cast<void>( values.at( 0 ) );
	} ) == DispatchResult::Accepted );
	assert( dispatcher.Post( [ & ] {
		assert( destroyed == 1 && !handle );
		assert( output.contains( "Dispatched removal failed" ) && output.contains( "std::out_of_range" ) );
		assert( output.contains( "Submitted at:" ) && output.contains( "Dispatcher.cpp" ) );
		assert( output.contains( "Dispatched callback" ) );
		loop.quit();
	} ) == DispatchResult::Accepted );
	loop.loop();
}

/// @brief Checks concurrent producers preserve each producer's ordering on the UI loop.
/// @pre No event loop exists on the current thread.
/// @post Every accepted callback ran exactly once and observed its owning thread.
auto CheckConcurrentProducers() -> void
{
	auto loop = trantor::EventLoop{};
	auto owner = tatzel::DispatcherOwner{ loop, Invoke };
	auto dispatcher = owner.GetDispatcher();
	auto seen = std::array<int, 4>{};
	auto completed = 0;
	auto workers = std::vector<std::thread>{};
	for( size_t producer = 0; producer < seen.size(); ++producer )
	{
		workers.emplace_back( [ &, producer ] {
			for( int value = 0; value < 128; ++value )
			{
				assert( dispatcher.Post( [ &, producer, value ] {
					assert( dispatcher.IsCurrentThread() && seen[ producer ]++ == value );
					if( ++completed == 512 ) loop.quit();
				} ) == DispatchResult::Accepted );
			}
		} );
	}
	for( auto& worker : workers ) worker.join();
	loop.runAfter( 5.0, [] { std::abort(); } );
	loop.loop();
	assert( completed == 512 );
}

/// @brief Races submissions with loop exit and destruction while retaining an owner and weak handle.
/// @pre No event loop exists on the current thread.
/// @post Surviving handles reject submissions safely after the actual event loop has been destroyed.
auto CheckLoopShutdown() -> void
{
	auto loop = std::make_unique<trantor::EventLoop>();
	auto owner = std::make_unique<tatzel::DispatcherOwner>( *loop, Invoke );
	auto dispatcher = owner->GetDispatcher();
	auto completed = 0;
	auto stop = std::atomic<bool>{ false };
	auto producer = std::thread{ [ & ] {
		while( !stop.load() )
		{
			auto result = dispatcher.Post( [ & ] { if( ++completed == 50 ) loop->quit(); } );
			assert( result == DispatchResult::Accepted || result == DispatchResult::QueueFull || result == DispatchResult::Closed );
			std::this_thread::yield();
		}
	} };
	loop->runAfter( 5.0, [] { std::abort(); } );
	loop->loop();
	try { tatzel::DispatcherOwner invalid{ *loop, Invoke }; std::abort(); }
	catch( const tatzel::Exception& ) {}
	loop.reset();
	for( int index = 0; index < 100; ++index ) assert( dispatcher.Post( [] {} ) == DispatchResult::Closed );
	assert( !dispatcher.IsCurrentThread() );
	stop.store( true );
	producer.join();
	assert( completed == 50 );
	owner.reset();
	assert( dispatcher.Post( [] {} ) == DispatchResult::Closed );
}

/// @brief Checks weak page ownership and cancellation when the dispatcher owner is released.
/// @pre No event loop exists on the current thread.
/// @post Dispatcher handles retain neither their owner nor a page captured weakly by its invoker.
auto CheckWeakOwnership() -> void
{
	auto loop = trantor::EventLoop{};
	auto page = std::make_shared<int>( 1 );
	auto weak_page = std::weak_ptr{ page };
	auto owner = std::make_unique<tatzel::DispatcherOwner>( loop, [ weak_page ](
		std::move_only_function<void()>& callback,
		std::source_location origin
	) { if( auto retained = weak_page.lock() ) callback(); } );
	auto dispatcher = owner->GetDispatcher();
	page.reset();
	assert( weak_page.expired() );
	assert( dispatcher.Post( [] { std::abort(); } ) == DispatchResult::Accepted );
	loop.queueInLoop( [ & ] { owner.reset(); loop.quit(); } );
	loop.loop();
	assert( dispatcher.Post( [] {} ) == DispatchResult::Closed );
}

/// @brief Checks closure inside an executing callback cancels later work without destroying the active capture.
/// @pre No event loop exists on the current thread.
/// @post The current callback finishes normally and the pending callback never starts.
auto CheckCloseDuringExecution() -> void
{
	auto loop = trantor::EventLoop{};
	auto owner = tatzel::DispatcherOwner{ loop, Invoke };
	auto dispatcher = owner.GetDispatcher();
	auto released = false;
	auto capture = std::make_unique<CaptureProbe>();
	capture->on_destroy = [ & ] { released = true; };
	assert( dispatcher.Post( [ &, capture = std::move( capture ) ] {
		owner.Close();
		assert( !released && dispatcher.Post( [] {} ) == DispatchResult::Closed );
		loop.quit();
	} ) == DispatchResult::Accepted );
	assert( dispatcher.Post( [] { std::abort(); } ) == DispatchResult::Accepted );
	loop.loop();
	assert( released );
}

/// @brief Exercises real event-loop dispatch, concurrency, lifetime, cancellation, and diagnostic boundaries.
/// @return Zero when all checks pass.
/// @pre Built with assertions enabled; the process has no running server.
/// @post All loops, workers, captures, and logical controls have been released.
auto main() -> int
{
	auto output = std::string{};
	trantor::Logger::setOutputFunction( [ &output ](
		const char* message,
		uint64_t length
	) { output.append( message, length ); }, [] {} );
	auto empty = tatzel::Dispatcher{};
	assert( !empty.IsCurrentThread() && empty.Post( [] {} ) == DispatchResult::Closed );
	try { empty.Post( {} ); std::abort(); }
	catch( const tatzel::Exception& ) {}
	CheckOrdering();
	CheckValidation();
	CheckCancellation();
	CheckFailures( output );
	CheckConcurrentProducers();
	CheckLoopShutdown();
	CheckWeakOwnership();
	CheckCloseDuringExecution();
	trantor::Logger::setOutputFunction( []( const char*, uint64_t ) {}, [] {} );
}
