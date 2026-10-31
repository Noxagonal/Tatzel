/// @file
/// @brief Ownership and lookup of one client's logical control tree.
module;

#include <algorithm>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <vector>
#include <string_view>

export module UI.ClientElementTree;

import UI.Element.LogicalElement;
import UI.DOM.ElementPart;

namespace tatzel {

/// @brief Owns one client's logical root and resolves logical IDs or browser part IDs.
export class ClientElementTree
{
public:
	/// @brief Creates the logical root matching the HTML template.
	/// @pre None.
	/// @post The empty root maps to browser node root|main.
	ClientElementTree();

	/// @brief Invalidates every active handle and disconnects services before destroying page controls.
	/// @pre No execution scope or callback is using this tree.
	/// @post Active and retired controls are destroyed; retained weak handles cannot resolve.
	~ClientElementTree();

	/// @brief Binds the page's concrete DOM-root removal sender.
	/// @param handler Callback receiving the rendered roots of removed subtrees; empty disables transport updates.
	/// @pre Called on the owning event loop; captured services outlive this tree.
	/// @post Future logical removals queue concrete root removal through handler.
	auto SetRemovalHandler( std::function<void( std::span<const dom::ElementPart> )> handler ) -> void;

	/// @brief Resolves an application-facing logical ID.
	/// @tparam ElementT Requested polymorphic control or event interface.
	/// @param id Logical ID to find.
	/// @return Matching typed logical control, or nullptr.
	/// @pre The tree is accessed on its owning event loop.
	/// @post Ownership and tree state are unchanged.
	template<typename ElementT>
	auto FindElementByID( std::string_view id ) -> ElementT*
	{
		return dynamic_cast<ElementT*>( FindLogicalElement( root, id, false ) );
	}

	/// @brief Resolves a concrete browser part to its logical owner.
	/// @tparam ElementT Requested polymorphic control or event interface.
	/// @param part_id Exact browser DOM ID; it is never parsed as a logical ID.
	/// @return Matching typed logical owner, or nullptr.
	/// @pre The tree is accessed on its owning event loop.
	/// @post Ownership and tree state are unchanged.
	template<typename ElementT>
	auto FindElementByPartID( std::string_view part_id ) -> ElementT*
	{
		return dynamic_cast<ElementT*>( FindLogicalElement( root, part_id, true ) );
	}

	/// @brief Root logical control whose DOM representation exists in the template.
	ui::LogicalElement root;

private:
	/// @brief Detaches and retires one active owned subtree.
	/// @param element Logical subtree root to remove.
	/// @return Whether removal occurred; roots and unowned/removed controls return false.
	/// @pre Called on the owning event loop.
	/// @post Lookup and handles cannot reach the subtree; rendered root removal is queued.
	auto RemoveSubtree( ui::LogicalElement& element ) -> bool;

	/// @brief Removes direct children in one or every semantic slot.
	/// @param element Active logical parent whose children are selected.
	/// @param slot Optional slot restriction; no value selects all direct children.
	/// @return Number of direct child subtrees detached.
	/// @pre Called on the owning event loop.
	/// @post Selected subtrees are retired and one browser removal operation is queued if needed.
	auto ClearChildren(
		ui::LogicalElement& element,
		std::optional<ui::ChildSlot> slot
	) -> size_t;

	/// @brief Detaches a prepared forest after allocating its browser removal payload.
	/// @param elements Nonoverlapping owned subtree roots.
	/// @return Number of detached subtree roots.
	/// @pre Called within a protected dispatch on the owning event loop.
	/// @post Subtrees are inactive and retained only until the outermost dispatch finishes.
	auto RetireSubtrees( std::span<ui::LogicalElement* const> elements ) -> size_t;

	/// @brief Identifies every rendered root in a logical subtree forest.
	/// @param elements Logical roots selected for removal.
	/// @return Root identities and parent/tag metadata; attributes and text are omitted to avoid copying removed content.
	/// @pre Selected objects and their DOM parts remain alive throughout this call.
	/// @post No logical or browser state is changed; IDs are never parsed.
	static auto FindRenderedRoots( std::span<ui::LogicalElement* const> elements ) -> std::vector<dom::ElementPart>;

	/// @brief Collects DOM-part records throughout one logical subtree.
	/// @param element Logical subtree to visit.
	/// @param parts Borrowed part-pointer output list.
	/// @pre The subtree remains alive and unchanged during collection.
	/// @post The output contains every stored part in logical traversal order.
	static auto CollectParts(
		const ui::LogicalElement& element,
		std::vector<const dom::ElementPart*>& parts
	) -> void;

	/// @brief Releases retired controls after the outermost dispatch has ended.
	/// @pre Called on the owning event loop; control destruction does not throw.
	/// @post The outermost drain empties the queue; reentrant requests are completed by that drain.
	auto DrainRetired() noexcept -> void;

	/// @brief Searches a logical subtree for either kind of identity.
	/// @param element Current logical subtree root.
	/// @param id Identity to match.
	/// @param match_part Whether to compare DOM part IDs instead of logical IDs.
	/// @return Logical owner of the identity, or nullptr.
	/// @pre The subtree is not being mutated concurrently.
	/// @post The subtree is unchanged.
	static auto FindLogicalElement(
		ui::LogicalElement& element,
		std::string_view id,
		bool match_part
	) -> ui::LogicalElement*
	{
		if( element.IsRemoved() ) return nullptr;
		if( !match_part && element.GetID() == id ) return &element;
		if( match_part && std::ranges::any_of( element.GetParts(),
			[ id ]( const auto& part ) { return part.GetPartID() == id; } ) ) return &element;

		for( const auto& child : element.GetChildren() )
		{
			if( auto* found = FindLogicalElement( *child, id, match_part ) ) return found;
		}
		return nullptr;
	}

	std::vector<std::unique_ptr<ui::LogicalElement>> retired_elements;
	std::function<void( std::span<const dom::ElementPart> )> removal_handler;
	bool draining = false;
};

}
