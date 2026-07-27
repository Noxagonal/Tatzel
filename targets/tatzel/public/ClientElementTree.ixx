module;

#include <string_view>

export module UI.ClientElementTree;

import UI.UI.LogicalElement;


namespace tatzel {


export
class ClientElementTree
{
public:
	ClientElementTree();
	ClientElementTree( const ClientElementTree& ) = delete;
	ClientElementTree( ClientElementTree&& ) = default;
	~ClientElementTree();

	auto operator=( const ClientElementTree& ) -> ClientElementTree& = delete;
	auto operator=( ClientElementTree&& ) -> ClientElementTree& = default;

	template<typename ElementT>
	auto FindElementById( std::string_view id ) -> ElementT*
	{
		auto RecursiveSearch = [id](this auto& self, ui::LogicalElement& element) -> ui::LogicalElement*
		{
			if( element.GetID() == id ) return &element;
			for( const auto& child : element.GetChildren() )
			{
				auto found = self( *child );
				if( found ) return found;
			}
			return nullptr;
		};

		return dynamic_cast<ElementT*>( RecursiveSearch( root ) );
	}

	ui::LogicalElement root;
};


}
