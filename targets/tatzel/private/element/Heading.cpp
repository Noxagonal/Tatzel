module;

#include <string_view>

module UI.Element.Heading;

namespace tatzel::ui {

auto Heading::SetText( std::string_view text ) -> void
{
	if( this->text == text ) return;
	this->text = text;
	NotifyChanged();
}

}
