/// @file
/// @brief Borrowed framework context shared by scoped logical builders.
module;

export module UI.PageBuilderCore;

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

private:
	ClientElementTree* element_tree;
	ClientUpdater* client_updater;
	ElementAdapter* element_adapter;
	Dispatcher dispatcher;
};

}
