/// @file
/// @brief WebSocket transport and DOM-part-to-logical-event routing.
module;

#include <drogon/WebSocketController.h>
#include <json/json.h>
#include <exception>
#include <memory>
#include <optional>
#include <mutex>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

export module UI.WSController;

import UI.Protocol;
import UI.ClientConnection;
import UI.Element.Form;
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
	/// @post A session is acknowledged, or its failure is reported and its transport closed best effort.
	void handleNewConnection(
		const drogon::HttpRequestPtr& request,
		const drogon::WebSocketConnectionPtr& ws_connection
	) override;

	/// @brief Removes a closed browser session from the registry.
	/// @param ws_connection Closed transport.
	/// @pre Drogon has stopped delivering new events for this connection.
	/// @post The registry releases the session, or an unexpected cleanup failure is reported.
	void handleConnectionClosed( const drogon::WebSocketConnectionPtr& ws_connection ) override;

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
	) override;

	/// @brief Sends a protocol message to a snapshot of all current transports.
	/// @param message Complete serialized server op/data message.
	/// @pre The message is valid for every recipient; callers maintain logical state separately.
	/// @post Each transport in the snapshot has been offered the message.
	static auto Broadcast( const std::string& message ) -> void;

private:
	/// @brief Closes a failed transport without letting a secondary exception escape.
	/// @param transport Affected browser transport.
	/// @pre transport is nonnull and used on its owning event loop.
	/// @post Closure is requested, or its failure is logged with available exception details.
	static auto CloseTransport( const drogon::WebSocketConnectionPtr& transport ) noexcept -> void;

	/// @brief Reports a failure and attempts a generic browser response under a second boundary.
	/// @param transport Affected browser transport.
	/// @param exception Original retained exception, including standard-library exceptions.
	/// @param context Event metadata captured before the failure.
	/// @param code Browser-visible error code.
	/// @param message Generic browser-visible description.
	/// @param fatal Whether this page requires a fresh session.
	/// @param location Report site, distinct from the original throw location.
	/// @pre transport is nonnull; context views remain valid during this call.
	/// @post The failure is logged; response failures are also logged and request transport closure.
	static auto ReportFailure(
		const drogon::WebSocketConnectionPtr& transport,
		std::exception_ptr exception,
		const tatzel::ErrorContext& context,
		std::string_view code,
		std::string_view message,
		bool fatal = false,
		std::source_location location = std::source_location::current()
	) noexcept -> void;

	/// @brief Retains newly created services in the session registry.
	/// @param ws_connection Browser transport to own.
	/// @return Retained stable session handle.
	/// @pre The transport has no existing session.
	/// @post The registry owns the new session.
	static auto CreateClientConnection( const drogon::WebSocketConnectionPtr& ws_connection )
		-> std::shared_ptr<tatzel::ClientConnection>;

	/// @brief Registers one registry-cleanup hook per owning event-loop lifetime.
	/// @pre Called on the connection's event loop before retaining it in the registry.
	/// @post Loop exit closes and releases its sessions outside the registry mutex.
	static auto RegisterLoopCleanup() -> void;

	/// @brief Releases this loop's sessions without invoking capture destructors under the registry lock.
	/// @pre Called on the exiting loop after its dispatcher shutdown gate has closed.
	/// @post This loop's registry entries and their pending callbacks have been released.
	static auto ReleaseLoopConnections() noexcept -> void;

	/// @brief Looks up a session while retaining its lifetime beyond the registry lock.
	/// @param ws_connection Browser transport to find.
	/// @return Stable session handle, or nullptr.
	/// @pre None.
	/// @post Registry state is unchanged.
	static auto FindClientConnection( const drogon::WebSocketConnectionPtr& ws_connection )
		-> std::shared_ptr<tatzel::ClientConnection>;

	/// @brief Sends a structured error using the same envelope as other server messages.
	/// @param ws_connection Recipient transport.
	/// @param code Machine-readable error code.
	/// @param message Human-readable error description.
	/// @param fatal Whether the connection must stop accepting page work.
	/// @pre ws_connection is nonnull.
	/// @post An error op/data message is queued.
	static auto SendError(
		const drogon::WebSocketConnectionPtr& ws_connection,
		std::string_view code,
		std::string_view message,
		bool fatal = false
	) -> void;

	/// @brief Validates browser counters without lossy integer conversion.
	/// @param data Field payload containing epoch and revision.
	/// @return Safe counters, or nullopt for malformed or out-of-range metadata.
	/// @pre None.
	/// @post No logical state changes.
	static auto ReadEdit( const Json::Value& data ) -> std::optional<tatzel::ui::ClientEdit>;

	/// @brief Resolves a complete browser field snapshot before applying any submitted values.
	/// @param connection Owning client page and identity registry.
	/// @param form Logical form receiving the snapshot.
	/// @param data Validated event envelope containing a fields array.
	/// @return Empty on acceptance, stale_form for obsolete generations, or invalid_message for other invalid snapshots.
	/// @pre Called on the owning event loop after the form's source marker is checked.
	/// @post Invalid snapshots change no fields; valid snapshots update editable values without edit callbacks.
	static auto ApplyFormSubmission(
		tatzel::ClientConnection& connection,
		tatzel::ui::Form& form,
		const Json::Value& data
	) -> std::string_view;

	/// @brief Handles a validated envelope using logical IDs only after resolving DOM parts.
	/// @param connection Owning client session.
	/// @param json Message with an op string and data object.
	/// @pre Called on the session's owning event loop.
	/// @post A page is built once or an event reaches the logical owner of its DOM part.
	static auto DispatchMessage(
		tatzel::ClientConnection& connection,
		const Json::Value& json
	) -> void;

	static inline std::mutex mutex;
	static inline std::vector<std::shared_ptr<tatzel::ClientConnection>> connections;

public:
	WS_PATH_LIST_BEGIN
		WS_PATH_ADD( "/ws" );
	WS_PATH_LIST_END
};
