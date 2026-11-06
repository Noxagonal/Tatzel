/// @file
/// @brief Application lifetime and logical page registration.
module;

#include <deque>
#include <functional>
#include <string>
#include <string_view>

export module UI.App;

import UI.Page;
import UI.PageBuilder;

namespace tatzel {

/// @brief Owns runtime resource locations for a server application.
/// @details Relative file paths use resource_directory. An empty resource directory
/// selects TATZEL_RESOURCE_DIRECTORY, an adjacent tatzel-runtime directory on Linux,
/// then the configured package/build runtime.
export struct AppOptions
{
	std::string resource_directory; ///< Runtime root; empty selects automatic discovery.
	std::string configuration_path = "data/drogon_config.json"; ///< Drogon configuration, or empty for its defaults.
	std::string template_path = "data/template.html"; ///< Nonempty HTML document path.
	std::string document_root = "www"; ///< Static root; empty retains the Drogon configuration's value.
};

/// @brief Owns registered logical page factories and runs the Drogon server.
/// @details One application may exist at a time. Register pages before Run().
/// Each browser connection receives a separate logical tree from its page factory.
export class App
{
public:
	/// @brief Resolves runtime resources, loads configuration, and establishes the global application.
	/// @param options Owned paths; relative file paths use the selected runtime root.
	/// @pre No other App exists; runtime resources and any configuration file are available.
	/// @post This object is returned by GetGlobalApp(); paths are independent of later working-directory changes.
	/// @throws tatzel::Exception If another App exists or the runtime/template location is invalid.
	App( AppOptions options = {} );

	/// @brief Loads explicit legacy file paths relative to the current working directory.
	/// @param configuration_path Drogon JSON configuration path, copied during construction.
	/// @param template_path HTML document path, owned for registered route responses.
	/// @pre No other App exists; the configuration file is readable.
	/// @post This object is returned by GetGlobalApp().
	/// @throws tatzel::Exception If another App already exists.
	App(
		std::string_view configuration_path,
		std::string_view template_path = "./data/template.html"
	);

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

	/// @brief Reads the resolved resource directory for application-owned runtime files.
	/// @return Borrowed absolute directory path valid for this application's lifetime.
	/// @pre This application is alive.
	/// @post Runtime configuration is unchanged.
	auto GetResourceDirectory() const noexcept -> std::string_view { return resource_directory; }

	/// @brief Registers the WebSocket controller and runs the server on port 8080.
	/// @pre Called once after all pages are registered; runtime assets are available.
	/// @post Returns after Drogon stops; the application cannot be run again.
	/// @throws tatzel::Exception If Run() has already been called.
	auto Run() -> void;

private:
	std::string resource_directory;
	std::string template_path;
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
