module;

#include <string_view>

module UI.Element.Modal;

import UI.Diagnostics;

namespace tatzel::ui {

auto Modal::SetTitle( std::string_view title ) -> void
{
	if( this->title == title ) return;

	this->title = title;
	NotifyChanged();
}

auto Modal::Open() -> void
{
	if( IsRemoved() || open ) return;

	open = true;
	NotifyChanged();
}

auto Modal::Close() -> void
{
	auto dispatch = BeginDispatch();
	if( IsRemoved() || !open ) return;

	open = false;
	NotifyChanged();
	if( IsRemoved() ) return;

	auto callback = on_close;
	if( callback )
	{
		auto scope = tatzel::CallbackScope{ "Modal close" };
		callback();
	}
}

}
