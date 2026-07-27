module;

#include <drogon/WebSocketController.h>
#include <json/json.h>

#include <string_view>
#include <span>

export module UI.ClientUpdater;

import UI.DOM.ElementPart;


namespace tatzel {


export
class ClientUpdater
{
public:
	ClientUpdater(
		drogon::WebSocketConnectionPtr ws_connection
	);
	ClientUpdater( const ClientUpdater& ) = delete;
	ClientUpdater( ClientUpdater&& ) = default;
	~ClientUpdater() = default;

	auto operator=( const ClientUpdater& ) -> ClientUpdater& = delete;
	auto operator=( ClientUpdater&& ) -> ClientUpdater& = default;

	auto CreateElementParts(
		std::span<const dom::ElementPart> parts
	) -> void;

	auto RemoveElementParts(
		std::span<const dom::ElementPart> parts
	) -> void;

	auto SetAttribute(
		std::string_view part_id,
		std::string_view attribute,
		std::string_view attribute_value
	) -> void;

	auto RemoveAttribute(
		std::string_view part_id,
		std::string_view attribute
	) -> void;

	auto SetText(
		std::string_view part_id,
		std::string_view text
	) -> void;

	auto SetModalOpen(
		std::string_view part_id,
		bool open
	) -> void;

	auto SetOnClick(
		std::string_view part_id
	) -> void;

	auto SetOnChange(
		std::string_view part_id
	) -> void;

	auto SetOnSubmit(
		std::string_view part_id
	) -> void;

private:

	auto CreateJSONOp( std::string_view op ) -> Json::Value;
	auto SendJSON( const Json::Value& json ) -> void;

	drogon::WebSocketConnectionPtr ws_connection;
};


}