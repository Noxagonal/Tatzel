module;

#include <drogon/WebSocketConnection.h>

#include "PCH.hpp"

module UI.ClientConnection;

import UI.ElementAdapter.TatzelElementAdapter;


tatzel::ClientConnection::ClientConnection(
	drogon::WebSocketConnectionPtr ws_connection
) :
	ws_connection( ws_connection )
{
	client_dom_tree = std::make_unique<ClientDOMTree>();
	client_updater = std::make_unique<ClientUpdater>( ws_connection );

	// TODO: Make it possible to use other element adapters in the future.
	element_adapter = std::make_unique<TatzelElementAdapter>();

	page_builder_core = std::make_unique<PageBuilderCore>( *client_dom_tree, *client_updater, *element_adapter );
}
