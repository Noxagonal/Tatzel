module;

#include <functional>
#include <utility>

module UI.Component.ConfirmationDialog;

import UI.Diagnostics;

namespace tatzel::ui {

auto ConfirmationDialog::Request( std::function<void( bool )> on_result ) -> void
{
	if( pending || !on_result || !dialog.Get() ) throw tatzel::Exception{ "Confirmation requires an active idle dialog and a result callback" };
	this->on_result = std::move( on_result );
	pending = true;
	dialog.Get()->Open();
}

auto ConfirmationDialog::Resolve( bool accepted ) -> void
{
	auto dispatch = BeginDispatch();
	if( IsRemoved() || !pending ) return;
	pending = false;
	auto callback = std::move( on_result );
	if( auto* modal = dialog.Get() ) modal->Close();
	if( callback )
	{
		auto boundary = tatzel::CallbackScope{ "Confirmation result" };
		callback( accepted );
	}
}

}
