module;

#include <string>
#include <string_view>

module UI.PageBuilderCore;

import UI.ClientUpdater;
import UI.Diagnostics;

namespace tatzel {

auto PageBuilderCore::Navigate(
	std::string_view hash,
	bool update_browser
) -> void
{
	if( hash.size() > 2048 || ( !hash.empty() && hash.front() != '#' ) )
		throw Exception{ "Navigation requires a hash location of at most 2048 bytes" };
	auto location = std::string{ hash };
	if( update_browser ) client_updater->SetLocation( location );
	auto handler = navigation_handler;
	if( handler )
	{
		auto boundary = CallbackScope{ "Page navigation" };
		handler( location );
	}
}

}
