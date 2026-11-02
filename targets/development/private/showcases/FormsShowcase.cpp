#include "Showcases.hpp"

#include <string>
#include <string_view>

namespace example {

auto BuildFormsShowcase( tatzel::PageBuilder<> ui ) -> void
{
	ui.Heading( "Contact form" );
	auto* status = ui.Text( "Ready" );

	auto* form = ui.Form( "Contact details", [ status ](
		auto content,
		tatzel::ui::Form* form
	) {
		content.Column( [ form, status ]( auto fields ) {
			auto* name = fields.TextInput( "Name" );
			name->SetRequired( true );
			auto* delivery = fields.Select( "Delivery", {
				{ "email", "Email" }, { "phone", "Phone" }, { "postal", "Mail", true }
			} );
			delivery->SetRequired( true );

			auto* notes = fields.TextArea( "Notes" );
			notes->SetRows( 3 );
			notes->SetPlaceholder( "Add a message" );
			notes->SetValidator( []( std::string_view value ) -> std::string {
				return value.size() > 80 ? "Notes are too long." : "";
			} );
			auto* consent = fields.Checkbox( "I agree to be contacted" );
			consent->SetRequired( true );

			auto* reference = fields.TextInput( "Reference", "TATZEL-001" );
			reference->SetReadOnly( true );
			auto* internal = fields.TextInput( "Internal", "server-only" );
			internal->SetDisabled( true );

			fields.Row( [ delivery ](
				auto actions,
				tatzel::ui::Row* row
			) {
				actions.SubmitButton( "Send" );
				actions.Button( "Use phone", [ delivery ] { delivery->SetValue( "phone" ); } );
				actions.Button( "Refresh choices", [ delivery ] {
					delivery->SetOptions( { { "phone", "Call me" }, { "pickup", "In person" }, { "postal", "Mail", true } } );
				} );
				actions.Button( "Compact layout", [ row ] {
					row->SetGap( tatzel::ui::Spacing::Small );
					row->SetJustification( tatzel::ui::Justification::SpaceBetween );
				} );
			}, { .alignment = tatzel::ui::Alignment::Center, .wrap = true } );

			form->SetOnInvalid( [ status ] { status->SetText( "Check highlighted fields" ); } );
			form->SetOnSubmit( [ status, name, delivery, notes, reference ] {
				status->SetText( "Sent: " + std::string{ name->GetValue() } + " | "
					+ std::string{ delivery->GetValue() } + " | " + std::string{ notes->GetValue() }
					+ " | " + std::string{ reference->GetValue() } );
			} );
		} );
	} );

	ui.Button( "Toggle form", [ form ] { form->SetDisabled( !form->IsDisabled() ); } );
}

}
