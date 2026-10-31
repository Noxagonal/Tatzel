/// @file
/// @brief Scoped application API for creating logical controls.
module;

#include <cassert>
#include <concepts>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

export module UI.PageBuilder;

import UI.PageBuilderCore;
import UI.ClientUpdater;
import UI.Utility;

export import UI.Element.LogicalElement;
export import UI.Element.Heading;
export import UI.Element.Button;
export import UI.Element.Container;

namespace tatzel {

/// @brief Lightweight borrowed handle for building a logical subtree.
/// @tparam ElementT Type of the logical parent represented by this scope.
/// @details Copying the handle preserves its scope. It may be captured by a page
/// callback, provided the owning client page still exists when it is used.
export template<ui::LogicalElementDerived ElementT = ui::LogicalElement>
class PageBuilder
{
public:
	/// @brief Creates a scoped logical builder.
	/// @param core Client-owned framework services.
	/// @param parent Logical parent for new controls.
	/// @pre core and parent belong to the same live client page.
	/// @post New controls are inserted under parent.
	PageBuilder(
		PageBuilderCore& core,
		ElementT& parent
	) :
		core{ &core },
		parent{ &parent }
	{}

	/// @brief Creates a builder for another logical parent on the same page.
	/// @tparam NewParentT New logical parent type.
	/// @param new_parent Parent for the new scope.
	/// @return Borrowed builder with the new logical parent.
	/// @pre new_parent belongs to this builder's live client page.
	/// @post This builder's scope is unchanged.
	template<ui::LogicalElementDerived NewParentT>
	auto Scope( NewParentT& new_parent ) const -> PageBuilder<NewParentT> { return { *core, new_parent }; }

	/// @brief Creates a logical heading and its adapter-selected DOM representation.
	/// @param text Initial logical text.
	/// @param style Semantic heading level.
	/// @return Borrowed heading pointer, valid for the client page lifetime.
	/// @pre The builder's page is alive and is accessed on its owning event loop.
	/// @post The heading is owned by the logical parent and DOM creation is queued.
	auto Heading(
		std::string_view text,
		ui::HeadingStyle style = ui::HeadingStyle::H1
	) -> ui::Heading*
	{
		return AddChild<ui::Heading>( text, style );
	}

	/// @brief Creates a logical button with a server-side click callback.
	/// @param text Initial logical label.
	/// @param on_click Callback invoked on the owning event loop.
	/// @return Borrowed button pointer, valid for the client page lifetime.
	/// @pre The page is alive; captured objects outlive callback invocations.
	/// @post The button is owned by the parent and browser clicks can reach its callback.
	auto Button(
		std::string_view text,
		std::function<void()> on_click
	) -> ui::Button*
	{
		auto* button = AddChild<ui::Button>( text, std::move( on_click ) );
		core->GetClientUpdater()->SetOnClick( button->GetBasePart().GetPartID() );
		return button;
	}

	/// @brief Creates a logical container and builds its logical children.
	/// @tparam ChildBuilderFn Callable accepting a scoped builder and optionally its parent pointer.
	/// @param child_builder_fn Function that builds the container's logical children.
	/// @return Borrowed container pointer, valid for the client page lifetime.
	/// @pre The page is alive and the callback is invocable with a scoped builder.
	/// @post The container and callback-created children belong to this page.
	template<typename ChildBuilderFn>
	auto Container( ChildBuilderFn&& child_builder_fn ) -> ui::Container*
	{
		auto* container = AddChild<ui::Container>();
		BuildChildren( *container, std::forward<ChildBuilderFn>( child_builder_fn ) );
		return container;
	}

private:
	/// @brief Allocates a logical control, maps its parts, and transfers ownership.
	/// @tparam NewElementT Logical control type.
	/// @tparam ArgsT Constructor argument types after identity and logical parent.
	/// @param args Constructor arguments forwarded to the logical control.
	/// @return Borrowed pointer to the owned logical control.
	/// @pre The builder's page is alive and the adapter supports NewElementT.
	/// @post The parent owns the control at a stable address and DOM creation is queued.
	template<
		ui::LogicalElementDerived NewElementT,
		typename ...ArgsT
	>
	auto AddChild( ArgsT&& ...args ) -> NewElementT*
	{
		assert( parent && core );
		auto element = std::make_unique<NewElementT>(
			std::string{ "ui-" } + GenerateUUID(), parent, std::forward<ArgsT>( args )... );
		core->GetElementAdapter()->CreatePartsFor( *element );
		auto* result = parent->InsertChild( std::move( element ) );
		core->GetClientUpdater()->CreateElementParts( result->GetParts() );
		return result;
	}

	/// @brief Invokes a child builder within a newly created logical scope.
	/// @tparam ParentT Logical parent type.
	/// @tparam ChildBuilderFn Child-builder callable type.
	/// @param child_parent Logical parent for the callback's scope.
	/// @param child_builder_fn Callable taking the scoped builder and optionally the parent pointer.
	/// @pre The callable accepts one of the documented builder signatures.
	/// @post The callable has run once in the child scope.
	template<
		ui::LogicalElementDerived ParentT,
		typename ChildBuilderFn
	>
	auto BuildChildren(
		ParentT& child_parent,
		ChildBuilderFn&& child_builder_fn
	) -> void
	{
		auto scoped_builder = Scope( child_parent );
		using ScopedBuilderT = decltype( scoped_builder );
		if constexpr( std::is_invocable_v<ChildBuilderFn&&, ScopedBuilderT&, ParentT*> )
			std::invoke( std::forward<ChildBuilderFn>( child_builder_fn ), scoped_builder, &child_parent );
		else
		{
			static_assert( std::is_invocable_v<ChildBuilderFn&&, ScopedBuilderT&>,
				"Child builder must accept a scoped builder and optionally its logical parent pointer." );
			std::invoke( std::forward<ChildBuilderFn>( child_builder_fn ), scoped_builder );
		}
	}

	PageBuilderCore* core;
	ElementT* parent;
};

}
