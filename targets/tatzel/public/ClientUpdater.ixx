/**
 * @file
 * @brief Internal sender for concrete browser DOM operations.
 */
module;

#include <drogon/WebSocketConnection.h>
#include <json/json.h>
#include <span>
#include <string_view>

export module UI.ClientUpdater;

import UI.DOM.ElementPart;

namespace tatzel {

/** @brief Serializes concrete DOM operations; all IDs accepted here are DOM part IDs. */
export class ClientUpdater
{
public:
	/**
	 * @brief Binds the sender to one browser transport.
	 * @param ws_connection Live browser WebSocket.
	 * @pre ws_connection is nonnull and belongs to the owning client page.
	 * @post Subsequent DOM operations use that transport.
	 */
	explicit ClientUpdater( drogon::WebSocketConnectionPtr ws_connection );

	/**
	 * @brief Queues creation of concrete DOM nodes.
	 * @param parts Parts ordered so parents precede their children.
	 * @pre Parent DOM nodes already exist or appear earlier in parts.
	 * @post One create_element_part message contains copied initial DOM state.
	 */
	auto CreateElementParts( std::span<const dom::ElementPart> parts ) -> void;

	/**
	 * @brief Queues removal of concrete DOM nodes.
	 * @param parts Concrete DOM parts to remove.
	 * @pre The caller separately manages logical ownership.
	 * @post One remove_element_part message is queued; logical ownership is unchanged.
	 */
	auto RemoveElementParts( std::span<const dom::ElementPart> parts ) -> void;

	/**
	 * @brief Queues an HTML attribute update.
	 * @param part_id Target DOM node ID.
	 * @param attribute HTML attribute name.
	 * @param attribute_value New HTML attribute value.
	 * @pre The target exists and attribute is not an inline event-handler attribute.
	 * @post A set_attribute message is queued.
	 */
	auto SetAttribute( std::string_view part_id, std::string_view attribute, std::string_view attribute_value ) -> void;

	/**
	 * @brief Queues removal of an HTML attribute.
	 * @param part_id Target DOM node ID.
	 * @param attribute HTML attribute name.
	 * @pre The target exists.
	 * @post A remove_attribute message is queued.
	 */
	auto RemoveAttribute( std::string_view part_id, std::string_view attribute ) -> void;

	/**
	 * @brief Queues replacement of a DOM node's text content.
	 * @param part_id Target DOM node ID.
	 * @param text New plain text content.
	 * @pre The target exists and replacing its descendants is intended.
	 * @post A set_text message is queued.
	 */
	auto SetText( std::string_view part_id, std::string_view text ) -> void;

	/**
	 * @brief Queues a concrete HTML dialog state change.
	 * @param part_id DOM ID of an HTML dialog.
	 * @param open Whether the dialog should be open.
	 * @pre The target is an existing HTML dialog.
	 * @post A set_modal_open message is queued; no logical modal API is implied.
	 */
	auto SetModalOpen( std::string_view part_id, bool open ) -> void;

	/**
	 * @brief Marks a DOM node as a logical click event source.
	 * @param part_id Target DOM node ID.
	 * @pre The target exists and its logical owner accepts click events.
	 * @post A set_on_click message is queued.
	 */
	auto SetOnClick( std::string_view part_id ) -> void;

	/**
	 * @brief Marks a DOM node as a logical change event source.
	 * @param part_id Target DOM node ID.
	 * @pre The target exists and its logical owner accepts change events.
	 * @post A set_on_change message is queued.
	 */
	auto SetOnChange( std::string_view part_id ) -> void;

	/**
	 * @brief Marks a DOM node as a logical submit event source.
	 * @param part_id Target DOM node ID.
	 * @pre The target exists and its logical owner accepts submit events.
	 * @post A set_on_submit message is queued.
	 */
	auto SetOnSubmit( std::string_view part_id ) -> void;

private:
	/**
	 * @brief Serializes and queues a JSON message on the bound transport.
	 * @param json Complete op/data message.
	 * @pre The transport is alive; json matches the documented wire protocol.
	 * @post An owned serialized message has been passed to Drogon.
	 */
	auto SendJSON( const Json::Value& json ) -> void;

	drogon::WebSocketConnectionPtr ws_connection;
};

}
