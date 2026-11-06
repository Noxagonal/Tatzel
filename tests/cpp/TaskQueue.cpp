#include <atomic>
#include <barrier>
#include <cassert>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>

import UI.TaskQueue;
import UI.Diagnostics;

/// @brief Checks that releasing a capture can reenter queue submission without a locked mutex.
struct CaptureProbe
{
	std::function<void()> on_release;

	/// @brief Runs the externally owned release assertion.
	/// @pre The callback and referenced queue remain alive and do not throw.
	/// @post Capture release has been observed outside the queue's synchronization lock.
	~CaptureProbe() { on_release(); }
};

/// @brief Checks ownership, draining, bounded rejection, failures, and concurrent shutdown.
/// @return Zero when the queue's application-facing lifetime contract holds.
/// @pre Assertions are enabled and the actual shared library is linked.
/// @post Every test worker is joined and all accepted captures have been released.
auto main() -> int
{
	using tatzel::TaskQueueResult;
	try { tatzel::TaskQueue invalid{ { .worker_count = 0 } }; assert( false ); }
	catch( const tatzel::Exception& ) {}
	try { tatzel::TaskQueue invalid{ { .capacity = 0 } }; assert( false ); }
	catch( const tatzel::Exception& ) {}

	auto order = std::vector<int>{};
	{
		auto queue = tatzel::TaskQueue{};
		try { queue.Post( {} ); assert( false ); }
		catch( const tatzel::Exception& ) {}
		for( int index = 0; index < 20; ++index )
			assert( queue.Post( [ &order, value = std::make_unique<int>( index ) ] { order.push_back( *value ); } ) == TaskQueueResult::Accepted );
		assert( queue.Post( [] { throw tatzel::Exception{ "Expected background task failure" }; } ) == TaskQueueResult::Accepted );
		assert( queue.Post( [ &order ] { order.push_back( 20 ); } ) == TaskQueueResult::Accepted );
	}
	assert( order.size() == 21 );
	for( int index = 0; index < 21; ++index ) assert( order[ index ] == index );

	auto queue = tatzel::TaskQueue{ { .capacity = 1 } };
	auto started = std::promise<void>{};
	auto release = std::promise<void>{};
	auto release_signal = release.get_future();
	assert( queue.Post( [ & ] { started.set_value(); release_signal.wait(); } ) == TaskQueueResult::Accepted );
	started.get_future().wait();
	auto completed = std::atomic<int>{ 0 };
	assert( queue.Post( [ & ] { ++completed; } ) == TaskQueueResult::Accepted );
	auto released = 0;
	auto full_capture = std::make_unique<CaptureProbe>( [ & ] {
		assert( queue.Post( [] {} ) == TaskQueueResult::QueueFull );
		++released;
	} );
	assert( queue.Post( [ capture = std::move( full_capture ) ] { assert( false ); } ) == TaskQueueResult::QueueFull );
	assert( released == 1 );
	release.set_value();
	queue.Shutdown();
	assert( completed == 1 );
	assert( queue.Post( [] { assert( false ); } ) == TaskQueueResult::Closed );
	auto closed_capture = std::make_unique<CaptureProbe>( [ & ] {
		assert( queue.Post( [] {} ) == TaskQueueResult::Closed );
		++released;
	} );
	assert( queue.Post( [ capture = std::move( closed_capture ) ] { assert( false ); } ) == TaskQueueResult::Closed );
	assert( released == 2 );
	queue.Shutdown();

	auto parallel = tatzel::TaskQueue{ { .worker_count = 3, .capacity = 512 } };
	auto accepted = std::atomic<int>{ 0 };
	auto executed = std::atomic<int>{ 0 };
	auto producers = std::vector<std::jthread>{};
	for( int producer = 0; producer < 4; ++producer )
		producers.emplace_back( [ & ] {
			for( int index = 0; index < 100; ++index )
				if( parallel.Post( [ & ] { ++executed; } ) == TaskQueueResult::Accepted ) ++accepted;
		} );
	for( auto& producer : producers ) producer.join();
	auto first_shutdown = std::jthread{ [ & ] { parallel.Shutdown(); } };
	parallel.Shutdown();
	first_shutdown.join();
	assert( accepted == 400 && executed == accepted );

	auto racing = tatzel::TaskQueue{ { .worker_count = 2, .capacity = 512 } };
	auto start = std::barrier{ 6 };
	accepted = 0;
	executed = 0;
	producers.clear();
	for( int producer = 0; producer < 4; ++producer )
		producers.emplace_back( [ & ] {
			start.arrive_and_wait();
			for( int index = 0; index < 100; ++index )
				if( racing.Post( [ & ] { ++executed; } ) == TaskQueueResult::Accepted ) ++accepted;
		} );
	auto closer = std::jthread{ [ & ] { start.arrive_and_wait(); racing.Shutdown(); } };
	start.arrive_and_wait();
	for( auto& producer : producers ) producer.join();
	closer.join();
	assert( executed == accepted );

	auto self_join = tatzel::TaskQueue{};
	auto rejected = std::promise<bool>{};
	assert( self_join.Post( [ & ] {
		try { self_join.Shutdown(); rejected.set_value( false ); }
		catch( const tatzel::Exception& ) { rejected.set_value( true ); }
	} ) == TaskQueueResult::Accepted );
	assert( rejected.get_future().get() );
	self_join.Shutdown();
}
