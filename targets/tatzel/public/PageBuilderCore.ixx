module;

#include <memory>

export module UI.PageBuilderCore;

import UI.ClientDOMTree;
import UI.ClientUpdater;
import UI.UI.LogicalElement;
import UI.ElementAdapter.ElementAdapter;


namespace tatzel {


export
class PageBuilderCore
{
public:
	inline PageBuilderCore(
		ClientDOMTree& dom_tree,
		ClientUpdater& client_updater,
		ElementAdapter& element_adapter
	) :
		dom_tree( &dom_tree ),
		root_element( &dom_tree.root ),
		client_updater( &client_updater ),
		element_adapter( &element_adapter )
	{}

	PageBuilderCore( const PageBuilderCore& ) = default;
	~PageBuilderCore() = default;

	auto operator=( const PageBuilderCore& ) -> PageBuilderCore& = default;

	inline auto GetDOMTree() { return dom_tree; }
	inline auto GetRootElement() { return root_element; }
	inline auto GetClientUpdater() { return client_updater; }
	inline auto GetElementAdapter() { return element_adapter; }

private:

	ClientDOMTree* dom_tree = nullptr;
	ui::LogicalElement* root_element = nullptr;
	ClientUpdater* client_updater = nullptr;
	ElementAdapter* element_adapter = nullptr;
};


}
