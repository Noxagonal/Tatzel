#include "Showcases.hpp"

#include <functional>
#include <string_view>
#include <utility>
#include <vector>

namespace example {

namespace {

/// @brief Associates a showcase's index label with its registered page factory.
struct Showcase
{
	std::string_view route; ///< Static absolute route.
	std::string_view label; ///< Static index link text.
	std::function<void( tatzel::PageBuilder<> )> build; ///< Per-client logical page factory.
};

}

auto RegisterShowcases(
	tatzel::App& app,
	trantor::ConcurrentTaskQueue& background
) -> void
{
	auto showcases = std::vector<Showcase>{
		{ "/controls", "Basic controls and profile preview", BuildControlsShowcase },
		{ "/forms", "Forms and validation", BuildFormsShowcase },
		{ "/elements", "Numbers, radio groups, links, and images", BuildElementsShowcase },
		{ "/lifecycle", "Element lifecycle and removal", BuildLifecycleShowcase },
		{ "/dispatcher", "Background work and dispatcher", [ &background ]( auto ui ) { BuildDispatcherShowcase( ui, background ); } },
		{ "/protocol", "Protocol reliability", [ &background ]( auto ui ) { BuildProtocolShowcase( ui, background ); } },
		{ "/components", "Reusable components and themes", [ &background ]( auto ui ) { BuildComponentsShowcase( ui, background ); } },
		{ "/tables", "Logical tables and stable rows", BuildTablesShowcase },
		{ "/failed-page", "Page initialization failure (expected error)", BuildFailureShowcase }
	};

	auto links = std::vector<std::pair<std::string_view, std::string_view>>{};
	for( auto& showcase : showcases )
	{
		links.emplace_back( showcase.route, showcase.label );
		app.RegisterPage( showcase.route, std::move( showcase.build ) );
	}

	app.RegisterPage( "/", [ links = std::move( links ) ]( tatzel::PageBuilder<> ui ) {
		ui.Column( [ &links ]( auto list ) {
			for( const auto& [ route, label ] : links ) list.Link( label, route );
		} );
	} );
}

}
