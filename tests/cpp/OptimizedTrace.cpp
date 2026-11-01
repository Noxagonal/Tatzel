#include <iostream>
#include <string_view>

import UI.Diagnostics;

/// @brief Throws a traced exception from optimized application code.
/// @pre An application callback scope is active.
/// @post Never returns; metadata describes this helper even if its frame is inlined.
/// @throws tatzel::Exception Always, to check release diagnostic capture.
auto FailOptimizedCallback() -> void
{
	throw tatzel::Exception{ "Optimized callback failed" };
}

/// @brief Invokes application code with an optimized native callback boundary.
/// @pre Called on the current thread without crossing coroutine boundaries.
/// @post Exception unwinding restores the preceding diagnostic scope.
/// @throws tatzel::Exception From the application helper.
auto InvokeOptimizedCallback() -> void
{
	auto scope = tatzel::CallbackScope{ "Optimized callback" };
	FailOptimizedCallback();
}

/// @brief Checks the actual diagnostic module with optimization and NDEBUG enabled.
/// @return Zero when original metadata and a bounded native trace survive unwinding.
/// @pre The target enables optimization, symbols, and NDEBUG independently of the main build type.
/// @post Failures are described without relying on assertions removed in release mode.
auto main() -> int
{
#ifndef NDEBUG
	std::cerr << "The optimized trace target requires NDEBUG\n";
	return 1;
#endif
	try { InvokeOptimizedCallback(); }
	catch( const tatzel::Exception& error )
	{
		const auto valid = error.GetCallbackName() == "Optimized callback" && error.HasCallbackBoundary()
			&& error.GetVisibleFrameCount() > 0 && error.GetVisibleFrameCount() < error.GetStackTrace().size()
			&& std::string_view{ error.GetLocation().function_name() }.contains( "FailOptimizedCallback" );
		if( valid ) return 0;
		std::cerr << "Optimized capture failed: callback=" << error.GetCallbackName()
			<< ", visible=" << error.GetVisibleFrameCount() << ", raw=" << error.GetStackTrace().size() << '\n';
		return 1;
	}
	std::cerr << "The optimized callback did not throw\n";
	return 1;
}
