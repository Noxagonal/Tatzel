#include <cassert>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

import UI.ClientElementTree;
import UI.Element.Row;
import UI.Element.Column;
import UI.Element.Form;
import UI.Element.TextInput;
import UI.Element.TextArea;
import UI.Element.Select;
import UI.Element.Checkbox;
import UI.Element.Button;

/// @brief Checks layout state, stable choices, and coordinated typed form submissions.
/// @return Zero if all logical behavior contracts hold.
/// @pre Tatzel modules are built with assertions enabled.
/// @post Temporary controls, snapshots, validators, and callbacks are destroyed.
auto main() -> int
{
	auto tree = tatzel::ClientElementTree{};
	auto row = tatzel::ui::Row{ "row", &tree.root };
	auto layout_updates = 0;
	row.SetChangeHandler( [ &layout_updates ] { ++layout_updates; } );
	row.SetGap( tatzel::ui::Spacing::Small );
	row.SetGap( tatzel::ui::Spacing::Small );
	row.SetAlignment( tatzel::ui::Alignment::Center );
	row.SetJustification( tatzel::ui::Justification::SpaceBetween );
	row.SetWrap( true );
	assert( layout_updates == 4 && row.GetDirection() == tatzel::ui::LayoutDirection::Row );

	auto select = tatzel::ui::Select{ "select", &tree.root, "Choice",
		{ { "a", "Same label" }, { "b", "Same label" }, { "off", "Unavailable", true } }, "b", {} };
	assert( select.GetValue() == "b" && !select.AcceptsChangeValue( "off" ) );
	select.SetOptions( { { "b", "Renamed" }, { "c", "Third" } } );
	assert( select.GetValue() == "b" && select.GetOptions().front().label == "Renamed" );
	try
	{
		select.SetOptions( { { "duplicate", "First" }, { "duplicate", "Second" } } );
		assert( false );
	}
	catch( const std::invalid_argument& ) {}
	assert( select.GetValue() == "b" && select.GetOptions().size() == 2 );
	select.SetOptions( { { "b", "Now unavailable", true } } );
	assert( select.GetValue().empty() );
	try
	{
		select.SetValue( "b" );
		assert( false );
	}
	catch( const std::invalid_argument& ) {}
	select.SetRequired( true );
	assert( !select.Validate() && !select.GetError().empty() );
	select.SetDisabled( true );
	assert( select.Validate() && select.GetError().empty() );

	auto* form = tree.root.InsertChild( std::make_unique<tatzel::ui::Form>( "form", &tree.root, "Contact", std::function<void()>{} ) );
	auto* column = form->InsertChild( std::make_unique<tatzel::ui::Column>( "column", form ) );
	auto* name = column->InsertChild( std::make_unique<tatzel::ui::TextInput>( "name", column, "Name", "before", std::function<void( std::string_view )>{} ) );
	auto* notes = column->InsertChild( std::make_unique<tatzel::ui::TextArea>( "notes", column, "Notes", "", std::function<void( std::string_view )>{} ) );
	auto* choice = column->InsertChild( std::make_unique<tatzel::ui::Select>( "choice", column, "Choice",
		std::vector<tatzel::ui::SelectOption>{ { "a", "First" }, { "b", "Second" } }, "", std::function<void( std::string_view )>{} ) );
	auto* consent = column->InsertChild( std::make_unique<tatzel::ui::Checkbox>( "consent", column, "Consent", false, std::function<void( bool )>{} ) );
	auto* reference = column->InsertChild( std::make_unique<tatzel::ui::TextInput>( "reference", column, "Reference", "server", std::function<void( std::string_view )>{} ) );
	auto* ignored = column->InsertChild( std::make_unique<tatzel::ui::TextInput>( "ignored", column, "Ignored", "keep", std::function<void( std::string_view )>{} ) );
	reference->SetReadOnly( true );
	ignored->SetDisabled( true );
	ignored->SetRequired( true );
	name->SetRequired( true );
	choice->SetRequired( true );
	consent->SetRequired( true );
	notes->SetRows( 7 );
	try
	{
		notes->SetRows( 0 );
		assert( false );
	}
	catch( const std::invalid_argument& ) {}
	assert( notes->GetRows() == 7 );

	auto edit_callbacks = 0;
	name->SetOnInput( [ &edit_callbacks ]( std::string_view value ) { ++edit_callbacks; } );
	name->SetOnChange( [ &edit_callbacks ]( std::string_view value ) { ++edit_callbacks; } );
	auto validators = 0;
	notes->SetValidator( [ &validators ]( std::string_view value ) -> std::string {
		++validators;
		return value == "bad" ? "Invalid notes" : "";
	} );
	auto submits = 0;
	auto invalids = 0;
	form->SetOnSubmit( [ &submits, name, notes, choice, consent ] {
		assert( name->GetValue() == "Ada" && notes->GetValue() == "first\nsecond" );
		assert( choice->GetValue() == "b" && consent->IsChecked() );
		++submits;
	} );
	form->SetOnInvalid( [ &invalids ] { ++invalids; } );

	auto submission = std::vector<tatzel::ui::FieldSubmission>{
		{ name, std::string{ "Ada" } }, { notes, std::string{ "first\nsecond" } },
		{ choice, std::string{ "b" } }, { consent, true }, { reference, std::string{ "forged" } }
	};
	auto malformed = submission;
	malformed.back().value = false;
	assert( !form->ApplySubmission( malformed ) && name->GetValue() == "before" );
	malformed = submission;
	malformed.back() = malformed.front();
	assert( !form->ApplySubmission( malformed ) && name->GetValue() == "before" );
	malformed = submission;
	malformed[ 2 ].value = std::string{ "not-an-option" };
	assert( !form->ApplySubmission( malformed ) && name->GetValue() == "before" );
	malformed.pop_back();
	assert( !form->ApplySubmission( malformed ) );
	auto outside = tatzel::ui::TextInput{ "outside", &tree.root, "Outside", "", {} };
	malformed = submission;
	malformed.back().field = &outside;
	assert( !form->ApplySubmission( malformed ) && outside.GetValue().empty() );

	assert( form->ApplySubmission( submission ) );
	assert( name->GetValueRevision() == 0 && edit_callbacks == 0 );
	assert( reference->GetValue() == "server" && ignored->GetValue() == "keep" );
	assert( form->Submit() && submits == 1 && invalids == 0 && validators == 1 );

	name->SetValue( "" );
	choice->SetValue( "" );
	consent->SetChecked( false );
	notes->SetValue( "bad" );
	assert( !form->Submit() && invalids == 1 && submits == 1 && validators == 2 );
	assert( !name->GetError().empty() && !choice->GetError().empty() && !consent->GetError().empty() );
	assert( notes->GetError() == "Invalid notes" && form->HasErrors() );
	notes->SetDisabled( true );
	assert( notes->GetError().empty() );
	form->SetDisabled( true );
	assert( !form->Submit() && !form->HasErrors() && !name->IsDisabled() );
	assert( !form->ApplySubmission( submission ) );
	form->SetDisabled( false );
	assert( form->HasErrors() );

	try
	{
		auto nested = tatzel::ui::Form{ "nested", column, "Invalid", {} };
		assert( false );
	}
	catch( const std::invalid_argument& ) {}
	return 0;
}
