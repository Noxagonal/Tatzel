module;

#include <drogon/drogon.h>
#include "drogon/HttpAppFramework.h"

#include "PCH.hpp"

module UI.App;

import UI.Utility;
import UI.UI.LogicalElement;
import UI.WSController;


tatzel::App* global_app = nullptr;


tatzel::App::App()
{
	drogon::app().loadConfigFile( "./data/drogon_config.json" );
	if( global_app ) throw std::runtime_error{ "Global app already exists" };
	global_app = this;
}

tatzel::App::~App()
{
	global_app = nullptr;
}


auto tatzel::App::RegisterPage( std::string_view route_path, std::function<void( PageBuilder<ui::LogicalElement> )>&& page_builder_fn ) -> void
{
	drogon::app().registerHandler(
		std::string{ route_path },
		[] (
			const drogon::HttpRequestPtr& request,
			std::function<void( const drogon::HttpResponsePtr& )>&& response_callback
		)
		{
			// This entrypoint will only provide the browser with a template html. The path will
			// handled later when the websockets connection has been established by the browser.
			auto html = tatzel::GetTemplateDocument( "./data/template.html" );

			auto response = drogon::HttpResponse::newHttpResponse();
			response->setContentTypeCode( drogon::CT_TEXT_HTML );
			response->setBody( std::move( html ) );

			response_callback( response );
		}
	);

	registered_page_list.push_back(
		Page {
			route_path,
			std::move( page_builder_fn )
		}
	);
}


auto tatzel::App::FindRegisteredPage( std::string_view route_path ) -> Page*
{
	auto it = std::ranges::find_if( registered_page_list, [route_path]( auto& p )
	{
		return p.route_path == route_path;
	} );
	if( it == registered_page_list.end() ) return nullptr;
	return &*it;
}


auto tatzel::App::Run() -> void
{
	drogon::app().registerController(
		std::make_shared<WsController>()
	);
	drogon::app()
		.addListener("0.0.0.0", 8080)
		.run();
}


auto tatzel::GetGlobalApp() -> App&
{
	if( !global_app ) throw std::runtime_error{ "Global app instance not running" };
	return *global_app;
}
