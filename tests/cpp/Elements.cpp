#include <cassert>
#include <cmath>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

import UI.Diagnostics;
import UI.ClientElementTree;
import UI.Element.NumberInput;
import UI.Element.RadioGroup;
import UI.Element.Link;
import UI.Element.Image;
import UI.Element.Form;

/// @brief Checks numeric drafts, constrained forms, choice identity, and resource ownership.
/// @return Zero if all boundary contracts hold.
/// @pre Modules are built with assertions enabled.
/// @post Temporary logical controls and callbacks are destroyed.
auto main() -> int
{
	auto tree = tatzel::ClientElementTree{};
	auto number = tatzel::ui::NumberInput{ "number", &tree.root, "Amount", {}, {} };
	assert( !number.GetValue() && !number.HasInvalidInput() && number.Validate() );
	number.SetRequired( true );
	assert( !number.Validate() );
	number.SetValue( 0 );
	assert( number.GetValue() == 0 && number.Validate() );
	number.SetRange( -1, 1 );
	number.SetStep( 0.1 );
	number.InvokeOnChange( "0.3" );
	assert( number.GetValue() == 0.3 && number.Validate() );
	number.InvokeOnChange( "0.35" );
	assert( !number.Validate() );
	number.InvokeOnInput( "2" );
	assert( number.GetValue() == 2 && !number.Validate() );
	for( auto text : { "NaN", "inf", "1e999", "-", "1x", "0x12", " 1", "1 " } )
	{
		number.InvokeOnInput( text );
		assert( !number.GetValue() && number.HasInvalidInput() && !number.Validate() );
	}
	number.InvokeOnInput( "1e-1" );
	assert( number.GetValue() == 0.1 && number.Validate() );
	number.SetRange( 0.05, 1 );
	number.SetValue( 0.15 );
	assert( number.Validate() );
	number.SetValue( 0.1 );
	assert( !number.Validate() );
	number.SetStep( {} );
	assert( number.Validate() );
	try { number.SetRange( 2, 1 ); assert( false ); } catch( const tatzel::Exception& ) {}
	assert( number.GetMinimum() == 0.05 && number.GetMaximum() == 1 );
	try { number.SetStep( 0 ); assert( false ); } catch( const tatzel::Exception& ) {}
	try { number.SetStep( -1 ); assert( false ); } catch( const tatzel::Exception& ) {}
	try { number.SetValue( std::numeric_limits<double>::infinity() ); assert( false ); } catch( const tatzel::Exception& ) {}
	try { number.SetRange( {}, std::numeric_limits<double>::quiet_NaN() ); assert( false ); } catch( const tatzel::Exception& ) {}
	assert( number.GetValue() == 0.1 && !number.GetStep() );

	auto revision = number.GetValueRevision();
	auto changes = 0;
	number.SetOnChange( [ &number, &changes ]( std::optional<double> value ) {
		++changes;
		number.SetValue( 0.5 );
		assert( value == 0.25 );
	} );
	number.InvokeOnChange( "0.25" );
	assert( changes == 1 && number.GetValue() == 0.5 && number.GetValueRevision() == revision + 1 );
	number.SetReadOnly( true );
	number.InvokeOnInput( "0.75" );
	number.InvokeOnChange( "0.25" );
	assert( number.GetValue() == 0.5 && changes == 1 );
	number.SetReadOnly( false );
	number.SetDisabled( true );
	number.InvokeOnChange( "0.25" );
	assert( changes == 1 && number.Validate() );
	number.SetDisabled( false );
	number.SetValidator( []( std::optional<double> value ) -> std::string { return value == 0.5 ? "Reserved amount" : ""; } );
	assert( !number.Validate() && number.GetError() == "Reserved amount" );
	number.SetValidator( {} );
	assert( number.Validate() );

	auto radios = tatzel::ui::RadioGroup{ "radio", &tree.root, "Delivery", {
		{ "a", "Same label" }, { "b", "Same label" }, { "off", "Unavailable", true }
	}, "a", {} };
	assert( !radios.AcceptsChangeValue( "off" ) && !radios.AcceptsChangeValue( "unknown" ) );
	try { radios.SetOptions( { { "a", "One" }, { "a", "Two" } } ); assert( false ); } catch( const tatzel::Exception& ) {}
	assert( radios.GetOptions().size() == 3 && radios.GetValue() == "a" );
	radios.SetOptions( { { "a", "Renamed" }, { "b", "Second" } } );
	assert( radios.GetValue() == "a" );
	radios.SetOptions( { { "a", "Disabled", true } } );
	assert( radios.GetValue().empty() );
	radios.SetRequired( true );
	assert( !radios.Validate() );

	auto* form = tree.root.InsertChild( std::make_unique<tatzel::ui::Form>( "form", &tree.root, "Order", std::function<void()>{} ) );
	auto* amount = form->InsertChild( std::make_unique<tatzel::ui::NumberInput>( "amount", form, "Amount", 0, std::function<void( std::optional<double> )>{} ) );
	auto* choice = form->InsertChild( std::make_unique<tatzel::ui::RadioGroup>( "choice", form, "Choice",
		std::vector<tatzel::ui::RadioOption>{ { "a", "A" }, { "b", "B" } }, "a", std::function<void( std::string_view )>{} ) );
	amount->SetRequired( true );
	amount->SetRange( 0, 10 );
	auto snapshot = std::vector<tatzel::ui::FieldSubmission>{ { amount, std::string{ "2.5" } }, { choice, std::string{ "missing" } } };
	assert( !form->ApplySubmission( snapshot ) && amount->GetValue() == 0 );
	snapshot[ 1 ].value = std::string{ "b" };
	assert( form->ApplySubmission( snapshot ) && form->Submit() && amount->GetValue() == 2.5 && choice->GetValue() == "b" );
	snapshot[ 0 ].value = std::string{ "NaN" };
	assert( form->ApplySubmission( snapshot ) && !form->Submit() && amount->HasInvalidInput() );

	auto destination = std::string{ "/forms?value=1#top" };
	auto link = tatzel::ui::Link{ "link", &tree.root, "<Home>", destination };
	auto image = tatzel::ui::Image{ "image", &tree.root, destination, "<Description>" };
	destination.assign( "changed" );
	assert( link.GetURL() == "/forms?value=1#top" && image.GetSource() == link.GetURL() );
	for( auto url : { "javascript:alert(1)", "JaVaScRiPt:alert(1)", "java\tscript:alert(1)", " data:text/html,hi", "vbscript:bad", "file:///tmp/test", "https:\\bad" } )
	{
		try { link.SetURL( url ); assert( false ); } catch( const tatzel::Exception& ) {}
		try { image.SetSource( url ); assert( false ); } catch( const tatzel::Exception& ) {}
	}
	link.SetURL( "mailto:contact@example.com" );
	link.SetURL( "tel:+15555555555" );
	link.SetURL( "HTTPS://example.com" );
	link.SetTarget( tatzel::ui::LinkTarget::NewTab );
	assert( link.GetTarget() == tatzel::ui::LinkTarget::NewTab );
	try { link.SetTarget( static_cast<tatzel::ui::LinkTarget>( 99 ) ); assert( false ); } catch( const tatzel::Exception& ) {}
	image.SetSize( 320, 160 );
	try { image.SetSize( 0, 20 ); assert( false ); } catch( const tatzel::Exception& ) {}
	assert( image.GetWidth() == 320 && image.GetHeight() == 160 );
	image.SetSize( {}, {} );
	image.SetSource( "" );
	image.SetAlternativeText( "" );
	image.SetLazyLoading( true );
	assert( !image.GetWidth() && !image.GetHeight() && image.GetSource().empty() && image.GetAlternativeText().empty() && image.IsLazyLoading() );
	return 0;
}
