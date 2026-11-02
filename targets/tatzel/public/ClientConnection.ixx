/// @file
/// @brief Services and logical state owned by a single WebSocket session.
module;

#include <drogon/WebSocketConnection.h>
#include <memory>
#include <functional>
#include <source_location>
#include <thread>

export module UI.ClientConnection;

import UI.ClientElementTree;
import UI.ClientUpdater;
import UI.PageBuilderCore;
import UI.Page;
import UI.ElementAdapter.ElementAdapter;
import UI.Dispatcher;

namespace tatzel {

/// @brief Owns the logical tree and framework services for one browser session.
/// @details Registry lookups retain shared ownership of this object. Members are
/// destroyed so the logical tree disappears before its adapter and DOM sender.
export class ClientConnection
{
public:
	/// @brief Creates shared session ownership and binds its weak dispatcher invocation boundary.
	/// @param ws_connection Live browser WebSocket.
	/// @return Shared session with an empty logical root and a working dispatcher.
	/// @pre ws_connection is nonnull and the caller is on its owning event loop.
	/// @post The session is ready for one connect message; queued work retains it only while executing.
	/// @throws tatzel::Exception If the caller has no owning event loop or the transport is null.
	static auto Create( drogon::WebSocketConnectionPtr ws_connection ) -> std::shared_ptr<ClientConnection>;

	/// @brief Closes dispatch submission and cancels pending callbacks for this session.
	/// @pre Called on the owning event loop before registry ownership is released.
	/// @post No further queued callback begins for this page.
	auto Close() noexcept -> void;

	/// @brief Tests whether this session belongs to the calling event-loop thread.
	/// @return Whether the caller is the session's owning thread, even after dispatcher closure.
	/// @pre Called on an event-loop thread.
	/// @post Session state is unchanged.
	auto IsOnCurrentLoop() const noexcept -> bool;

	/// @brief Browser transport retained for the session lifetime.
	drogon::WebSocketConnectionPtr ws_connection;

	/// @brief Registered page after the first successful connect message, otherwise nullptr.
	Page* page = nullptr;

	/// @brief Whether the page factory and initial navigation completed successfully.
	bool page_built = false;

	/// @brief Whether the initial logical updates were flushed and readiness acknowledged.
	bool page_ready = false;

	/// @brief Whether the session has stopped accepting application work.
	bool closed = false;

	/// @brief DOM protocol sender; outlives the adapter and logical controls.
	std::unique_ptr<ClientUpdater> client_updater;

	/// @brief Mapping services; outlive the logical controls they bind.
	std::unique_ptr<ElementAdapter> element_adapter;

	/// @brief Logical ownership tree; DOM part state is internal to its controls.
	std::unique_ptr<ClientElementTree> element_tree;

	/// @brief Borrowed framework context; destroyed before its services.
	std::unique_ptr<PageBuilderCore> page_builder_core;

private:
	/// @brief Allocates the logical tree, adapter, and DOM sender before dispatcher initialization.
	/// @param ws_connection Nonnull browser transport.
	/// @pre Called by Create on the transport's owning event loop.
	/// @post Logical services are ready; Create completes the dispatcher and builder context.
	explicit ClientConnection( drogon::WebSocketConnectionPtr ws_connection );

	/// @brief Runs posted application work while retaining normal logical removal and trace boundaries.
	/// @param callback Application callback to invoke synchronously.
	/// @param origin Submission site included in a failure report.
	/// @pre Called on the owning loop with shared session ownership retained by the invoker.
	/// @post The callback completes or its failure is logged; retired controls have been released.
	auto InvokeDispatched(
		std::move_only_function<void()>& callback,
		std::source_location origin
	) noexcept -> void;

	const std::thread::id owner_thread = std::this_thread::get_id();
	std::unique_ptr<DispatcherOwner> dispatcher_owner;
};

}
