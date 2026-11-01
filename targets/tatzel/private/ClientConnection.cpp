module;

#include <drogon/WebSocketConnection.h>
#include <cassert>
#include <memory>
#include <span>
#include <utility>

module UI.ClientConnection;

import UI.ClientElementTree;
import UI.ClientUpdater;
import UI.PageBuilderCore;
import UI.DOM.ElementPart;

import UI.ElementAdapter.TatzelElementAdapter;

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
	page_builder_core = std::make_unique<PageBuilderCore>( *element_tree, *client_updater, *element_adapter );
}
