module;

#include <drogon/WebSocketController.h>

#include <string>
#include <mutex>

export module UI.WSController;

import UI.Utility;
import UI.ClientConnection;
import UI.PageBuilder;
import UI.App;

import UI.UI.Interface.Clickable;
import UI.UI.Interface.Changeable;
import UI.UI.Interface.Submittable;


export
class WsController : public drogon::WebSocketController<WsController>
{
public:
	static inline std::mutex mutex;
	static inline std::vector<tatzel::ClientConnection> connections;

	void handleNewConnection(
		const drogon::HttpRequestPtr& request,
		const drogon::WebSocketConnectionPtr& ws_connection
	) override
	{
		CreateClientConnection( ws_connection );

		ws_connection->send( R"({"type":"ok","message":"Connected"})" );
	}

	void handleConnectionClosed(
		const drogon::WebSocketConnectionPtr& ws_connection
	) override
	{
		EraseClientConnection( ws_connection );
	}

	void handleNewMessage(
		const drogon::WebSocketConnectionPtr& ws_connection,
		std::string&& message,
		const drogon::WebSocketMessageType& type
	) override
	{
		// Only accept text.
		if( type != drogon::WebSocketMessageType::Text ) return;

		auto client_connection = FindClientConnection( ws_connection );
		if( !client_connection )
		{
			ws_connection->send( R"({"type":"error","message":"Session expired, please reconnect"})" );
			return;
		}

		auto json = tatzel::ParseJson( message );
		auto json_op = json[ "op" ].asString();

		if( json_op == "connect" )
		{
			auto& json_data = json[ "data" ];
			if( json_data.empty() )
			{
				ws_connection->send( R"({"type":"error","message":"Payload not found"})"  );
				return;
			}

			// Handle new connection.
			auto route_path = json_data[ "route_path" ].asString();
			auto* page = tatzel::GetGlobalApp().FindRegisteredPage( route_path );

			if( page == nullptr )
			{
				ws_connection->send( R"({"type":"error","message":"Page not found"})" );
				return;
			}
			client_connection->page = page;

			// Build initial page contents.
			auto page_builder = tatzel::PageBuilder{
				*client_connection->page_builder_core,
				*client_connection->page_builder_core->GetRootElement()
			};
			client_connection->page->page_builder_fn( page_builder );

			return;
		}

		if( json_op == "on_click" )
		{
			auto& json_data = json[ "data" ];
			if( json_data.empty() )
			{
				ws_connection->send( R"({"type":"error","message":"Payload not found"})"  );
				return;
			}

			auto part_id = json_data[ "part_id" ].asString();
			auto element = client_connection->client_dom_tree->FindElementById<tatzel::ui::interface::Clickable>( part_id );
			if( element ) element->InvokeOnClick();
			return;
		}

		if( json_op == "on_change" )
		{
			auto& json_data = json[ "data" ];
			if( json_data.empty() )
			{
				ws_connection->send( R"({"type":"error","message":"Payload not found"})"  );
				return;
			}

			auto part_id = json_data[ "part_id" ].asString();
			auto element = client_connection->client_dom_tree->FindElementById<tatzel::ui::interface::Changeable>( part_id );
			if( element ) element->InvokeOnChange( json[ "value" ].asString() );
			return;
		}

		if( json_op == "on_submit" )
		{
			auto& json_data = json[ "data" ];
			if( json_data.empty() )
			{
				ws_connection->send( R"({"type":"error","message":"Payload not found"})"  );
				return;
			}

			auto part_id = json_data[ "part_id" ].asString();
			auto element = client_connection->client_dom_tree->FindElementById<tatzel::ui::interface::Submittable>( part_id );
			if( element ) element->InvokeOnSubmit();
			return;
		}
	}

	static auto Broadcast( const std::string& message ) -> void
	{
		std::lock_guard lock(mutex);

		for(const auto& connection : connections)
		{
			connection.ws_connection->send( message );
		}
	}

private:

	static auto EraseClientConnection( const drogon::WebSocketConnectionPtr& ws_connection ) -> void
	{
		std::lock_guard lock( mutex );
		auto it = std::ranges::find_if( connections, [ &ws_connection ]( auto& c ){
			return c.ws_connection == ws_connection;
		});
		if( it == connections.end() ) return;
		connections.erase( it );
	}

	static auto CreateClientConnection( const drogon::WebSocketConnectionPtr& ws_connection ) -> tatzel::ClientConnection*
	{
		std::lock_guard lock( mutex );
		connections.push_back(
			tatzel::ClientConnection{
				ws_connection
			}
		);
		return &connections.back();
	}

	static auto FindClientConnection( const drogon::WebSocketConnectionPtr& ws_connection ) -> tatzel::ClientConnection*
	{
		std::lock_guard lock( mutex );
		auto it = std::ranges::find_if( connections, [&ws_connection]( auto& c ){ return c.ws_connection == ws_connection; } );
		if( it == connections.end() ) return nullptr;
		return &*it;
	}

public:
	WS_PATH_LIST_BEGIN
		WS_PATH_ADD("/ws");
	WS_PATH_LIST_END
};
