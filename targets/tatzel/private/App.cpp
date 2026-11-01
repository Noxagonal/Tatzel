module;

#include <drogon/drogon.h>
#include <algorithm>
#include <exception>
#include <functional>
#include <memory>
#include <string>
#include <string_view>

module UI.App;

import UI.Diagnostics;

import UI.Page;
import UI.PageBuilder;

import UI.Utility;
import UI.WSController;
import UI.Logging;

namespace {
	tatzel::App* global_app = nullptr;
}

tatzel::App::App(
	std::string_view configuration_path,
	std::string_view template_path
) :
	template_path{ template_path }
{
	if( global_app ) throw tatzel::Exception{ "Global app already exists" };
	drogon::app().loadConfigFile( std::string{ configuration_path } );
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
	if( has_run ) throw tatzel::Exception{ "Register pages before starting the server" };
	if( route_path.empty() || route_path.front() != '/' || !page_builder_fn || FindRegisteredPage( route_path ) )
		throw tatzel::Exception{ "Page requires a unique absolute route and a builder" };

	drogon::app().registerHandler(
		std::string{ route_path },
		[ route = std::string{ route_path }, document_path = template_path ](
			const drogon::HttpRequestPtr& request,
			std::function<void( const drogon::HttpResponsePtr& )>&& response_callback
		)
		{
			// The logical page is built after the browser identifies its route over WebSocket.
			auto response = drogon::HttpResponsePtr{};
			try
			{
				response = drogon::HttpResponse::newHttpResponse();
				response->setContentTypeCode( drogon::CT_TEXT_HTML );
				response->setBody( GetTemplateDocument( document_path ) );
			}
			catch( ... )
			{
				LogException( std::current_exception(), { route, "http_request" } );
				try
				{
					response = drogon::HttpResponse::newHttpResponse();
					response->setStatusCode( drogon::k500InternalServerError );
					response->setContentTypeCode( drogon::CT_TEXT_PLAIN );
					response->setBody( "Internal server error" );
					response->setCloseConnection( true );
				}
				catch( ... )
				{
					LogException( std::current_exception(), { route, "http_error_response" } );
					return;
				}
			}
			try { response_callback( response ); }
			catch( ... ) { LogException( std::current_exception(), { route, "http_response" } ); }
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
	if( has_run ) throw tatzel::Exception{ "The application can only run once" };
	has_run = true;
	drogon::app().registerController( std::make_shared<WsController>() );
	drogon::app().registerBeginningAdvice( [ this ] {
		// Beginning advice precedes startListening; report readiness on the next loop turn.
		try
		{
			drogon::app().getLoop()->queueInLoop( [ this ] {
				try
				{
#ifdef NDEBUG
					const auto mode = "Release";
#else
					const auto mode = "Debug";
#endif
					Log( LogLevel::Info, std::string{ "Tatzel listening at http://0.0.0.0:8080 (" } + mode
						+ ", " + std::to_string( registered_page_list.size() ) + " routes)" );
					for( const auto& page : registered_page_list )
						Log( LogLevel::Debug, "Registered route: " + page.route_path );
				}
				catch( ... ) { LogException( std::current_exception(), { {}, "startup_report" } ); }
			} );
		}
		catch( ... ) { LogException( std::current_exception(), { {}, "startup_report" } ); }
	} );
	Log( LogLevel::Info, "Starting Tatzel on 0.0.0.0:8080" );
	drogon::app().addListener( "0.0.0.0", 8080 ).run();
	Log( LogLevel::Info, "Tatzel stopped" );
}

auto tatzel::GetGlobalApp() -> App&
{
	if( !global_app ) throw tatzel::Exception{ "Global app instance not running" };
	return *global_app;
}
