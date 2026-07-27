module;

#include <algorithm>
#include <json/json.h>
#include <drogon/WebSocketController.h>
#include <json/value.h>
#include <string_view>

#include "PCH.hpp"

module UI.ClientUpdater;

import UI.DOM.ElementPart;
import UI.Utility;



// TODO: Implement send queue for batching the data sent to the client.
// The data is already organized so that the elements can be batched later.



tatzel::ClientUpdater::ClientUpdater(
	drogon::WebSocketConnectionPtr ws_connection
) :
	ws_connection( ws_connection )
{}


auto tatzel::ClientUpdater::CreateElementParts(
	std::span<const dom::ElementPart> parts
) -> void
{
	auto MakeJson_Part = []( const dom::ElementPart& part ) -> Json::Value
	{
		auto json_part = Json::Value{ Json::ValueType::objectValue };
		json_part[ "part_id" ] = std::string{ part.GetPartID() };
		json_part[ "parent_id" ] = std::string{ part.GetParentID() };
		json_part[ "tag" ] = std::string{ part.GetTag() };
		json_part[ "text_content" ] = std::string{ part.GetTextContent() };

		auto json_part_classes = Json::Value{ Json::ValueType::arrayValue };
		for( auto& element_class : part.GetClasses() )
		{
			json_part_classes.append( element_class );
		}
		json_part[ "classes" ] = std::move( json_part_classes );

		auto json_part_attributes = Json::Value{ Json::ValueType::objectValue };
		for( auto& element_attribute : part.GetAttributes() )
		{
			json_part_attributes[ std::string{ element_attribute.first } ] = std::string{ element_attribute.second };
		}
		json_part[ "attributes" ] = std::move( json_part_attributes );

		return json_part;
	};

	auto MakeJson_Data = [ & ] -> Json::Value
	{
		auto json_data = Json::Value{ Json::ValueType::arrayValue };

		for( const auto& part : parts )
		{
			json_data.append( MakeJson_Part( part ) );
		}

		return json_data;
	};

	auto json = Json::Value{ Json::ValueType::objectValue };
	json[ "op" ] = "create_element_part";
	json[ "data" ] = MakeJson_Data();

	SendJSON( json );
}


auto tatzel::ClientUpdater::RemoveElementParts(
	std::span<const dom::ElementPart> parts
) -> void
{
	auto MakeJson_Part = []( const dom::ElementPart& part ) -> Json::Value
	{
		auto json_part = Json::Value{ Json::ValueType::objectValue };
		json_part[ "part_id" ] = std::string{ part.GetPartID() };
		return json_part;
	};

	auto MakeJson_Data = [ & ] -> Json::Value
	{
		auto json_part_data = Json::Value{ Json::ValueType::arrayValue };

		for( const auto& part : parts )
		{
			json_part_data.append( MakeJson_Part( part ) );
		}

		return json_part_data;
	};

	auto json = Json::Value{ Json::ValueType::objectValue };
	json[ "op" ] = "remove_element_part";
	json[ "data" ] = MakeJson_Data();

	SendJSON( json );
}


auto tatzel::ClientUpdater::SetAttribute(
	std::string_view part_id,
	std::string_view attribute,
	std::string_view attribute_value
) -> void
{
	auto MakeJson_Part = []( std::string_view part_id, std::string_view attribute, std::string_view attribute_value ) -> Json::Value
	{
		auto json_part = Json::Value{ Json::ValueType::objectValue };
		json_part[ "part_id" ] = std::string{ part_id };
		json_part[ "attribute" ] = std::string{ attribute };
		json_part[ "attribute_value" ] = std::string{ attribute_value };
		return json_part;
	};

	auto MakeJson_Data = [ & ] -> Json::Value
	{
		auto json_part_data = Json::Value{ Json::ValueType::arrayValue };
		json_part_data.append( MakeJson_Part( part_id, attribute, attribute_value ) );
		return json_part_data;
	};

	auto json = Json::Value{ Json::ValueType::objectValue };
	json[ "op" ] = "set_attribute";
	json[ "data" ] = MakeJson_Data();

	SendJSON( json );
}


auto tatzel::ClientUpdater::RemoveAttribute(
	std::string_view part_id,
	std::string_view attribute
) -> void
{
	auto MakeJson_Part = []( std::string_view part_id, std::string_view attribute ) -> Json::Value
	{
		auto json_part = Json::Value{ Json::ValueType::objectValue };
		json_part[ "part_id" ] = std::string{ part_id };
		json_part[ "attribute" ] = std::string{ attribute };
		return json_part;
	};

	auto MakeJson_Data = [ & ] -> Json::Value
	{
		auto json_part_data = Json::Value{ Json::ValueType::arrayValue };
		json_part_data.append( MakeJson_Part( part_id, attribute ) );
		return json_part_data;
	};

	auto json = Json::Value{ Json::ValueType::objectValue };
	json[ "op" ] = "remove_attribute";
	json[ "data" ] = MakeJson_Data();

	SendJSON( json );
}


auto tatzel::ClientUpdater::SetText(
	std::string_view part_id,
	std::string_view text
) -> void
{
	auto MakeJson_Part = []( std::string_view part_id, std::string_view text ) -> Json::Value
	{
		auto json_part = Json::Value{ Json::ValueType::objectValue };
		json_part[ "part_id" ] = std::string{ part_id };
		json_part[ "text" ] = std::string{ text };
		return json_part;
	};

	auto MakeJson_Data = [ & ] -> Json::Value
	{
		auto json_part_data = Json::Value{ Json::ValueType::arrayValue };
		json_part_data.append( MakeJson_Part( part_id, text ) );
		return json_part_data;
	};

	auto json = Json::Value{ Json::ValueType::objectValue };
	json[ "op" ] = "set_text";
	json[ "data" ] = MakeJson_Data();

	SendJSON( json );
}


auto tatzel::ClientUpdater::SetModalOpen(
	std::string_view part_id,
	bool open
) -> void
{
	auto MakeJson_Part = []( std::string_view part_id, bool open ) -> Json::Value
	{
		auto json_part = Json::Value{ Json::ValueType::objectValue };
		json_part[ "part_id" ] = std::string{ part_id };
		json_part[ "open" ] = open;
		return json_part;
	};

	auto MakeJson_Data = [ & ] -> Json::Value
	{
		auto json_part_data = Json::Value{ Json::ValueType::arrayValue };
		json_part_data.append( MakeJson_Part( part_id, open ) );
		return json_part_data;
	};

	auto json = Json::Value{ Json::ValueType::objectValue };
	json[ "op" ] = "set_modal_open";
	json[ "data" ] = MakeJson_Data();

	SendJSON( json );
}


auto tatzel::ClientUpdater::SetOnClick(
	std::string_view part_id
) -> void
{
	auto MakeJson_Part = []( std::string_view part_id ) -> Json::Value
	{
		auto json_part = Json::Value{ Json::ValueType::objectValue };
		json_part[ "part_id" ] = std::string{ part_id };
		return json_part;
	};

	auto MakeJson_Data = [ & ] -> Json::Value
	{
		auto json_part_data = Json::Value{ Json::ValueType::arrayValue };
		json_part_data.append( MakeJson_Part( part_id ) );
		return json_part_data;
	};

	auto json = Json::Value{ Json::ValueType::objectValue };
	json[ "op" ] = "set_on_click";
	json[ "data" ] = MakeJson_Data();

	SendJSON( json );
}


auto tatzel::ClientUpdater::SetOnChange(
	std::string_view part_id
) -> void
{
	auto MakeJson_Part = []( std::string_view part_id ) -> Json::Value
	{
		auto json_part = Json::Value{ Json::ValueType::objectValue };
		json_part[ "part_id" ] = std::string{ part_id };
		return json_part;
	};

	auto MakeJson_Data = [ & ] -> Json::Value
	{
		auto json_part_data = Json::Value{ Json::ValueType::arrayValue };
		json_part_data.append( MakeJson_Part( part_id ) );
		return json_part_data;
	};

	auto json = Json::Value{ Json::ValueType::objectValue };
	json[ "op" ] = "set_on_change";
	json[ "data" ] = MakeJson_Data();

	SendJSON( json );
}


auto tatzel::ClientUpdater::SetOnSubmit(
	std::string_view part_id
) -> void
{
	auto MakeJson_Part = []( std::string_view part_id ) -> Json::Value
	{
		auto json_part = Json::Value{ Json::ValueType::objectValue };
		json_part[ "part_id" ] = std::string{ part_id };
		return json_part;
	};

	auto MakeJson_Data = [ & ] -> Json::Value
	{
		auto json_part_data = Json::Value{ Json::ValueType::arrayValue };
		json_part_data.append( MakeJson_Part( part_id ) );
		return json_part_data;
	};

	auto json = Json::Value{ Json::ValueType::objectValue };
	json[ "op" ] = "set_on_submit";
	json[ "data" ] = MakeJson_Data();

	SendJSON( json );
}


auto tatzel::ClientUpdater::SendJSON(
	const Json::Value& json
) -> void
{
	ws_connection->send( tatzel::JsonToString( json ) );
}