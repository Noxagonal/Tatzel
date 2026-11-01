module;

#include <drogon/drogon.h>
#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <stdexcept>

module UI.App;

import UI.Page;
import UI.PageBuilder;

import UI.Utility;
import UI.WSController;
import UI.Logging;

namespace {
	tatzel::App* global_app = nullptr;
}

tatzel::App::App()
{
	if( global_app ) throw std::runtime_error{ "Global app already exists" };
	drogon::app().loadConfigFile( "./data/drogon_config.json" );
	global_app = this;
}

tatzel::App::~App()
{
	global_app = nullptr;
}

auto tatzel::App::RegisterPage(
	std::string_view route_path,
	std::function<void( PageBuilder<> )>&& page_builder_fn
) -> void
{
	if( has_run ) throw std::logic_error{ "Register pages before starting the server" };
	if( route_path.empty() || route_path.front() != '/' || !page_builder_fn || FindRegisteredPage( route_path ) )
		throw std::invalid_argument{ "Page requires a unique absolute route and a builder" };

	drogon::app().registerHandler(
		std::string{ route_path },
		[](
			const drogon::HttpRequestPtr& request,
			std::function<void( const drogon::HttpResponsePtr& )>&& response_callback
		)
		{
			// The logical page is built after the browser identifies its route over WebSocket.
			auto response = drogon::HttpResponse::newHttpResponse();
			response->setContentTypeCode( drogon::CT_TEXT_HTML );
			response->setBody( GetTemplateDocument( "./data/template.html" ) );
			response_callback( response );
		}
	);
	registered_page_list.emplace_back( route_path, std::move( page_builder_fn ) );
}

auto tatzel::App::FindRegisteredPage( std::string_view route_path ) -> Page*
{
	auto found = std::ranges::find_if( registered_page_list,
		[ route_path ]( const auto& page ) { return page.route_path == route_path; } );
	return found == registered_page_list.end() ? nullptr : &*found;
}

auto tatzel::App::Run() -> void
{
	if( has_run ) throw std::logic_error{ "The application can only run once" };
	has_run = true;
	drogon::app().registerController( std::make_shared<WsController>() );
	drogon::app().registerBeginningAdvice( [ this ] {
		// Beginning advice precedes startListening; report readiness on the next loop turn.
		drogon::app().getLoop()->queueInLoop( [ this ] {
#ifdef NDEBUG
			const auto mode = "Release";
#else
			const auto mode = "Debug";
#endif
			Log( LogLevel::Info, std::string{ "Tatzel listening at http://0.0.0.0:8080 (" } + mode
				+ ", " + std::to_string( registered_page_list.size() ) + " routes)" );
			for( const auto& page : registered_page_list )
				Log( LogLevel::Debug, "Registered route: " + page.route_path );
		} );
	} );
	Log( LogLevel::Info, "Starting Tatzel on 0.0.0.0:8080" );
	drogon::app().addListener( "0.0.0.0", 8080 ).run();
	Log( LogLevel::Info, "Tatzel stopped" );
}

auto tatzel::GetGlobalApp() -> App&
{
	if( !global_app ) throw std::runtime_error{ "Global app instance not running" };
	return *global_app;
}
