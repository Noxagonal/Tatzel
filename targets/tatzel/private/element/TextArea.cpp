module;

#include <cstddef>

module UI.Element.TextArea;

import UI.Diagnostics;

namespace tatzel::ui {

auto TextArea::SetRows( size_t rows ) -> void
{
	if( rows == 0 ) throw tatzel::Exception{ "TextArea rows must be positive" };
	if( this->rows == rows ) return;

	this->rows = rows;
	NotifyChanged();
}

}
