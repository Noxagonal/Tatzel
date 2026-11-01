module;

#include <array>
#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <span>
#include <unordered_set>
#include <utility>
#include <string_view>
#include <vector>

module UI.ClientElementTree;

import UI.DOM.ElementPart;

tatzel::ClientElementTree::ClientElementTree() :
	root{
		"root", nullptr,
		std::vector<dom::ElementPart>{
			dom::ElementPart{ "root|main", "", "main", std::array<std::string_view, 1>{ "container" } }
		},
		0, 0
	}
{
	root.SetLifecycleHandlers( [ this ]( ui::LogicalElement& element ) { return RemoveSubtree( element ); },
		[ this ](
			ui::LogicalElement& element,
			std::optional<ui::ChildSlot> slot
		) { return ClearChildren( element, slot ); },
		[ this ] { DrainRetired(); } );
}


tatzel::ClientElementTree::~ClientElementTree()
{
	root.InvalidateSubtree();
	root.SetLifecycleHandlers( {}, {}, {} );
}

auto tatzel::ClientElementTree::SetRemovalHandler( std::function<void( std::span<const dom::ElementPart> )> handler ) -> void
{
	removal_handler = std::move( handler );
}

auto tatzel::ClientElementTree::RemoveSubtree( ui::LogicalElement& element ) -> bool
{
	auto dispatch = root.BeginDispatch();
	auto* parent = element.GetParent();
	if( element.IsRemoved() || !parent || parent->IsRemoved()
		|| FindElementByID<ui::LogicalElement>( parent->GetID() ) != parent ) return false;

	const auto owned = std::ranges::any_of( parent->GetChildren(), [ &element ]( const auto& child ) { return child.get() == &element; } );
	if( !owned ) return false;

	auto selected = std::array{ &element };
	return RetireSubtrees( selected ) == 1;
}

auto tatzel::ClientElementTree::ClearChildren(
	ui::LogicalElement& element,
	std::optional<ui::ChildSlot> slot
) -> size_t
{
	auto dispatch = root.BeginDispatch();
	if( element.IsRemoved() || FindElementByID<ui::LogicalElement>( element.GetID() ) != &element ) return 0;

	auto selected = std::vector<ui::LogicalElement*>{};
	for( const auto& child : element.GetChildren() )
	{
		if( !slot || child->GetParentSlot() == *slot ) selected.push_back( child.get() );
	}
	return RetireSubtrees( selected );
}

auto tatzel::ClientElementTree::RetireSubtrees( std::span<ui::LogicalElement* const> elements ) -> size_t
{
	if( elements.empty() ) return 0;

	auto roots = FindRenderedRoots( elements );
	retired_elements.reserve( retired_elements.size() + elements.size() );
	for( auto* element : elements )
	{
		auto detached = element->GetParent()->DetachChild( *element );
		detached->InvalidateSubtree();
		retired_elements.push_back( std::move( detached ) );
	}

	if( !roots.empty() && removal_handler )
	{
		auto handler = removal_handler;
		handler( roots );
	}
	return elements.size();
}

auto tatzel::ClientElementTree::CollectParts(
	const ui::LogicalElement& element,
	std::vector<const dom::ElementPart*>& parts
) -> void
{
	for( const auto& part : element.GetParts() ) parts.push_back( &part );
	for( const auto& child : element.GetChildren() ) CollectParts( *child, parts );
}

auto tatzel::ClientElementTree::FindRenderedRoots( std::span<ui::LogicalElement* const> elements ) -> std::vector<dom::ElementPart>
{
	auto parts = std::vector<const dom::ElementPart*>{};
	for( const auto* element : elements ) CollectParts( *element, parts );

	auto ids = std::unordered_set<std::string_view>{};
	for( const auto* part : parts ) ids.insert( part->GetPartID() );

	auto roots = std::vector<dom::ElementPart>{};
	for( const auto* part : parts )
	{
		if( !ids.contains( part->GetParentID() ) )
		{
			roots.emplace_back( part->GetPartID(), part->GetParentID(), part->GetTag() );
		}
	}
	return roots;
}

auto tatzel::ClientElementTree::DrainRetired() noexcept -> void
{
	if( draining || retired_elements.empty() ) return;

	draining = true;
	{
		auto cleanup_scope = root.BeginDispatch();
		while( !retired_elements.empty() )
		{
			auto batch = std::vector<std::unique_ptr<ui::LogicalElement>>{};
			batch.swap( retired_elements );
		}
	}
	draining = false;
}
