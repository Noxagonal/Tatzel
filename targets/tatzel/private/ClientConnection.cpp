module;

#include <drogon/WebSocketConnection.h>
#include <trantor/net/EventLoop.h>
#include <cassert>
#include <memory>
#include <span>
#include <utility>
#include <exception>
#include <functional>
#include <source_location>
#include <thread>

module UI.ClientConnection;

import UI.ClientElementTree;
import UI.ClientUpdater;
import UI.PageBuilderCore;
import UI.DOM.ElementPart;

import UI.ElementAdapter.TatzelElementAdapter;
import UI.Diagnostics;
import UI.Logging;

auto tatzel::ClientConnection::Create( drogon::WebSocketConnectionPtr ws_connection ) -> std::shared_ptr<ClientConnection>
{
	auto* loop = trantor::EventLoop::getEventLoopOfCurrentThread();
	if( !loop || !ws_connection ) throw Exception{ "Client connection requires a transport and its owning event loop" };

	auto connection = std::shared_ptr<ClientConnection>{ new ClientConnection{ std::move( ws_connection ) } };
	connection->dispatcher_owner = std::make_unique<DispatcherOwner>( *loop,
		[ weak = std::weak_ptr{ connection } ](
			std::move_only_function<void()>& callback,
			std::source_location origin
		) {
			if( auto retained = weak.lock() ) retained->InvokeDispatched( callback, origin );
		} );
	connection->page_builder_core = std::make_unique<PageBuilderCore>( *connection->element_tree,
		*connection->client_updater, *connection->element_adapter, connection->dispatcher_owner->GetDispatcher() );
	return connection;
}

tatzel::ClientConnection::ClientConnection( drogon::WebSocketConnectionPtr ws_connection ) :
	ws_connection{ std::move( ws_connection ) }
{
	assert( this->ws_connection );
	client_updater = std::make_unique<ClientUpdater>( this->ws_connection );
	element_adapter = std::make_unique<TatzelElementAdapter>( *client_updater );
	element_tree = std::make_unique<ClientElementTree>();
	element_tree->SetRemovalHandler( [ updater = client_updater.get() ]( std::span<const dom::ElementPart> parts ) {
		updater->RemoveElementParts( parts );
	} );
}

auto tatzel::ClientConnection::Close() noexcept -> void
{
	closed = true;
	page_ready = false;
	dispatcher_owner->Close();
	client_updater->Close();
}

auto tatzel::ClientConnection::IsOnCurrentLoop() const noexcept -> bool
{
	return owner_thread == std::this_thread::get_id();
}

auto tatzel::ClientConnection::InvokeDispatched(
	std::move_only_function<void()>& callback,
	std::source_location origin
) noexcept -> void
{
	try
	{
		auto updates = client_updater->BeginBatch();
		auto dispatch = element_tree->root.BeginDispatch();
		auto boundary = CallbackScope{ "Dispatched callback" };
		callback();
	}
	catch( ... )
	{
		LogException( std::current_exception(), { page ? page->route_path : "", "dispatch", {}, {}, origin } );
		try { client_updater->SendError( "handler_failed", "A background page operation failed. You can continue or retry." ); }
		catch( ... )
		{
			LogException( std::current_exception(), { page ? page->route_path : "", "dispatch_error_response" } );
			Close();
			try { ws_connection->forceClose(); } catch( ... ) {}
		}
	}
}
