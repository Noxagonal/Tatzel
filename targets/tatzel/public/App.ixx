/// @file
/// @brief Application lifetime and logical page registration.
module;

#include <deque>
#include <functional>
#include <string_view>

export module UI.App;

import UI.Page;
import UI.PageBuilder;

namespace tatzel {

/// @brief Owns registered logical page factories and runs the Drogon server.
/// @details One application may exist at a time. Register pages before Run().
/// Each browser connection receives a separate logical tree from its page factory.
export class App
{
public:
	/// @brief Loads runtime configuration and establishes the global application.
	/// @pre No other App exists; the working directory contains data/drogon_config.json.
	/// @post This object is returned by GetGlobalApp().
	/// @throws tatzel::Exception If another App already exists.
	App();

	/// @brief Prevents a second application owner.
	/// @param other Source application.
	/// @pre None.
	/// @post Copy construction is unavailable.
	App( const App& other ) = delete;

	/// @brief Preserves the global application's address.
	/// @param other Source application.
	/// @pre None.
	/// @post Move construction is unavailable.
	App( App&& other ) = delete;

	/// @brief Releases the global application identity.
	/// @pre Run() has returned and no callback uses this application.
	/// @post GetGlobalApp() no longer returns this object.
	~App();

	/// @brief Prevents duplicate application ownership.
	/// @param other Source application.
	/// @return No value; operation is deleted.
	/// @pre None.
	/// @post Copy assignment is unavailable.
	auto operator=( const App& other ) -> App& = delete;

	/// @brief Preserves global application identity.
	/// @param other Source application.
	/// @return No value; operation is deleted.
	/// @pre None.
	/// @post Move assignment is unavailable.
	auto operator=( App&& other ) -> App& = delete;

	/// @brief Registers an HTTP route and its per-client logical page factory.
	/// @param route_path Absolute route path, such as /.
	/// @param page_builder_fn Factory that creates one client's logical controls.
	/// @pre Run() has not started; the route is unique; captures outlive their use.
	/// @post HTTP requests receive the template and WebSocket connect builds this page.
	/// @throws tatzel::Exception If the route or factory is invalid or duplicated.
	/// @throws tatzel::Exception If registration happens after Run() starts.
	auto RegisterPage(
		std::string_view route_path,
		std::function<void( PageBuilder<> )>&& page_builder_fn
	) -> void;

	/// @brief Finds a registered logical page factory by route.
	/// @param route_path Exact route path to find.
	/// @return Borrowed page pointer, or nullptr; valid until application destruction.
	/// @pre No page registration is running concurrently.
	/// @post Registered pages are unchanged.
	auto FindRegisteredPage( std::string_view route_path ) -> Page*;

	/// @brief Registers the WebSocket controller and runs the server on port 8080.
	/// @pre Called once after all pages are registered; runtime assets are available.
	/// @post Returns after Drogon stops; the application cannot be run again.
	/// @throws tatzel::Exception If Run() has already been called.
	auto Run() -> void;

private:
	std::deque<Page> registered_page_list;
	bool has_run = false;
};

/// @brief Retrieves the single live application.
/// @return Borrowed application reference.
/// @pre An App has been constructed and is still alive.
/// @post Global application identity is unchanged.
/// @throws tatzel::Exception If no application exists.
export auto GetGlobalApp() -> App&;

}
