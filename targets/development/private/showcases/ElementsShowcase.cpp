#include "Showcases.hpp"

#include <optional>
#include <string>
#include <string_view>

namespace example {

auto BuildElementsShowcase( tatzel::PageBuilder<> ui ) -> void
{
	ui.Heading( "Numbers, choices, and resources" );
	auto* status = ui.Paragraph( "Ready" );
	auto* link = ui.Link( "Tatzel home", "/", tatzel::ui::LinkTarget::NewTab );
	ui.Paragraph( "A local image with an explicit description:" );
	auto* image = ui.Image( "/framework/example-image.svg", "Tatzel mountain landscape" );
	image->SetSize( 320, 160 );
	image->SetLazyLoading( true );

	ui.Form( "Order", [ status, link, image ](
		auto fields,
		tatzel::ui::Form* form
	) {
		auto* amount = fields.NumberInput( "Amount", 1.5 );
		amount->SetRequired( true );
		amount->SetRange( 0, 100 );
		amount->SetStep( 0.5 );
		amount->SetOnInput( [ status ]( std::optional<double> value ) {
			status->SetText( value ? "Editing: " + std::to_string( *value ) : "Editing: empty or invalid" );
		} );
		auto* delivery = fields.RadioGroup( "Delivery", {
			{ "pickup", "Pickup" }, { "courier", "Courier" }, { "post", "Post (unavailable)", true }
		} );
		delivery->SetRequired( true );
		delivery->SetOnChange( [ status ]( std::string_view value ) {
			status->SetText( "Choice: " + std::string{ value } );
		} );
		fields.RadioGroup( "Packaging", { { "box", "Box" }, { "bag", "Bag" } }, "box" );
		form->SetOnSubmit( [ amount, delivery, status ] {
			auto value = amount->GetValue();
			status->SetText( "Ordered: " + ( value ? std::to_string( *value ) : std::string{ "none" } ) + " | " + std::string{ delivery->GetValue() } );
		} );
		form->SetOnInvalid( [ status ] { status->SetText( "Check highlighted fields" ); } );
		fields.SubmitButton( "Place order" );
		fields.Row( [ amount, delivery, link, image ]( auto actions ) {
			actions.Button( "Reset elements", [ amount, delivery, link, image ] {
				amount->SetValue( std::nullopt, tatzel::ui::ValueUpdatePolicy::Replace );
				delivery->SetValue( "", tatzel::ui::ValueUpdatePolicy::Replace );
				link->SetText( "Form example" );
				link->SetURL( "/forms" );
				link->SetTarget( tatzel::ui::LinkTarget::SameContext );
				image->SetAlternativeText( "" );
				image->SetSize( std::nullopt, std::nullopt );
				image->SetLazyLoading( false );
			} );
			actions.Button( "Refresh radios", [ delivery ] {
				delivery->SetOptions( { { "pickup", "Collect locally" }, { "bike", "Bike courier" } } );
			} );
			actions.Button( "Toggle fields", [ amount, delivery ] {
				amount->SetDisabled( !amount->IsDisabled() );
				delivery->SetDisabled( !delivery->IsDisabled() );
			} );
			actions.Button( "Toggle read only", [ amount ] { amount->SetReadOnly( !amount->IsReadOnly() ); } );
		}, { .gap = tatzel::ui::Spacing::Small, .wrap = true } );
	} );
}

}
