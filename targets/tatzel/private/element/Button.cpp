module;

#include <string_view>

module UI.Element.Button;

import UI.Diagnostics;

namespace tatzel::ui {

auto Button::SetText( std::string_view text ) -> void
{
	if( this->text == text ) return;
	this->text = text;
	NotifyChanged();
}

auto Button::SetVariant( ButtonVariant variant ) -> void
{
	if( this->variant == variant ) return;
	this->variant = variant;
	NotifyChanged();
}

auto Button::SetSize( ControlSize size ) -> void
{
	if( this->size == size ) return;
	this->size = size;
	NotifyChanged();
}

auto Button::SetDisabled( bool disabled ) -> void
{
	if( this->disabled == disabled ) return;
	this->disabled = disabled;
	NotifyChanged();
}

auto Button::InvokeOnClick() -> void
{
	auto dispatch = BeginDispatch();
	if( IsRemoved() || disabled || role != ButtonRole::Action ) return;

	auto callback = on_click;
	if( callback )
	{
		auto scope = tatzel::CallbackScope{ "Button click" };
		callback();
	}
}

}
