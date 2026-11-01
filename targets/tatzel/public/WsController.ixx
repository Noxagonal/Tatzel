/// @file
/// @brief WebSocket transport and DOM-part-to-logical-event routing.
module;

#include <drogon/WebSocketController.h>
#include <json/json.h>
#include <trantor/utils/Logger.h>
#include <algorithm>
#include <exception>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>
#include <utility>

export module UI.WSController;

import UI.Utility;
import UI.ClientConnection;
import UI.PageBuilder;
import UI.App;
import UI.Element.Interface.Clickable;
import UI.Element.Interface.Changeable;
import UI.Element.Interface.Submittable;
import UI.Element.Interface.Inputable;
import UI.Element.Interface.Dismissible;
import UI.Element.Form;
import UI.Diagnostics;
import UI.Logging;

/// @brief Routes browser transport messages to the owning client's logical page.
/// @details Connection registry entries have stable shared ownership. Each logical
/// tree is used only on its WebSocket's event loop; the registry mutex protects
/// registry access rather than arbitrary cross-thread logical mutations.
export class WsController : public drogon::WebSocketController<WsController, false>
{
public:
	/// @brief Creates session services and acknowledges the browser transport.
	/// @param request HTTP upgrade request.
	/// @param ws_connection Newly opened browser transport.
	/// @pre Drogon supplies a nonnull connection on its owning event loop.
	/// @post A session exists and a connected op/data acknowledgement is queued.
	void handleNewConnection(
		const drogon::HttpRequestPtr& request,
		const drogon::WebSocketConnectionPtr& ws_connection
	) override
	{
		CreateClientConnection( ws_connection );
		ws_connection->send( R"({"op":"connected","data":{"message":"Connected"}})" );
	}

	/// @brief Removes a closed browser session from the registry.
	/// @param ws_connection Closed transport.
	/// @pre Drogon has stopped delivering new events for this connection.
	/// @post The registry no longer owns the session; retained in-flight handles remain valid.
	void handleConnectionClosed( const drogon::WebSocketConnectionPtr& ws_connection ) override
	{
		std::lock_guard lock{ mutex };
		std::erase_if( connections, [ &ws_connection ]( const auto& connection ) {
			return connection->ws_connection == ws_connection;
		} );
	}

	/// @brief Validates a browser message and dispatches it to logical page state.
	/// @param ws_connection Transport that sent the message.
	/// @param message UTF-8 browser payload.
	/// @param type WebSocket frame type.
	/// @pre Called by Drogon on the connection's owning event loop.
	/// @post Valid messages are handled; malformed messages and callback failures get structured errors.
	void handleNewMessage(
		const drogon::WebSocketConnectionPtr& ws_connection,
		std::string&& message,
		const drogon::WebSocketMessageType& type
	) override
	{
		if( type != drogon::WebSocketMessageType::Text ) return;
		auto connection = FindClientConnection( ws_connection );
		if( !connection )
		{
			SendError( ws_connection, "session_expired", "Session expired; reload the page" );
			return;
		}

		Json::Value json;
		try { json = tatzel::ParseJson( message ); }
		catch( const std::exception& )
		{
			SendError( ws_connection, "invalid_json", "Message must contain valid JSON" );
			return;
		}
		if( !json.isObject() || !json[ "op" ].isString() || !json[ "data" ].isObject() )
		{
			SendError( ws_connection, "invalid_message", "Expected an op string and a data object" );
			return;
		}

		auto route = std::string{};
		auto operation = std::string{};
		auto part_id = std::string{};
		auto element_id = std::string{};
		try
		{
			operation = json[ "op" ].asString();
			if( connection->page ) route = connection->page->route_path;
			else if( json[ "data" ][ "route_path" ].isString() ) route = json[ "data" ][ "route_path" ].asString();
			if( json[ "data" ][ "part_id" ].isString() )
			{
				part_id = json[ "data" ][ "part_id" ].asString();
				if( auto* owner = connection->element_tree->FindElementByPartID<tatzel::ui::LogicalElement>( part_id ) )
					element_id = owner->GetID();
			}
			auto dispatch = connection->element_tree->root.BeginDispatch();
			DispatchMessage( *connection, json );
		}
		catch( ... )
		{
			tatzel::LogException( std::current_exception(), { route, operation, element_id, part_id } );
			try { SendError( ws_connection, "handler_failed", "Page handler failed" ); }
			catch( ... )
			{
				tatzel::LogException( std::current_exception(), { route, "error_response", element_id, part_id } );
				try { ws_connection->forceClose(); }
				catch( ... ) { tatzel::Log( tatzel::LogLevel::Error, "Failed to close the affected WebSocket" ); }
			}
		}
	}

	/// @brief Sends a protocol message to a snapshot of all current transports.
	/// @param message Complete serialized server op/data message.
	/// @pre The message is valid for every recipient; callers maintain logical state separately.
	/// @post Each transport in the snapshot has been offered the message.
	static auto Broadcast( const std::string& message ) -> void
	{
		std::vector<std::shared_ptr<tatzel::ClientConnection>> recipients;
		{
			std::lock_guard lock{ mutex };
			recipients = connections;
		}
		for( const auto& connection : recipients ) connection->ws_connection->send( message );
	}

private:
	/// @brief Retains newly created services in the session registry.
	/// @param ws_connection Browser transport to own.
	/// @return Retained stable session handle.
	/// @pre The transport has no existing session.
	/// @post The registry owns the new session.
	static auto CreateClientConnection( const drogon::WebSocketConnectionPtr& ws_connection )
		-> std::shared_ptr<tatzel::ClientConnection>
	{
		auto connection = std::make_shared<tatzel::ClientConnection>( ws_connection );
		std::lock_guard lock{ mutex };
		connections.push_back( connection );
		return connection;
	}

	/// @brief Looks up a session while retaining its lifetime beyond the registry lock.
	/// @param ws_connection Browser transport to find.
	/// @return Stable session handle, or nullptr.
	/// @pre None.
	/// @post Registry state is unchanged.
	static auto FindClientConnection( const drogon::WebSocketConnectionPtr& ws_connection )
		-> std::shared_ptr<tatzel::ClientConnection>
	{
		std::lock_guard lock{ mutex };
		auto found = std::ranges::find_if( connections,
			[ &ws_connection ]( const auto& connection ) { return connection->ws_connection == ws_connection; } );
		return found == connections.end() ? nullptr : *found;
	}

	/// @brief Sends a structured error using the same envelope as other server messages.
	/// @param ws_connection Recipient transport.
	/// @param code Machine-readable error code.
	/// @param message Human-readable error description.
	/// @pre ws_connection is nonnull.
	/// @post An error op/data message is queued.
	static auto SendError(
		const drogon::WebSocketConnectionPtr& ws_connection,
		std::string_view code,
		std::string_view message
	) -> void
	{
		Json::Value json{ Json::objectValue };
		json[ "op" ] = "error";
		json[ "data" ][ "code" ] = std::string{ code };
		json[ "data" ][ "message" ] = std::string{ message };
		ws_connection->send( tatzel::JsonToString( json ) );
	}

	/// @brief Resolves a complete browser field snapshot before applying any submitted values.
	/// @param connection Owning client page and identity registry.
	/// @param form Logical form receiving the snapshot.
	/// @param data Validated event envelope containing a fields array.
	/// @return Whether every source, member, value type, and choice was accepted.
	/// @pre Called on the owning event loop after the form's source marker is checked.
	/// @post Invalid snapshots change no fields; valid snapshots update editable values without edit callbacks.
	static auto ApplyFormSubmission(
		tatzel::ClientConnection& connection,
		tatzel::ui::Form& form,
		const Json::Value& data
	) -> bool
	{
		if( !data[ "fields" ].isArray() ) return false;

		auto values = std::vector<tatzel::ui::FieldSubmission>{};
		for( const auto& entry : data[ "fields" ] )
		{
			if( !entry.isObject() || !entry[ "part_id" ].isString()
				|| ( !entry[ "value" ].isString() && !entry[ "value" ].isBool() ) ) return false;

			const auto part_id = entry[ "part_id" ].asString();
			auto* field = connection.element_tree->FindElementByPartID<tatzel::ui::FormField>( part_id );
			if( !field || !std::ranges::any_of( field->GetParts(), [ &part_id ]( const auto& part ) {
				return part.GetPartID() == part_id && part.GetAttribute( "data-ui-form-field" ).has_value();
			} ) ) return false;

			auto value = entry[ "value" ].isBool()
				? tatzel::ui::FieldValue{ entry[ "value" ].asBool() }
				: tatzel::ui::FieldValue{ entry[ "value" ].asString() };
			values.push_back( { field, std::move( value ) } );
		}
		return form.ApplySubmission( values );
	}

	/// @brief Handles a validated envelope using logical IDs only after resolving DOM parts.
	/// @param connection Owning client session.
	/// @param json Message with an op string and data object.
	/// @pre Called on the session's owning event loop.
	/// @post A page is built once or an event reaches the logical owner of its DOM part.
	static auto DispatchMessage(
		tatzel::ClientConnection& connection,
		const Json::Value& json
	) -> void
	{
		const auto operation = json[ "op" ].asString();
		const auto& data = json[ "data" ];
		const auto& transport = connection.ws_connection;

		if( operation == "connect" )
		{
			if( connection.page )
			{
				SendError( transport, "already_connected", "A page has already been built for this session" );
				return;
			}
			if( !data[ "route_path" ].isString() )
			{
				SendError( transport, "invalid_message", "connect requires route_path" );
				return;
			}
			auto* page = tatzel::GetGlobalApp().FindRegisteredPage( data[ "route_path" ].asString() );
			if( !page )
			{
				SendError( transport, "page_not_found", "Page not found" );
				return;
			}
			connection.page = page;
			auto builder = tatzel::PageBuilder{ *connection.page_builder_core, connection.element_tree->root };
			auto scope = tatzel::CallbackScope{ "Page builder" };
			page->page_builder_fn( builder );
			return;
		}

		if( operation != "on_click" && operation != "on_change" && operation != "on_submit"
			&& operation != "on_input" && operation != "on_dismiss" )
		{
			SendError( transport, "unknown_operation", "Unknown browser operation" );
			return;
		}
		if( !connection.page || !data[ "part_id" ].isString() || data[ "part_id" ].asString().empty() )
		{
			SendError( transport, "invalid_message", "Events require a connected page and a part_id" );
			return;
		}

		const auto part_id = data[ "part_id" ].asString();
		auto* owner = connection.element_tree->FindElementByPartID<tatzel::ui::LogicalElement>( part_id );
		if( !owner ) return;

		auto* owning_form = dynamic_cast<tatzel::ui::Form*>( owner );
		for( auto* ancestor = owner->GetParent(); ancestor && !owning_form; ancestor = ancestor->GetParent() )
		{
			owning_form = dynamic_cast<tatzel::ui::Form*>( ancestor );
		}
		if( owning_form && owning_form->IsDisabled() && operation != "on_dismiss" ) return;

		const auto marker = "data-ui-on-" + operation.substr( 3 );
		auto parts = owner->GetParts();
		auto source = std::ranges::find_if( parts, [ &part_id ]( const auto& part ) { return part.GetPartID() == part_id; } );
		// The adapter registers the exact parts that can emit each event.
		if( !source->GetAttribute( marker ) ) return;

		if( operation == "on_click" )
		{
			auto* control = dynamic_cast<tatzel::ui::interface::Clickable*>( owner );
			if( control ) control->InvokeOnClick();
		}
		else if( operation == "on_change" )
		{
			auto* control = dynamic_cast<tatzel::ui::interface::Changeable*>( owner );
			if( !control ) return;

			const auto accepts_boolean = control->GetChangeValueType() == tatzel::ui::interface::ChangeValueType::Boolean;
			if( accepts_boolean ? !data[ "value" ].isBool() : !data[ "value" ].isString() )
			{
				SendError( transport, "invalid_message", "Change value has the wrong type for its control" );
				return;
			}
			auto value = data[ "value" ].isBool()
				? std::string{ data[ "value" ].asBool() ? "true" : "false" } : data[ "value" ].asString();
			if( !control->AcceptsChangeValue( value ) )
			{
				SendError( transport, "invalid_message", "Change value is unavailable for this control" );
				return;
			}
			control->InvokeOnChange( value );
		}
		else if( operation == "on_submit" )
		{
			if( owning_form && owning_form != owner )
			{
				SendError( transport, "invalid_message", "Submit the enclosing form with its complete field snapshot" );
				return;
			}
			if( auto* form = dynamic_cast<tatzel::ui::Form*>( owner ); form && !ApplyFormSubmission( connection, *form, data ) )
			{
				SendError( transport, "invalid_message", "Form snapshot has missing, duplicate, foreign, or invalid field values" );
				return;
			}
			auto* control = dynamic_cast<tatzel::ui::interface::Submittable*>( owner );
			if( control ) control->InvokeOnSubmit();
		}
		else if( operation == "on_input" )
		{
			if( !data[ "value" ].isString() )
			{
				SendError( transport, "invalid_message", "on_input requires a string value" );
				return;
			}
			auto* control = dynamic_cast<tatzel::ui::interface::Inputable*>( owner );
			if( control ) control->InvokeOnInput( data[ "value" ].asString() );
		}
		else
		{
			auto* control = dynamic_cast<tatzel::ui::interface::Dismissible*>( owner );
			if( control ) control->InvokeOnDismiss();
		}
	}

	static inline std::mutex mutex;
	static inline std::vector<std::shared_ptr<tatzel::ClientConnection>> connections;

public:
	WS_PATH_LIST_BEGIN
		WS_PATH_ADD( "/ws" );
	WS_PATH_LIST_END
};
