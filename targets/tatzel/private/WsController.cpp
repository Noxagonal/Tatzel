module;

#include <drogon/WebSocketController.h>
#include <json/json.h>
#include <trantor/net/EventLoop.h>
#include <algorithm>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

module UI.WSController;

import UI.Internal.JSON;
import UI.Protocol;
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
import UI.DOM.ElementPart;

void WsController::handleNewConnection(
	const drogon::HttpRequestPtr& request,
	const drogon::WebSocketConnectionPtr& ws_connection
)
{
	try
	{
		CreateClientConnection( ws_connection );
		auto acknowledgment = Json::Value{ Json::objectValue };
		acknowledgment[ "op" ] = "connected";
		acknowledgment[ "data" ][ "message" ] = "Connected";
		acknowledgment[ "data" ][ "protocol_version" ] = tatzel::ProtocolVersion;
		ws_connection->send( tatzel::JsonToString( acknowledgment ) );
	}
	catch( ... )
	{
		tatzel::LogException( std::current_exception(), { {}, "connection_open" } );
		CloseTransport( ws_connection );
	}
}

void WsController::handleConnectionClosed( const drogon::WebSocketConnectionPtr& ws_connection )
{
	try
	{
		auto connection = FindClientConnection( ws_connection );
		if( connection ) connection->Close();
		std::lock_guard lock{ mutex };
		std::erase_if( connections, [ &ws_connection ]( const auto& connection ) {
			return connection->ws_connection == ws_connection;
		} );
	}
	catch( ... ) { tatzel::LogException( std::current_exception(), { {}, "connection_close" } ); }
}

void WsController::handleNewMessage(
	const drogon::WebSocketConnectionPtr& ws_connection,
	std::string&& message,
	const drogon::WebSocketMessageType& type
)
{
	auto route = std::string{};
	auto operation = std::string{};
	auto part_id = std::string{};
	auto element_id = std::string{};
	auto connection = std::shared_ptr<tatzel::ClientConnection>{};
	try
	{
		operation = "receive_message";
		if( type != drogon::WebSocketMessageType::Text ) return;
		connection = FindClientConnection( ws_connection );
		if( !connection )
		{
			SendError( ws_connection, "session_expired", "Session expired; reload the page" );
			return;
		}
		if( connection->closed ) return;
		if( connection->page ) route = connection->page->route_path;
		operation = "parse_json";
		Json::Value json;
		try { json = tatzel::ParseJson( message ); }
		catch( const tatzel::Exception& )
		{
			ReportFailure( ws_connection, std::current_exception(), { route, operation },
				"invalid_json", "Message must contain valid JSON" );
			return;
		}
		if( !json.isObject() || !json[ "op" ].isString() || !json[ "data" ].isObject() )
		{
			SendError( ws_connection, "invalid_message", "Expected an op string and a data object" );
			return;
		}
		operation = json[ "op" ].asString();
		if( connection->page ) route = connection->page->route_path;
		else if( json[ "data" ][ "route_path" ].isString() ) route = json[ "data" ][ "route_path" ].asString();
		if( json[ "data" ][ "part_id" ].isString() )
		{
			part_id = json[ "data" ][ "part_id" ].asString();
			if( auto* owner = connection->element_tree->FindElementByPartID<tatzel::ui::LogicalElement>( part_id ) )
				element_id = owner->GetID();
		}
		{
			auto updates = connection->client_updater->BeginBatch();
			auto dispatch = connection->element_tree->root.BeginDispatch();
			DispatchMessage( *connection, json );
		}
		if( connection->page_built && !connection->page_ready && !connection->closed )
		{
			connection->client_updater->SendReady();
			connection->page_ready = true;
		}
	}
	catch( ... )
	{
		const auto fatal = connection && connection->page && !connection->page_ready;
		ReportFailure( ws_connection, std::current_exception(), { route, operation, element_id, part_id },
			fatal ? "page_failed" : "handler_failed",
			fatal ? "The page could not be prepared. Reload to try again." : "A page operation failed. You can continue or retry.", fatal );
	}
}

auto WsController::Broadcast( const std::string& message ) -> void
{
	std::vector<std::shared_ptr<tatzel::ClientConnection>> recipients;
	{
		std::lock_guard lock{ mutex };
		recipients = connections;
	}
	for( const auto& connection : recipients ) connection->ws_connection->send( message );
}

auto WsController::CloseTransport( const drogon::WebSocketConnectionPtr& transport ) noexcept -> void
{
	try { transport->forceClose(); }
	catch( ... ) { tatzel::LogException( std::current_exception(), { {}, "transport_close" } ); }
}

auto WsController::ReportFailure(
	const drogon::WebSocketConnectionPtr& transport,
	std::exception_ptr exception,
	const tatzel::ErrorContext& context,
	std::string_view code,
	std::string_view message,
	bool fatal,
	std::source_location location
) noexcept -> void
{
	tatzel::LogException( exception, context, location );
	try { SendError( transport, code, message, fatal ); }
	catch( ... )
	{
		tatzel::LogException( std::current_exception(), { context.route, "error_response", context.element_id, context.part_id } );
		CloseTransport( transport );
	}
}

auto WsController::CreateClientConnection( const drogon::WebSocketConnectionPtr& ws_connection )
	-> std::shared_ptr<tatzel::ClientConnection>
{
	auto connection = tatzel::ClientConnection::Create( ws_connection );
	RegisterLoopCleanup();
	std::lock_guard lock{ mutex };
	connections.push_back( connection );
	return connection;
}

auto WsController::RegisterLoopCleanup() -> void
{
	static thread_local bool registered = false;
	if( registered ) return;
	trantor::EventLoop::getEventLoopOfCurrentThread()->runOnQuit( [] {
		ReleaseLoopConnections();
		registered = false;
	} );
	registered = true;
}

auto WsController::ReleaseLoopConnections() noexcept -> void
{
	try
	{
		for( ;; )
		{
			auto connection = std::shared_ptr<tatzel::ClientConnection>{};
			{
				auto lock = std::lock_guard{ mutex };
				auto found = std::ranges::find_if( connections,
					[]( const auto& item ) { return item->IsOnCurrentLoop(); } );
				if( found == connections.end() ) return;
				connection = std::move( *found );
				connections.erase( found );
			}
			connection->Close();
		}
	}
	catch( ... ) { tatzel::LogException( std::current_exception(), { {}, "session_shutdown" } ); }
}

auto WsController::FindClientConnection( const drogon::WebSocketConnectionPtr& ws_connection )
	-> std::shared_ptr<tatzel::ClientConnection>
{
	std::lock_guard lock{ mutex };
	auto found = std::ranges::find_if( connections,
		[ &ws_connection ]( const auto& connection ) { return connection->ws_connection == ws_connection; } );
	return found == connections.end() ? nullptr : *found;
}

auto WsController::SendError(
	const drogon::WebSocketConnectionPtr& ws_connection,
	std::string_view code,
	std::string_view message,
	bool fatal
) -> void
{
	if( auto connection = FindClientConnection( ws_connection ) )
	{
		connection->client_updater->SendError( code, message, fatal );
		if( fatal ) connection->Close();
		return;
	}
	Json::Value json{ Json::objectValue };
	json[ "op" ] = "error";
	json[ "data" ][ "code" ] = std::string{ code };
	json[ "data" ][ "message" ] = std::string{ message };
	json[ "data" ][ "fatal" ] = fatal;
	ws_connection->send( tatzel::JsonToString( json ) );
}

auto WsController::ReadEdit( const Json::Value& data ) -> std::optional<tatzel::ui::ClientEdit>
{
	if( !data[ "epoch" ].isUInt64() || !data[ "revision" ].isUInt64()
		|| data[ "epoch" ].asUInt64() > tatzel::MaximumWireRevision
		|| data[ "revision" ].asUInt64() > tatzel::MaximumWireRevision ) return std::nullopt;

	return tatzel::ui::ClientEdit{ data[ "epoch" ].asUInt64(), data[ "revision" ].asUInt64() };
}

auto WsController::ApplyFormSubmission(
	tatzel::ClientConnection& connection,
	tatzel::ui::Form& form,
	const Json::Value& data
) -> std::string_view
{
	if( !data[ "fields" ].isArray() ) return "invalid_message";

	auto values = std::vector<tatzel::ui::FieldSubmission>{};
	for( const auto& entry : data[ "fields" ] )
	{
		if( !entry.isObject() || !entry[ "part_id" ].isString()
			|| ( !entry[ "value" ].isString() && !entry[ "value" ].isBool() ) ) return "invalid_message";

		const auto part_id = entry[ "part_id" ].asString();
		auto* field = connection.element_tree->FindElementByPartID<tatzel::ui::FormField>( part_id );
		if( !field || !std::ranges::any_of( field->GetParts(), [ &part_id ]( const auto& part ) {
			return part.GetPartID() == part_id && part.GetAttribute( "data-ui-form-field" ).has_value();
		} ) ) return "invalid_message";

		auto value = entry[ "value" ].isBool()
			? tatzel::ui::FieldValue{ entry[ "value" ].asBool() }
			: tatzel::ui::FieldValue{ entry[ "value" ].asString() };
		auto edit = ReadEdit( entry );
		if( !edit ) return "invalid_message";
		const auto accepted = field->CheckClientEdit( *edit, value );
		if( accepted == tatzel::ui::EditAcceptance::Stale ) return "stale_form";
		if( accepted == tatzel::ui::EditAcceptance::Invalid ) return "invalid_message";
		values.push_back( { field, std::move( value ), edit } );
	}
	return form.ApplySubmission( values ) ? "" : "invalid_message";
}

auto WsController::DispatchMessage(
	tatzel::ClientConnection& connection,
	const Json::Value& json
) -> void
{
	const auto operation = json[ "op" ].asString();
	const auto& data = json[ "data" ];
	const auto& transport = connection.ws_connection;

	if( operation == "connect" )
	{
		if( !data[ "protocol_version" ].isUInt() || data[ "protocol_version" ].asUInt() != tatzel::ProtocolVersion )
		{
			SendError( transport, "protocol_mismatch", "The page and server use different versions. Reload the page.", true );
			return;
		}
		if( connection.page )
		{
			SendError( transport, "already_connected", "A page has already been built for this session" );
			return;
		}
		if( data.isMember( "hash" ) && ( !data[ "hash" ].isString() || data[ "hash" ].asString().size() > 2048
			|| ( !data[ "hash" ].asString().empty() && data[ "hash" ].asString().front() != '#' ) ) )
		{
			SendError( transport, "invalid_message", "connect hash must be an empty or hash-prefixed location" );
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
		connection.page_builder_core->Navigate( data[ "hash" ].asString(), false );
		connection.page_built = true;
		return;
	}

	if( !connection.page_ready )
	{
		SendError( transport, "page_not_ready", "The page is not ready for interaction." );
		return;
	}

	if( operation == "on_navigate" )
	{
		if( !connection.page || !data[ "hash" ].isString() || data[ "hash" ].asString().size() > 2048
			|| ( !data[ "hash" ].asString().empty() && data[ "hash" ].asString().front() != '#' ) )
		{
			SendError( transport, "invalid_message", "Navigation requires a connected page and a hash location" );
			return;
		}
		connection.page_builder_core->Navigate( data[ "hash" ].asString(), false );
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

	if( operation == "on_input" || operation == "on_change" )
	{
		if( auto* field = dynamic_cast<tatzel::ui::FormField*>( owner ) )
		{
			if( field->IsDisabled() || field->IsReadOnly() ) return;
			auto edit = ReadEdit( data );
			if( !edit || ( !data[ "value" ].isString() && !data[ "value" ].isBool() ) )
			{
				SendError( transport, "invalid_message", "Field edits require safe epoch and revision counters." );
				return;
			}
			auto value = data[ "value" ].isBool() ? tatzel::ui::FieldValue{ data[ "value" ].asBool() }
				: tatzel::ui::FieldValue{ data[ "value" ].asString() };
			const auto accepted = field->CheckClientEdit( *edit, value );
			if( accepted == tatzel::ui::EditAcceptance::Stale ) return;
			if( accepted == tatzel::ui::EditAcceptance::Invalid || !field->CanAcceptFieldValue( value ) )
			{
				SendError( transport, "invalid_message", "Field value or revision is invalid." );
				return;
			}
			if( edit->revision == field->GetClientRevision() ) return;
			field->AcceptClientEdit( *edit );
		}
	}

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
		if( auto* form = dynamic_cast<tatzel::ui::Form*>( owner ) )
		{
			const auto error = ApplyFormSubmission( connection, *form, data );
			if( !error.empty() )
			{
				SendError( transport, error, error == "stale_form"
					? "Some fields changed before submission. Review the current values and submit again."
					: "Form snapshot has missing, duplicate, foreign, or invalid field values" );
				return;
			}
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
