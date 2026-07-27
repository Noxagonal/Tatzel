module;

#include <drogon/WebSocketConnection.h>

#include <memory>

export module UI.ClientConnection;

import UI.ClientElementTree;
import UI.ClientUpdater;
import UI.PageBuilderCore;
import UI.Page;
import UI.ElementAdapter.ElementAdapter;


namespace tatzel {


export
class ClientConnection
{
public:
	ClientConnection(
		drogon::WebSocketConnectionPtr ws_connection
	);
	ClientConnection( const ClientConnection& ) = delete;
	ClientConnection( ClientConnection&& ) = default;

	auto operator=( const ClientConnection& ) -> ClientConnection& = delete;
	auto operator=( ClientConnection&& ) -> ClientConnection& = default;

	drogon::WebSocketConnectionPtr ws_connection = nullptr;
	Page* page = nullptr;
	std::unique_ptr<ClientElementTree> client_dom_tree = nullptr;
	std::unique_ptr<ClientUpdater> client_updater = nullptr;
	std::unique_ptr<ElementAdapter> element_adapter = nullptr;
	std::unique_ptr<PageBuilderCore> page_builder_core = nullptr;
};


}
