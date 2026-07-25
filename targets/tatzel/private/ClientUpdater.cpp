module;

#include <algorithm>
#include <json/json.h>
#include <drogon/WebSocketController.h>
#include <json/value.h>

#include "PCH.hpp"

module UI.ClientUpdater;

import UI.DOM.ElementPart;
import UI.Utility;


tatzel::ClientUpdater::ClientUpdater(
	drogon::WebSocketConnectionPtr ws_connection
) :
	ws_connection( ws_connection )
{}


auto tatzel::ClientUpdater::CreateElement(
	std::span<const dom::ElementPart> parts
) -> void
{
	auto json = Json::Value{ Json::ValueType::objectValue };
	json[ "op" ] = "create_element";

	auto json_part_list = Json::Value{ Json::ValueType::arrayValue };
	for( auto& part : parts )
	{
		auto json_part_obj = Json::Value{ Json::ValueType::objectValue };
		json_part_obj[ "part_id" ] = std::string{ part.GetPartID() };
		json_part_obj[ "parent_id" ] = std::string{ part.GetParentID() };
		json_part_obj[ "tag" ] = std::string{ part.GetTag() };
		json_part_obj[ "text_content" ] = std::string{ part.GetTextContent() };

		auto json_part_obj_classes = Json::Value{ Json::ValueType::arrayValue };
		for( auto& element_class : part.GetClasses() )
		{
			json_part_obj_classes.append( element_class );
		}
		json_part_obj[ "classes" ] = std::move( json_part_obj_classes );

		auto json_part_obj_attributes = Json::Value{ Json::ValueType::objectValue };
		for( auto& element_attribute : part.GetAttributes() )
		{
			json_part_obj_attributes[ std::string{ element_attribute.first } ] = std::string{ element_attribute.second };
		}
		json_part_obj[ "attributes" ] = std::move( json_part_obj_attributes );

		json_part_list.append( std::move( json_part_obj ) );
	}

	json[ "parts" ] = std::move( json_part_list );

	SendJSON( json );
}


auto tatzel::ClientUpdater::RemoveElement(
	std::string_view id
) -> void
{
	SendJSON( CreateJSONOp( "remove_element", id ) );
}


auto tatzel::ClientUpdater::SetAttribute(
	std::string_view id,
	std::string_view part_name,
	std::string_view attribute,
	std::string_view attribute_value
) -> void
{
	auto json = CreateJSONOp( "set_attribute", id );
	json[ "attribute" ] = std::string{ attribute };
	json[ "attribute_value" ] = std::string{ attribute_value };
	SendJSON( json );
}


auto tatzel::ClientUpdater::RemoveAttribute(
	std::string_view id,
	std::string_view part_name,
	std::string_view attribute
) -> void
{
	auto json = CreateJSONOp( "remove_attribute", id );
	json[ "attribute" ] = std::string{ attribute };
	SendJSON( json );
}


auto tatzel::ClientUpdater::SetText(
	std::string_view id,
	std::string_view part_name,
	std::string_view text
) -> void
{
	auto json = CreateJSONOp( "set_text", id );
	json[ "text" ] = std::string{ text };
	SendJSON( json );
}


auto tatzel::ClientUpdater::SetModalOpen(
	std::string_view id,
	std::string_view part_name,
	bool open
) -> void
{
	auto json = CreateJSONOp( "set_modal_open", id );
	json[ "open" ] = open;
	SendJSON( json );
}


auto tatzel::ClientUpdater::SetOnClick(
	std::string_view id,
	std::string_view part_name
) -> void
{
	SendJSON( CreateJSONOp( "set_on_click", id ) );
}


auto tatzel::ClientUpdater::SetOnChange(
	std::string_view id,
	std::string_view part_name
) -> void
{
	SendJSON( CreateJSONOp( "set_on_change", id ) );
}


auto tatzel::ClientUpdater::SetOnSubmit(
	std::string_view id,
	std::string_view part_name
) -> void
{
	SendJSON( CreateJSONOp( "set_on_submit", id ) );
}


auto tatzel::ClientUpdater::CreateJSONOp(
	std::string_view op,
	std::string_view id
) -> Json::Value
{
	auto json = Json::Value{ Json::ValueType::objectValue };
	json[ "op" ] = std::string{ op };
	json[ "id" ] = std::string{ id };
	return json;
}


auto tatzel::ClientUpdater::SendJSON(
	const Json::Value& json
) -> void
{
	ws_connection->send( tatzel::JsonToString( json ) );
}