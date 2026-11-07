module;

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module UI.Element.LogicalElement;

import UI.DOM.ElementPart;

namespace tatzel::ui {

LogicalElement::LogicalElement(
	std::string_view id,
	LogicalElement* parent
) :
	id{ id },
	parent{ parent },
	dispatch_context{ parent ? parent->dispatch_context : std::make_shared<ElementDispatchContext>() },
	removal_services{ parent ? parent->removal_services : std::make_shared<ElementRemovalServices>() }
{}

LogicalElement::LogicalElement(
	std::string_view id,
	LogicalElement* parent,
	std::vector<dom::ElementPart>&& parts,
	size_t base_part_index,
	size_t child_root_part_index
) :
	LogicalElement{ id, parent }
{
	SetParts( std::move( parts ), base_part_index, child_root_part_index );
}

auto LogicalElement::Remove() -> bool
{
	auto dispatch = BeginDispatch();
	if( removed || !parent || !removal_services->remove ) return false;
	return removal_services->remove( *this );
}

auto LogicalElement::SetLifecycleHandlers(
	std::function<bool( LogicalElement& )> remove_handler,
	std::function<size_t( LogicalElement&, std::optional<ChildSlot> )> clear_handler,
	std::function<void()> on_idle
) -> void
{
	assert( !parent );
	removal_services->remove = std::move( remove_handler );
	removal_services->clear = std::move( clear_handler );
	dispatch_context->SetOnIdle( std::move( on_idle ) );
}

auto LogicalElement::InvalidateSubtree() noexcept -> void
{
	removed = true;
	lifetime_state->Invalidate();
	for( const auto& child : child_list ) child->InvalidateSubtree();
}

auto LogicalElement::DetachChild( LogicalElement& child ) -> std::unique_ptr<LogicalElement>
{
	auto found = std::ranges::find_if( child_list, [ &child ]( const auto& owned ) { return owned.get() == &child; } );
	if( found == child_list.end() ) return nullptr;

	auto result = std::move( *found );
	child_list.erase( found );
	result->parent = nullptr;
	return result;
}

auto LogicalElement::SetParentSlot( ChildSlot slot ) -> void
{
	assert( part_list.empty() );
	parent_slot = slot;
}

auto LogicalElement::SetParts(
	std::vector<dom::ElementPart>&& parts,
	size_t base_part_index,
	size_t child_root_part_index
) -> void
{
	assert( part_list.empty() );
	assert( base_part_index < parts.size() );
	assert( child_root_part_index < parts.size() );
	part_list = std::move( parts );
	this->base_part_index = base_part_index;
	this->child_root_part_index = child_root_part_index;
}

auto LogicalElement::FindPart( std::string_view part_name ) -> dom::ElementPart*
{
	auto part_id = id + "|" + std::string{ part_name };
	auto found = std::ranges::find_if( part_list, [ &part_id ]( const auto& part ) { return part.GetPartID() == part_id; } );
	return found == part_list.end() ? nullptr : &*found;
}

auto LogicalElement::SetChangeHandler( std::function<void()> handler ) -> void
{
	change_handler = handler ? std::make_shared<std::function<void()>>( std::move( handler ) ) : nullptr;
}

auto LogicalElement::SetAppearance( std::string_view appearance ) -> void
{
	auto dispatch = BeginDispatch();
	if( removed || this->appearance == appearance ) return;
	this->appearance = appearance;
	auto handler = appearance_handler;
	if( handler ) ( *handler )( this->appearance );
}

auto LogicalElement::SetAppearanceHandler( std::function<void( std::string_view )> handler ) -> void
{
	appearance_handler = handler ? std::make_shared<std::function<void( std::string_view )>>( std::move( handler ) ) : nullptr;
}

auto LogicalElement::NotifyChanged() -> void
{
	auto dispatch = BeginDispatch();
	if( removed ) return;

	auto handler = change_handler;
	if( handler ) ( *handler )();
}

auto LogicalElement::ClearChildrenImpl( std::optional<ChildSlot> slot ) -> size_t
{
	auto dispatch = BeginDispatch();
	if( removed || !removal_services->clear ) return 0;
	return removal_services->clear( *this, slot );
}

}
