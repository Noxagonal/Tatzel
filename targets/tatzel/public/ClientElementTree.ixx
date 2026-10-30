/**
 * @file
 * @brief Ownership and lookup of one client's logical control tree.
 */
module;

#include <algorithm>
#include <concepts>
#include <string_view>

export module UI.ClientElementTree;

import UI.Element.LogicalElement;

namespace tatzel {

/** @brief Owns one client's logical root and resolves logical IDs or browser part IDs. */
export class ClientElementTree
{
public:
	/** @brief Creates the logical root matching the HTML template. @pre None. @post The empty root maps to browser node root|main. */
	ClientElementTree();

	/**
	 * @brief Resolves an application-facing logical ID.
	 * @tparam ElementT Requested polymorphic control or event interface.
	 * @param id Logical ID to find.
	 * @return Matching typed logical control, or nullptr.
	 * @pre The tree is accessed on its owning event loop.
	 * @post Ownership and tree state are unchanged.
	 */
	template<typename ElementT>
	auto FindElementByID( std::string_view id ) -> ElementT*
	{
		return dynamic_cast<ElementT*>( FindLogicalElement( root, id, false ) );
	}

	/**
	 * @brief Resolves a concrete browser part to its logical owner.
	 * @tparam ElementT Requested polymorphic control or event interface.
	 * @param part_id Exact browser DOM ID; it is never parsed as a logical ID.
	 * @return Matching typed logical owner, or nullptr.
	 * @pre The tree is accessed on its owning event loop.
	 * @post Ownership and tree state are unchanged.
	 */
	template<typename ElementT>
	auto FindElementByPartID( std::string_view part_id ) -> ElementT*
	{
		return dynamic_cast<ElementT*>( FindLogicalElement( root, part_id, true ) );
	}

	/** @brief Root logical control whose DOM representation exists in the template. */
	ui::LogicalElement root;

private:
	/**
	 * @brief Searches a logical subtree for either kind of identity.
	 * @param element Current logical subtree root.
	 * @param id Identity to match.
	 * @param match_part Whether to compare DOM part IDs instead of logical IDs.
	 * @return Logical owner of the identity, or nullptr.
	 * @pre The subtree is not being mutated concurrently.
	 * @post The subtree is unchanged.
	 */
	static auto FindLogicalElement( ui::LogicalElement& element, std::string_view id, bool match_part ) -> ui::LogicalElement*
	{
		if( !match_part && element.GetID() == id ) return &element;
		if( match_part && std::ranges::any_of( element.GetParts(),
			[ id ]( const auto& part ) { return part.GetPartID() == id; } ) ) return &element;

		for( const auto& child : element.GetChildren() )
		{
			if( auto* found = FindLogicalElement( *child, id, match_part ) ) return found;
		}
		return nullptr;
	}
};

}
