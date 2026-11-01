#include <algorithm>
#include <cassert>
#include <concepts>
#include <functional>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

import UI.DOM.ElementPart;
import UI.ClientElementTree;
import UI.Element.Text;
import UI.Element.Button;
import UI.Element.Card;
import UI.Element.TextInput;
import UI.Element.Checkbox;
import UI.Element.Select;
import UI.Element.Modal;
import UI.Element.Form;

/// @brief Text control recording exact destruction timing and optional resource cleanup.
class TrackedText : public tatzel::ui::Text
{
public:
	/// @brief Creates a tracked text control.
	/// @param id Unique logical identity.
	/// @param parent Owning logical parent.
	/// @param destroyed Counter incremented when the object is destroyed.
	/// @param on_destroy Optional nonthrowing captured-resource cleanup probe.
	/// @pre Counter and callback captures outlive this control.
	/// @post The control owns its cleanup callback and has empty text.
	TrackedText(
		std::string_view id,
		tatzel::ui::LogicalElement* parent,
		int& destroyed,
		std::function<void()> on_destroy = {}
	) :
		Text{ id, parent, "" },
		destroyed{ destroyed },
		on_destroy{ std::move( on_destroy ) }
	{}

	/// @brief Records destruction after execution protection has ended.
	/// @pre Captured cleanup does not throw.
	/// @post The counter and optional cleanup probe have run once.
	~TrackedText() override
	{
		++destroyed;
		if( on_destroy ) on_destroy();
	}

private:
	int& destroyed;
	std::function<void()> on_destroy;
};

/// @brief Button recording whether removal destroys a currently executing receiver.
class TrackedButton : public tatzel::ui::Button
{
public:
	/// @brief Creates a tracked callback receiver.
	/// @param id Unique logical identity.
	/// @param parent Owning logical parent.
	/// @param destroyed Counter incremented by destruction.
	/// @param on_click Application callback under test.
	/// @pre Counter and captures outlive their use.
	/// @post The button owns its callback.
	TrackedButton(
		std::string_view id,
		tatzel::ui::LogicalElement* parent,
		int& destroyed,
		std::function<void()> on_click
	) :
		Button{ id, parent, "Remove", std::move( on_click ) },
		destroyed{ destroyed }
	{}

	/// @brief Records receiver destruction.
	/// @pre No application callback is still executing on this object.
	/// @post The destruction counter is incremented once.
	~TrackedButton() override { ++destroyed; }

private:
	int& destroyed;
};

/// @brief Checks deterministic removal, weak handles, rendered roots, callbacks, and teardown.
/// @return Zero when every lifecycle assertion holds.
/// @pre Logical modules are built with assertions enabled; optional sanitizer instrumentation is active when configured.
/// @post Every page and retired control has been destroyed without retained object ownership.
auto main() -> int
{
	{
		auto tree = tatzel::ClientElementTree{};
		auto destroyed = 0;
		auto children_destroyed = 0;
		auto control = std::make_unique<TrackedText>( "logical|arbitrary", &tree.root, destroyed );
		control->SetParts( {
			{ "body-node", "root|main", "div" }, { "caption-node", "body-node", "span" },
			{ "portal-node", "root|main", "aside" }
		}, 0, 0 );
		auto* pointer = tree.root.InsertChild( std::move( control ) );
		pointer->GetBasePart().SetTextContent( std::string( 128000, 'x' ) );
		auto* child = pointer->InsertChild( std::make_unique<TrackedText>( "child", pointer, children_destroyed ) );
		child->SetParts( { { "child-node", "portal-node", "span" } }, 0, 0 );
		auto handle = pointer->GetHandle();
		auto child_handle = child->GetHandle();
		static_assert( std::same_as<decltype( handle ), tatzel::ui::ElementHandle<TrackedText>> );
		const auto& immutable = *pointer;
		static_assert( std::same_as<decltype( immutable.GetHandle() ), tatzel::ui::ElementHandle<const TrackedText>> );
		auto const_handle = immutable.GetHandle();
		assert( const_handle.Get() == pointer );

		auto operations = 0;
		tree.SetRemovalHandler( [ & ]( std::span<const tatzel::dom::ElementPart> roots ) {
			assert( !handle && !child_handle && destroyed == 0 && children_destroyed == 0 );
			assert( tree.FindElementByPartID<tatzel::ui::Text>( "body-node" ) == nullptr );
			assert( roots.size() == 2 && roots[ 0 ].GetPartID() == "body-node" && roots[ 1 ].GetPartID() == "portal-node" );
			assert( roots[ 0 ].GetTextContent().empty() && roots[ 0 ].GetAttributes().empty() );
			++operations;
		} );
		assert( pointer->Remove() );
		assert( destroyed == 1 && children_destroyed == 1 && operations == 1 );
		assert( !handle.Get() && !const_handle.Get() && !child_handle.Get() && tree.root.GetChildren().empty() );
		assert( !tree.root.Remove() );
	}

	{
		auto tree = tatzel::ClientElementTree{};
		auto destroyed = 0;
		auto updates = 0;
		auto* control = tree.root.InsertChild( std::make_unique<TrackedText>( "nested", &tree.root, destroyed ) );
		control->SetChangeHandler( [ &updates ] { ++updates; } );
		auto handle = control->GetHandle();
		{
			auto outer = tree.root.BeginDispatch();
			{
				auto inner = control->BeginDispatch();
				assert( control->Remove() && !handle );
				assert( !control->Remove() && control->IsRemoved() && !control->GetParent() );
				control->SetText( "No browser update" );
				assert( destroyed == 0 && updates == 0 );
			}
			assert( destroyed == 0 );
		}
		assert( destroyed == 1 );
	}

	{
		auto tree = tatzel::ClientElementTree{};
		auto destroyed = 0;
		auto clicks = 0;
		TrackedButton* button = nullptr;
		button = tree.root.InsertChild( std::make_unique<TrackedButton>( "self", &tree.root, destroyed, [ & ] {
			++clicks;
			assert( button->Remove() );
			assert( destroyed == 0 && button->GetText() == "Remove" );
			button->InvokeOnClick();
			assert( clicks == 1 );
		} ) );
		auto handle = button->GetHandle();
		button->InvokeOnClick();
		assert( destroyed == 1 && !handle );
	}

	{
		auto tree = tatzel::ClientElementTree{};
		auto parent_destroyed = 0;
		auto buttons_destroyed = 0;
		auto* parent = tree.root.InsertChild( std::make_unique<TrackedText>( "parent", &tree.root, parent_destroyed ) );
		TrackedButton* first = nullptr;
		auto* second = parent->InsertChild( std::make_unique<TrackedButton>( "second", parent, buttons_destroyed, [] { assert( false ); } ) );
		first = parent->InsertChild( std::make_unique<TrackedButton>( "first", parent, buttons_destroyed, [ & ] {
			assert( parent->Remove() );
			assert( buttons_destroyed == 0 && parent_destroyed == 0 && first->IsRemoved() );
			second->InvokeOnClick();
		} ) );
		first->InvokeOnClick();
		assert( parent_destroyed == 1 && buttons_destroyed == 2 );
	}

	{
		auto tree = tatzel::ClientElementTree{};
		auto destroyed = 0;
		TrackedButton* button = nullptr;
		button = tree.root.InsertChild( std::make_unique<TrackedButton>( "throwing", &tree.root, destroyed, [ & ] {
			button->Remove();
			throw std::runtime_error{ "Application callback failed" };
		} ) );
		auto handle = button->GetHandle();
		try { button->InvokeOnClick(); assert( false ); }
		catch( const std::runtime_error& ) {}
		assert( destroyed == 1 && !handle );
	}

	{
		auto tree = tatzel::ClientElementTree{};
		auto body_destroyed = 0;
		auto actions_destroyed = 0;
		auto* card = tree.root.InsertChild( std::make_unique<tatzel::ui::Card>( "card", &tree.root, "Preserved", true ) );
		card->SetParts( {
			{ "card-root", "root|main", "article" }, { "card-content", "card-root", "div" }, { "card-actions", "card-root", "footer" }
		}, 0, 1 );
		auto* content = card->InsertChild( std::make_unique<TrackedText>( "body", card, body_destroyed ) );
		content->SetParts( { { "body-root", "card-content", "span" } }, 0, 0 );
		auto action = std::make_unique<TrackedText>( "action", card, actions_destroyed );
		action->SetParentSlot( tatzel::ui::ChildSlot::Actions );
		action->SetParts( { { "action-root", "card-actions", "span" } }, 0, 0 );
		auto* action_pointer = card->InsertChild( std::move( action ) );
		auto action_handle = action_pointer->GetHandle();
		assert( card->ClearChildren( tatzel::ui::ChildSlot::Content ) == 1 );
		assert( body_destroyed == 1 && actions_destroyed == 0 && action_handle );
		assert( card->GetTitle() == "Preserved" && card->GetParts().size() == 3 );
		assert( card->ClearChildren() == 1 && actions_destroyed == 1 );
		assert( card->ClearChildren() == 0 && !action_handle );
		assert( tree.root.ClearChildren() == 1 );
	}

	{
		auto tree = tatzel::ClientElementTree{};
		auto* form = tree.root.InsertChild( std::make_unique<tatzel::ui::Form>( "form", &tree.root, "Fields", std::function<void()>{} ) );
		auto* field = form->InsertChild( std::make_unique<tatzel::ui::TextInput>( "field", form, "Required", "", std::function<void( std::string_view )>{} ) );
		field->SetRequired( true );
		assert( !form->Validate() );
		field->Remove();
		assert( form->GetFields().empty() && form->Validate() && !form->HasErrors() );
		form->SetOnSubmit( [ form ] { assert( form->Remove() && form->IsRemoved() ); } );
		auto handle = form->GetHandle();
		assert( form->Submit() && !handle );
	}

	{
		auto tree = tatzel::ClientElementTree{};
		auto* input = tree.root.InsertChild( std::make_unique<tatzel::ui::TextInput>( "input", &tree.root, "Name", "", std::function<void( std::string_view )>{} ) );
		input->SetOnInput( [ input ]( std::string_view value ) {
			input->Remove();
			assert( input->GetValue() == value && input->IsRemoved() );
		} );
		input->InvokeOnInput( "Edited" );
		auto* checkbox = tree.root.InsertChild( std::make_unique<tatzel::ui::Checkbox>( "check", &tree.root, "Checked", false, std::function<void( bool )>{} ) );
		checkbox->SetOnChange( [ checkbox ]( bool checked ) { checkbox->Remove(); assert( checked && checkbox->IsChecked() ); } );
		checkbox->InvokeOnChange( "true" );
		auto* select = tree.root.InsertChild( std::make_unique<tatzel::ui::Select>( "select", &tree.root, "Choice", std::vector<tatzel::ui::SelectOption>{ { "id", "Choice" } }, "", std::function<void( std::string_view )>{} ) );
		select->SetOnChange( [ select ]( std::string_view value ) { select->Remove(); assert( select->GetValue() == value ); } );
		select->InvokeOnChange( "id" );
		auto* modal = tree.root.InsertChild( std::make_unique<tatzel::ui::Modal>( "modal", &tree.root, "Dialog" ) );
		modal->Open();
		modal->SetOnClose( [ modal ] { modal->Remove(); assert( !modal->IsOpen() && modal->IsRemoved() ); } );
		modal->Close();
		assert( tree.root.GetChildren().empty() );
	}

	{
		auto tree = tatzel::ClientElementTree{};
		auto destroyed = 0;
		auto* later = tree.root.InsertChild( std::make_unique<TrackedText>( "later", &tree.root, destroyed ) );
		auto later_handle = later->GetHandle();
		auto* first = tree.root.InsertChild( std::make_unique<TrackedText>( "resource", &tree.root, destroyed, [ later_handle ] {
			if( auto* target = later_handle.Get() ) target->Remove();
		} ) );
		first->Remove();
		assert( destroyed == 2 && !later_handle && tree.root.GetChildren().empty() );
	}

	{
		auto tree = tatzel::ClientElementTree{};
		auto destroyed = 0;
		auto* control = tree.root.InsertChild( std::make_unique<TrackedText>( "transport-failure", &tree.root, destroyed ) );
		control->SetParts( { { "transport-root", "root|main", "span" } }, 0, 0 );
		auto handle = control->GetHandle();
		tree.SetRemovalHandler( []( std::span<const tatzel::dom::ElementPart> parts ) { throw std::runtime_error{ "Transport failed" }; } );
		try { control->Remove(); assert( false ); }
		catch( const std::runtime_error& ) {}
		assert( destroyed == 1 && !handle && tree.root.GetChildren().empty() );
	}

	{
		auto tree = tatzel::ClientElementTree{};
		auto destroyed = 0;
		auto* text = tree.root.InsertChild( std::make_unique<TrackedText>( "adapter-removal", &tree.root, destroyed ) );
		text->SetChangeHandler( [ text, &destroyed ] {
			text->Remove();
			assert( destroyed == 0 && text->IsRemoved() );
		} );
		text->SetText( "Remove through adapter synchronization" );
		assert( destroyed == 1 );
	}

	{
		auto destroyed = 0;
		auto observed = 0;
		auto tree = tatzel::ClientElementTree{};
		auto* text = tree.root.InsertChild( std::make_unique<TrackedText>( "adapter-state", &tree.root, destroyed ) );
		text->SetChangeHandler( [ text, &observed, count = 0 ]() mutable {
			observed = ++count;
			if( count == 2 )
			{
				text->SetChangeHandler( [ &observed ] { observed = 10; } );
				assert( count == 2 );
			}
		} );
		text->SetText( "First update" );
		assert( observed == 1 );
		text->SetText( "Second update replaces its own callback" );
		assert( observed == 2 );
		text->SetText( "Replacement update" );
		assert( observed == 10 );
	}

	{
		auto tree = tatzel::ClientElementTree{};
		auto destroyed = 0;
		for( int index = 0; index < 1000; ++index )
		{
			auto* text = tree.root.InsertChild( std::make_unique<TrackedText>( std::to_string( index ), &tree.root, destroyed ) );
			auto handle = text->GetHandle();
			text->Remove();
			assert( destroyed == index + 1 && !handle && tree.root.GetChildren().empty() );
		}
	}

	tatzel::ui::ElementHandle<tatzel::ui::LogicalElement> root_handle;
	tatzel::ui::ElementHandle<TrackedText> retained;
	auto destroyed = 0;
	{
		auto tree = tatzel::ClientElementTree{};
		root_handle = tree.root.GetHandle();
		auto* control = tree.root.InsertChild( std::make_unique<TrackedText>( "teardown", &tree.root, destroyed, [ &retained, &root_handle ] {
			assert( !retained && !root_handle );
		} ) );
		retained = control->GetHandle();
		assert( retained && root_handle );
	}
	assert( destroyed == 1 && !retained && !root_handle );
	return 0;
}
