module;

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

module UI.Element.TextInput;

import UI.Diagnostics;

namespace tatzel::ui {

auto TextInput::SetValue(
	std::string_view value,
	ValueUpdatePolicy policy
) -> void
{
	if( this->value == value && policy != ValueUpdatePolicy::Replace ) return;

	RecordServerValueChange( policy );

	this->value = value;
	++value_revision;
	NotifyValueChanged();
}

auto TextInput::TrySetValue(
	std::string_view value,
	const EditRevision& revision
) -> bool
{
	if( !IsEditRevisionCurrent( revision ) ) return false;
	SetValue( value );
	return true;
}

auto TextInput::SetPlaceholder( std::string_view placeholder ) -> void
{
	if( this->placeholder == placeholder ) return;

	this->placeholder = placeholder;
	NotifyChanged();
}

auto TextInput::SetReadOnly( bool read_only ) -> void
{
	if( this->read_only == read_only ) return;

	this->read_only = read_only;
	NotifyChanged();
}

auto TextInput::InvokeOnInput( std::string_view value ) -> void
{
	auto dispatch = BeginDispatch();
	if( IsRemoved() ) return;

	if( IsDisabled() || read_only || this->value == value ) return;

	this->value = value;
	NotifyValueChanged();
	if( IsRemoved() ) return;

	auto callback = on_input;
	auto snapshot = this->value;
	if( callback )
	{
		auto scope = tatzel::CallbackScope{ "Text input" };
		callback( snapshot );
	}
}

auto TextInput::InvokeOnChange( std::string_view value ) -> void
{
	auto dispatch = BeginDispatch();
	if( IsRemoved() ) return;

	if( IsDisabled() || read_only ) return;

	this->value = value;
	NotifyValueChanged();
	if( IsRemoved() ) return;

	auto callback = on_change;
	auto snapshot = this->value;
	if( callback )
	{
		auto scope = tatzel::CallbackScope{ "Text change" };
		callback( snapshot );
	}
}

auto TextInput::InvokeOnSubmit() -> void
{
	auto dispatch = BeginDispatch();
	if( IsRemoved() ) return;

	if( IsDisabled() ) return;

	auto callback = on_submit;
	auto snapshot = value;
	if( callback )
	{
		auto scope = tatzel::CallbackScope{ "Text submit" };
		callback( snapshot );
	}
}

auto TextInput::SetValidator( std::function<std::string( std::string_view )> validator ) -> void
{
	SetValidationHandler( [ this, validator = std::move( validator ) ] {
		auto snapshot = value;
		return validator ? validator( snapshot ) : std::string{};
	} );
}

auto TextInput::AcceptFieldValue( const FieldValue& value ) -> void
{
	if( IsDisabled() || IsReadOnly() ) return;

	this->value = std::get<std::string>( value );
	NotifyValueChanged();
}

}
