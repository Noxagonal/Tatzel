/// @file
/// @brief Borrowed framework context shared by scoped logical builders.
module;

#include <functional>
#include <string>
#include <string_view>
#include <utility>

export module UI.PageBuilderCore;

import UI.Diagnostics;

import UI.ClientElementTree;
import UI.ClientUpdater;
import UI.ElementAdapter.ElementAdapter;
import UI.Dispatcher;

namespace tatzel {

/// @brief Gives builders access to the client-owned logical tree, adapter, and DOM sender.
export class PageBuilderCore
{
public:
	/// @brief Binds the framework services used by scoped builders.
	/// @param element_tree Logical tree owned by the client connection.
	/// @param client_updater DOM protocol sender owned by the client connection.
	/// @param element_adapter Logical-to-DOM adapter owned by the client connection.
	/// @param dispatcher Weak submission handle for this client's UI event loop; empty for standalone integration.
	/// @pre All services outlive this context and every builder copied from it.
	/// @post Accessors return the supplied services without taking ownership.
	PageBuilderCore(
		ClientElementTree& element_tree,
		ClientUpdater& client_updater,
		ElementAdapter& element_adapter,
		Dispatcher dispatcher = {}
	) :
		element_tree{ &element_tree },
		client_updater{ &client_updater },
		element_adapter{ &element_adapter },
		dispatcher{ dispatcher }
	{}

	/// @brief Reads the logical tree.
	/// @return Borrowed tree.
	/// @pre Bound services are alive.
	/// @post State is unchanged.
	auto GetElementTree() const noexcept -> ClientElementTree* { return element_tree; }

	/// @brief Reads the logical root.
	/// @return Borrowed root control.
	/// @pre Bound services are alive.
	/// @post State is unchanged.
	auto GetRootElement() const noexcept -> ui::LogicalElement* { return &element_tree->root; }

	/// @brief Reads the DOM message sender.
	/// @return Borrowed updater.
	/// @pre Bound services are alive.
	/// @post State is unchanged.
	auto GetClientUpdater() const noexcept -> ClientUpdater* { return client_updater; }

	/// @brief Reads the logical-to-DOM adapter.
	/// @return Borrowed adapter.
	/// @pre Bound services are alive.
	/// @post State is unchanged.
	auto GetElementAdapter() const noexcept -> ElementAdapter* { return element_adapter; }

	/// @brief Copies the non-owning dispatcher for this logical page.
	/// @return Thread-transferable submission handle, possibly empty for standalone contexts.
	/// @pre This context is alive and accessed on its owning loop.
	/// @post The returned handle retains neither this context nor its services.
	auto GetDispatcher() const noexcept -> Dispatcher { return dispatcher; }

	/// @brief Replaces the page-local hash navigation callback.
	/// @param handler Callback receiving owned current-location storage during invocation.
	/// @pre Called on the owning loop; captures outlive their invocation.
	/// @post Subsequent navigation invokes the replacement; current location is unchanged.
	auto SetNavigationHandler( std::function<void( std::string_view )> handler ) -> void
	{
		navigation_handler = std::move( handler );
	}

	/// @brief Accepts browser navigation or requests a same-page history entry.
	/// @param hash Empty or hash-prefixed location of at most 2048 bytes.
	/// @param update_browser Whether to queue a browser history update.
	/// @pre Called on the live page's owning loop.
	/// @post The location is owned and the application callback has run; browser updates are queued.
	/// @throws tatzel::Exception If hash is not a supported same-page location.
	auto Navigate(
		std::string_view hash,
		bool update_browser = true
	) -> void
	{
		if( hash.size() > 2048 || ( !hash.empty() && hash.front() != '#' ) )
			throw Exception{ "Navigation requires a hash location of at most 2048 bytes" };
		auto location = std::string{ hash };
		if( update_browser ) client_updater->SetLocation( location );
		auto handler = navigation_handler;
		if( handler )
		{
			auto boundary = CallbackScope{ "Page navigation" };
			handler( location );
		}
	}

private:
	std::function<void( std::string_view )> navigation_handler;
	ClientElementTree* element_tree;
	ClientUpdater* client_updater;
	ElementAdapter* element_adapter;
	Dispatcher dispatcher;
};

}
