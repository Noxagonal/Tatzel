/** @file @brief Services and logical state owned by a single WebSocket session. */
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

/**
 * @brief Owns the logical tree and framework services for one browser session.
 * @details Registry lookups retain shared ownership of this object. Members are
 * destroyed so the logical tree disappears before its adapter and DOM sender.
 */
export class ClientConnection
{
public:
	/**
	 * @brief Creates a client-owned logical tree, adapter, sender, and builder context.
	 * @param ws_connection Live browser WebSocket.
	 * @pre ws_connection is nonnull.
	 * @post The session has an empty logical root and is ready for one connect message.
	 */
	explicit ClientConnection( drogon::WebSocketConnectionPtr ws_connection );

	/** @brief Browser transport retained for the session lifetime. */
	drogon::WebSocketConnectionPtr ws_connection;
	/** @brief Registered page after the first successful connect message, otherwise nullptr. */
	Page* page = nullptr;
	/** @brief DOM protocol sender; outlives the adapter and logical controls. */
	std::unique_ptr<ClientUpdater> client_updater;
	/** @brief Mapping services; outlive the logical controls they bind. */
	std::unique_ptr<ElementAdapter> element_adapter;
	/** @brief Logical ownership tree; DOM part state is internal to its controls. */
	std::unique_ptr<ClientElementTree> element_tree;
	/** @brief Borrowed framework context; destroyed before its services. */
	std::unique_ptr<PageBuilderCore> page_builder_core;
};

}
