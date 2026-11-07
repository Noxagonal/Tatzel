module;

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

module UI.Element.Checkbox;

import UI.Diagnostics;

namespace tatzel::ui {

auto Checkbox::SetChecked(
	bool checked,
	ValueUpdatePolicy policy
) -> void
{
	if( this->checked == checked && policy != ValueUpdatePolicy::Replace ) return;

	RecordServerValueChange( policy );

	this->checked = checked;
	++value_revision;
	NotifyValueChanged();
}

auto Checkbox::TrySetChecked(
	bool checked,
	const EditRevision& revision
) -> bool
{
	if( !IsEditRevisionCurrent( revision ) ) return false;
	SetChecked( checked );
	return true;
}

auto Checkbox::InvokeOnChange( std::string_view value ) -> void
{
	auto dispatch = BeginDispatch();
	if( IsRemoved() ) return;

	if( value != "true" && value != "false" )
	{
		throw tatzel::Exception{ "Checkbox changes require a boolean" };
	}
	if( IsDisabled() ) return;

	checked = value == "true";
	NotifyValueChanged();
	if( IsRemoved() ) return;

	auto callback = on_change;
	if( callback )
	{
		auto scope = tatzel::CallbackScope{ "Checkbox change" };
		callback( checked );
	}
}

auto Checkbox::SetValidator( std::function<std::string( bool )> validator ) -> void
{
	SetValidationHandler( [ this, validator = std::move( validator ) ] {
		auto snapshot = checked;
		return validator ? validator( snapshot ) : std::string{};
	} );
}

auto Checkbox::AcceptFieldValue( const FieldValue& value ) -> void
{
	if( IsDisabled() || IsReadOnly() ) return;

	this->checked = std::get<bool>( value );
	NotifyValueChanged();
}

}
