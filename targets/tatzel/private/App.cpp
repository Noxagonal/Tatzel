module;

#include <drogon/drogon.h>
#include "RuntimeConfiguration.hpp"
#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

module UI.App;

import UI.Diagnostics;

import UI.Page;
import UI.PageBuilder;

import UI.Utility;
import UI.WSController;
import UI.Logging;

namespace {
	tatzel::App* global_app = nullptr;

	/// @brief Chooses an owned absolute runtime root with explicit paths taking precedence.
	/// @param requested Explicit application resource directory, or empty for discovery.
	/// @return Absolute resource directory; no file ownership is retained.
	/// @pre Environment variables are not concurrently modified.
	/// @post Filesystem state is unchanged.
	/// @throws tatzel::Exception If the chosen runtime directory does not exist.
	auto ResolveResourceDirectory( std::string_view requested ) -> std::string
	{
		auto directory = std::filesystem::path{ requested };
		if( directory.empty() )
		{
			if( const auto* configured = std::getenv( "TATZEL_RESOURCE_DIRECTORY" ); configured && *configured ) directory = configured;
		}
#if defined( __linux__ )
		if( directory.empty() )
		{
			auto error = std::error_code{};
			auto executable = std::filesystem::read_symlink( "/proc/self/exe", error );
			if( !error )
			{
				auto adjacent = executable.parent_path() / "tatzel-runtime";
				if( std::filesystem::is_directory( adjacent, error ) ) directory = std::move( adjacent );
			}
		}
#endif
		if( directory.empty() ) directory = TATZEL_DEFAULT_RUNTIME_DIRECTORY;
		directory = std::filesystem::absolute( directory ).lexically_normal();
		if( !std::filesystem::is_directory( directory ) ) throw tatzel::Exception{ "Tatzel runtime directory does not exist: " + directory.string() };
		return directory.string();
	}

	/// @brief Resolves one owned resource path against an absolute runtime directory.
	/// @param directory Absolute runtime root.
	/// @param path Absolute path or resource-relative path.
	/// @return Owned absolute normalized path.
	/// @pre directory is absolute.
	/// @post No files are opened or changed.
	auto ResolveResourcePath(
		std::string_view directory,
		std::string_view path
	) -> std::string
	{
		return ( std::filesystem::path{ directory } / std::filesystem::path{ path } ).lexically_normal().string();
	}
}

tatzel::App::App( AppOptions options ) :
	resource_directory{ ResolveResourceDirectory( options.resource_directory ) },
	template_path{ ResolveResourcePath( resource_directory, options.template_path ) },
	listen_address{ std::move( options.listen_address ) },
	port{ options.port }
{
	if( global_app ) throw tatzel::Exception{ "Global app already exists" };
	if( options.template_path.empty() ) throw tatzel::Exception{ "Tatzel template path must not be empty" };
	if( listen_address.empty() || !port || !options.thread_count ) throw tatzel::Exception{ "Server address, port, and thread count must be nonempty" };

	if( !options.configuration_path.empty() ) drogon::app().loadConfigFile( ResolveResourcePath( resource_directory, options.configuration_path ) );
	if( !options.document_root.empty() ) drogon::app().setDocumentRoot( ResolveResourcePath( resource_directory, options.document_root ) );
	drogon::app().setThreadNum( options.thread_count );
	SetLogLevel( options.log_level );
	global_app = this;
}

tatzel::App::App(
	std::string_view configuration_path,
	std::string_view template_path
) :
	App{ AppOptions{
		.resource_directory = std::filesystem::current_path().string(),
		.configuration_path = std::string{ configuration_path },
		.template_path = std::string{ template_path },
		.document_root = ""
	} }
{}

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
					Log( LogLevel::Info, "Tatzel listening at http://" + listen_address + ":" + std::to_string( port ) + " (" + mode
						+ ", " + std::to_string( registered_page_list.size() ) + " routes)" );
					for( const auto& page : registered_page_list )
						Log( LogLevel::Debug, "Registered route: " + page.route_path );
				}
				catch( ... ) { LogException( std::current_exception(), { {}, "startup_report" } ); }
			} );
		}
		catch( ... ) { LogException( std::current_exception(), { {}, "startup_report" } ); }
	} );
	Log( LogLevel::Info, "Starting Tatzel on " + listen_address + ":" + std::to_string( port ) );
	drogon::app().addListener( listen_address, port ).run();
	Log( LogLevel::Info, "Tatzel stopped" );
}

auto tatzel::GetGlobalApp() -> App&
{
	if( !global_app ) throw tatzel::Exception{ "Global app instance not running" };
	return *global_app;
}
