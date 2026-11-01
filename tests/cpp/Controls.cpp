#include <cassert>
#include <stdexcept>
#include <string>
#include <string_view>

import UI.Diagnostics;
import UI.ClientElementTree;
import UI.Element.Text;
import UI.Element.Paragraph;
import UI.Element.Card;
import UI.Element.TextInput;
import UI.Element.Checkbox;
import UI.Element.Modal;

/// @brief Checks logical ownership, browser edit acceptance, and callback transitions.
/// @return Zero when every control contract holds.
/// @pre Tatzel modules and the library have been built with assertions enabled.
/// @post All temporary controls and their callbacks have been destroyed.
auto main() -> int
{
	auto tree = tatzel::ClientElementTree{};
	auto paragraph = tatzel::ui::Paragraph{ "paragraph", &tree.root, std::string( 1024, 'x' ) };
	assert( paragraph.GetText() == std::string( 1024, 'x' ) );
	paragraph.SetText( "<b>plain</b>\nsecond line" );
	assert( paragraph.GetText() == "<b>plain</b>\nsecond line" );

	auto card = tatzel::ui::Card{ "card", &tree.root, "Title", true };
	assert( card.HasActions() && card.GetTitle() == "Title" );
	card.SetTitle( "" );
	assert( card.GetTitle().empty() );

	auto input = tatzel::ui::TextInput{ "input", &tree.root, "Name", "Initial", {} };
	auto inputs = 0;
	auto commits = 0;
	auto submissions = 0;
	input.SetOnInput( [ &input, &inputs ]( std::string_view value ) {
		assert( input.GetValue() == value );
		++inputs;
	} );
	input.SetOnChange( [ &input, &commits ]( std::string_view value ) {
		assert( input.GetValue() == value );
		++commits;
	} );
	input.SetOnSubmit( [ &input, &submissions ]( std::string_view value ) {
		assert( input.GetValue() == value );
		++submissions;
	} );
	input.InvokeOnInput( "Typing" );
	input.InvokeOnInput( "Typing" );
	assert( inputs == 1 && input.GetValueRevision() == 0 );
	input.InvokeOnChange( "Typing" );
	input.InvokeOnSubmit();
	assert( commits == 1 && submissions == 1 );

	input.SetReadOnly( true );
	input.InvokeOnInput( "Ignored" );
	input.InvokeOnChange( "Ignored" );
	assert( input.GetValue() == "Typing" && inputs == 1 && commits == 1 );
	input.SetDisabled( true );
	input.InvokeOnSubmit();
	assert( submissions == 1 );
	input.SetReadOnly( false );
	input.SetDisabled( false );

	input.SetOnInput( [ &input ]( std::string_view value ) {
		input.SetValue( std::string( 2048, 'y' ) );
		// Event text remains valid even if a callback reallocates the logical value.
		assert( value == "snapshot" );
		input.SetOnInput( {} );
	} );
	input.InvokeOnInput( "snapshot" );
	assert( input.GetValue() == std::string( 2048, 'y' ) && input.GetValueRevision() == 1 );

	auto checkbox = tatzel::ui::Checkbox{ "checkbox", &tree.root, "Enabled", false, {} };
	auto changes = 0;
	checkbox.SetOnChange( [ &checkbox, &changes ]( bool checked ) {
		assert( checked && checkbox.IsChecked() );
		++changes;
	} );
	checkbox.InvokeOnChange( "true" );
	assert( changes == 1 && checkbox.IsChecked() && checkbox.GetValueRevision() == 0 );
	checkbox.SetDisabled( true );
	checkbox.InvokeOnChange( "false" );
	assert( checkbox.IsChecked() && changes == 1 );
	checkbox.SetChecked( false );
	assert( !checkbox.IsChecked() && checkbox.GetValueRevision() == 1 );
	try
	{
		checkbox.InvokeOnChange( "not-a-boolean" );
		assert( false );
	}
	catch( const tatzel::Exception& ) {}

	auto modal = tatzel::ui::Modal{ "modal", &tree.root, "Preview" };
	auto closes = 0;
	auto transitions = 0;
	modal.SetChangeHandler( [ &transitions ] { ++transitions; } );
	modal.SetOnClose( [ &modal, &closes ] {
		assert( !modal.IsOpen() );
		++closes;
	} );
	modal.Open();
	modal.Open();
	modal.InvokeOnDismiss();
	modal.InvokeOnDismiss();
	assert( !modal.IsOpen() && closes == 1 && transitions == 2 );
	modal.SetOnClose( [ &modal ] { modal.Open(); } );
	modal.Open();
	modal.Close();
	assert( modal.IsOpen() );
	return 0;
}
