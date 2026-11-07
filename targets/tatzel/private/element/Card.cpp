module;

#include <string_view>

module UI.Element.Card;

namespace tatzel::ui {

auto Card::SetTitle( std::string_view title ) -> void
{
	if( this->title == title ) return;

	this->title = title;
	NotifyChanged();
}

}
