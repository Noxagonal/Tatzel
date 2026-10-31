/// @file
/// @brief Registered per-client logical page factory.
module;

#include <functional>
#include <string>
#include <string_view>

export module UI.Page;

import UI.PageBuilder;

namespace tatzel {

/// @brief Associates a route with the factory used to build each client's logical controls.
export class Page
{
public:
	/// @brief Stores an owned route and a logical page factory.
	/// @param route_path Registered absolute route.
	/// @param page_builder_fn Per-client logical page factory.
	/// @pre The route and factory are valid; captures outlive their use.
	/// @post The route is copied and the factory is owned by this page.
	Page(
		std::string_view route_path,
		std::function<void( PageBuilder<> )>&& page_builder_fn
	);

	/// @brief Exact registered HTTP and WebSocket route path.
	std::string route_path;

	/// @brief Factory invoked once for each successfully connected client page.
	std::function<void( PageBuilder<> )> page_builder_fn;
};

}
