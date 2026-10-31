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
export import UI.Element.Text;
export import UI.Element.Paragraph;
export import UI.Element.Card;
export import UI.Element.TextInput;
export import UI.Element.Checkbox;
export import UI.Element.Modal;

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
	/// @param child_slot Semantic insertion area supported by parent.
	/// @pre core and parent belong to the same live client page; parent supports child_slot.
	/// @post New controls are inserted under parent.
	PageBuilder(
		PageBuilderCore& core,
		ElementT& parent,
		ui::ChildSlot child_slot = ui::ChildSlot::Content
	) :
		core{ &core },
		parent{ &parent },
		child_slot{ child_slot }
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

	/// @brief Creates inline plain text, preserving line breaks and escaping markup.
	/// @param text Initial text, copied before returning.
	/// @return Borrowed page-owned text control.
	/// @pre Called on the live page's owning event loop.
	/// @post The control is owned and its DOM creation is queued.
	auto Text( std::string_view text ) -> ui::Text* { return AddChild<ui::Text>( text ); }

	/// @brief Creates a semantic paragraph containing plain text.
	/// @param text Initial paragraph text, copied before returning.
	/// @return Borrowed page-owned paragraph.
	/// @pre Called on the live page's owning event loop.
	/// @post The paragraph is owned and its DOM creation is queued.
	auto Paragraph( std::string_view text ) -> ui::Paragraph* { return AddChild<ui::Paragraph>( text ); }

	/// @brief Creates a card with content and no actions area.
	/// @tparam ContentFn Callable accepting a scoped builder and optionally the card pointer.
	/// @param title Optional card heading, copied before returning.
	/// @param content_fn Builder for the card's logical content.
	/// @return Borrowed page-owned card.
	/// @pre Called on the owning event loop; content_fn has a supported builder signature.
	/// @post The card owns the content created by content_fn.
	template<typename ContentFn>
	auto Card(
		std::string_view title,
		ContentFn&& content_fn
	) -> ui::Card*
	{
		auto* card = AddChild<ui::Card>( title, false );
		BuildChildren( *card, std::forward<ContentFn>( content_fn ) );
		return card;
	}

	/// @brief Creates a card with separate content and action builders.
	/// @tparam ContentFn Content-builder callable type.
	/// @tparam ActionsFn Action-builder callable type.
	/// @param title Optional card heading, copied before returning.
	/// @param content_fn Builder for main logical content.
	/// @param actions_fn Builder for controls in the card's actions area.
	/// @return Borrowed page-owned card.
	/// @pre Called on the owning event loop; both callbacks accept scoped builders.
	/// @post Both sets of children have the same logical card parent and distinct insertion slots.
	template<
		typename ContentFn,
		typename ActionsFn
	>
	auto Card(
		std::string_view title,
		ContentFn&& content_fn,
		ActionsFn&& actions_fn
	) -> ui::Card*
	{
		auto* card = AddChild<ui::Card>( title, true );
		BuildChildren( *card, std::forward<ContentFn>( content_fn ) );
		BuildChildren( *card, std::forward<ActionsFn>( actions_fn ), ui::ChildSlot::Actions );
		return card;
	}

	/// @brief Creates a labeled single-line editor.
	/// @param label Accessible label, copied before returning.
	/// @param value Initial text value, copied before returning.
	/// @param on_change Optional callback for committed changes.
	/// @return Borrowed page-owned editor; input and submit callbacks may be set separately.
	/// @pre Called on the owning event loop; callback captures outlive their invocation.
	/// @post Label and editor parts are created and browser editing events can reach the control.
	auto TextInput(
		std::string_view label,
		std::string_view value = {},
		std::function<void( std::string_view )> on_change = {}
	) -> ui::TextInput*
	{
		return AddChild<ui::TextInput>( label, value, std::move( on_change ) );
	}

	/// @brief Creates an accessible labeled checkbox.
	/// @param label Label text, copied before returning.
	/// @param checked Initial boolean value.
	/// @param on_change Optional callback receiving accepted changes.
	/// @return Borrowed page-owned checkbox.
	/// @pre Called on the owning event loop; callback captures outlive their invocation.
	/// @post The checkbox is created and browser changes can reach its logical state.
	auto Checkbox(
		std::string_view label,
		bool checked = false,
		std::function<void( bool )> on_change = {}
	) -> ui::Checkbox*
	{
		return AddChild<ui::Checkbox>( label, checked, std::move( on_change ) );
	}

	/// @brief Creates a closed modal and builds its content before it can open.
	/// @tparam ContentFn Callable accepting a scoped builder and optionally the modal pointer.
	/// @param title Accessible dialog title, copied before returning.
	/// @param content_fn Builder for the modal's logical content.
	/// @return Borrowed page-owned modal; call Open to present it.
	/// @pre Called on the owning event loop; content_fn has a supported builder signature.
	/// @post The modal owns its content and is initially closed unless content_fn opens it.
	template<typename ContentFn>
	auto Modal(
		std::string_view title,
		ContentFn&& content_fn
	) -> ui::Modal*
	{
		auto* modal = AddChild<ui::Modal>( title );
		BuildChildren( *modal, std::forward<ContentFn>( content_fn ) );
		return modal;
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
		element->SetParentSlot( child_slot );
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
	/// @param slot Semantic insertion slot within child_parent.
	/// @pre The callable accepts one of the documented builder signatures.
	/// @post The callable has run once in the child scope.
	template<
		ui::LogicalElementDerived ParentT,
		typename ChildBuilderFn
	>
	auto BuildChildren(
		ParentT& child_parent,
		ChildBuilderFn&& child_builder_fn,
		ui::ChildSlot slot = ui::ChildSlot::Content
	) -> void
	{
		auto scoped_builder = PageBuilder<ParentT>{ *core, child_parent, slot };
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
	ui::ChildSlot child_slot;
};

}
