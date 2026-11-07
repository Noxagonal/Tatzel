module;

#include <string_view>

module UI.Element.Text;

namespace tatzel::ui {

auto Text::SetText( std::string_view text ) -> void
{
	if( this->text == text ) return;

	this->text = text;
	NotifyChanged();
}

auto Text::SetLiveRegion( LiveRegion live_region ) -> void
{
	if( this->live_region == live_region ) return;
	this->live_region = live_region;
	NotifyChanged();
}

}
