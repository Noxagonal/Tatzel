module;

#include <algorithm>
#include <functional>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

module UI.Element.Form;

import UI.Diagnostics;

namespace tatzel::ui {

Form::Form(
	std::string_view id,
	LogicalElement* parent,
	std::string_view label,
	std::function<void()> on_submit
) :
	LogicalElement{ id, parent },
	label{ label },
	on_submit{ std::move( on_submit ) }
{
	for( auto* ancestor = parent; ancestor; ancestor = ancestor->GetParent() )
	{
		if( dynamic_cast<Form*>( ancestor ) ) throw tatzel::Exception{ "Forms cannot be nested" };
	}
}

auto Form::SetLabel( std::string_view label ) -> void
{
	if( this->label == label ) return;

	this->label = label;
	NotifyChanged();
}

auto Form::SetDisabled( bool disabled ) -> void
{
	if( this->disabled == disabled ) return;

	this->disabled = disabled;
	NotifyChanged();
}

auto Form::GetFields() const -> std::vector<FormField*>
{
	auto fields = std::vector<FormField*>{};
	if( !IsRemoved() ) CollectFields( *this, fields );
	return fields;
}

auto Form::ApplySubmission( std::span<const FieldSubmission> submission ) -> bool
{
	auto dispatch = BeginDispatch();
	if( IsRemoved() || disabled ) return false;

	auto fields = GetFields();
	std::erase_if( fields, []( const auto* field ) { return field->IsDisabled(); } );
	if( fields.size() != submission.size() ) return false;

	auto seen = std::vector<FormField*>{};
	for( const auto& entry : submission )
	{
		if( !entry.field || std::ranges::find( fields, entry.field ) == fields.end()
			|| std::ranges::find( seen, entry.field ) != seen.end()
			|| !entry.field->CanAcceptFieldValue( entry.value )
			|| ( entry.edit && entry.field->CheckClientEdit( *entry.edit, entry.value ) != EditAcceptance::Current ) ) return false;
		seen.push_back( entry.field );
	}

	for( const auto& entry : submission )
	{
		if( entry.edit ) entry.field->AcceptClientEdit( *entry.edit );
		entry.field->AcceptFieldValue( entry.value );
	}
	return true;
}

auto Form::Validate() -> bool
{
	auto dispatch = BeginDispatch();
	if( IsRemoved() ) return false;
	if( disabled ) return true;

	auto valid = true;
	for( auto* field : GetFields() )
	{
		if( !field->IsRemoved() && !field->Validate() ) valid = false;
		if( IsRemoved() ) return false;
	}
	return valid;
}

auto Form::HasErrors() const -> bool
{
	return !IsRemoved() && !disabled && std::ranges::any_of( GetFields(), []( const auto* field ) {
		return !field->IsDisabled() && !field->GetError().empty();
	} );
}

auto Form::Submit() -> bool
{
	auto dispatch = BeginDispatch();
	if( IsRemoved() || disabled ) return false;

	const auto valid = Validate();
	if( IsRemoved() ) return false;
	auto callback = valid ? on_submit : on_invalid;
	if( callback )
	{
		auto scope = tatzel::CallbackScope{ valid ? "Form submit" : "Form invalid" };
		callback();
	}
	return valid;
}

auto Form::CollectFields(
	const LogicalElement& parent,
	std::vector<FormField*>& fields
) -> void
{
	for( const auto& child : parent.GetChildren() )
	{
		if( auto* field = dynamic_cast<FormField*>( child.get() ) ) fields.push_back( field );
		CollectFields( *child, fields );
	}
}

}
