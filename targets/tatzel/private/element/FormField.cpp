module;

#include <string>
#include <string_view>
#include <variant>

module UI.Element.FormField;

import UI.Diagnostics;

namespace tatzel::ui {

auto FormField::SetSize( ControlSize size ) -> void
{
	if( this->size == size ) return;
	this->size = size;
	NotifyChanged();
}

auto FormField::SetLabel( std::string_view label ) -> void
{
	if( this->label == label ) return;

	this->label = label;
	NotifyChanged();
}

auto FormField::SetDisabled( bool disabled ) -> void
{
	if( this->disabled == disabled ) return;

	this->disabled = disabled;
	if( disabled ) error.clear();
	NotifyChanged();
}

auto FormField::SetRequired( bool required ) -> void
{
	if( this->required == required ) return;

	this->required = required;
	NotifyChanged();
}

auto FormField::SetError( std::string_view error ) -> void
{
	if( this->error == error ) return;

	this->error = error;
	NotifyChanged();
}

auto FormField::CanAcceptFieldValue( const FieldValue& value ) const -> bool
{
	if( GetChangeValueType() == interface::ChangeValueType::Boolean ) return std::holds_alternative<bool>( value );
	auto* text = std::get_if<std::string>( &value );
	return text && AcceptsChangeValue( *text );
}

auto FormField::Validate() -> bool
{
	auto dispatch = BeginDispatch();
	if( IsRemoved() ) return true;

	if( disabled )
	{
		SetError( {} );
		return true;
	}

	auto value = GetFieldValue();
	const auto* text = std::get_if<std::string>( &value );
	const auto empty = text ? text->empty() : !std::get<bool>( value );
	if( required && empty ) SetError( "This field is required." );
	else
	{
		auto validator = validation_handler;
		auto result = std::string{};
		if( validator )
		{
			auto scope = tatzel::CallbackScope{ "Field validator" };
			result = validator();
		}
		if( IsRemoved() ) return true;
		SetError( result );
	}
	return IsRemoved() || error.empty();
}

auto FormField::CheckClientEdit(
	ClientEdit edit,
	const FieldValue& value
) const -> EditAcceptance
{
	if( edit.epoch > epoch || edit.revision > tatzel::MaximumWireRevision ) return EditAcceptance::Invalid;
	if( edit.epoch < epoch || edit.revision < client_revision ) return EditAcceptance::Stale;
	if( edit.revision == client_revision && !IsReadOnly() && value != GetFieldValue() ) return EditAcceptance::Invalid;
	return EditAcceptance::Current;
}

auto FormField::RecordServerValueChange( ValueUpdatePolicy policy ) -> void
{
	if( server_revision == tatzel::MaximumWireRevision || epoch == tatzel::MaximumWireRevision )
		throw tatzel::Exception{ "Field revision counter exhausted" };

	value_update = { epoch, epoch, client_revision, policy == ValueUpdatePolicy::Replace };
	++server_revision;
	if( policy == ValueUpdatePolicy::Replace )
	{
		value_update.epoch = ++epoch;
		client_revision = 0;
	}
}

}
