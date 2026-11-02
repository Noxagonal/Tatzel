module;

#include <algorithm>
#include <cassert>
#include <exception>
#include <functional>
#include <cstdint>
#include <json/json.h>
#include <drogon/WebSocketController.h>
#include <json/value.h>
#include <string_view>

#include <string>
#include <utility>

module UI.ClientUpdater;

import UI.Diagnostics;
import UI.Logging;

import UI.DOM.ElementPart;
import UI.DOM.SelectOption;
import UI.Utility;







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
	auto MakeJson_Part = [](
		std::string_view part_id,
		std::string_view attribute,
		std::string_view attribute_value
	) -> Json::Value
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
	auto MakeJson_Part = [](
		std::string_view part_id,
		std::string_view attribute
	) -> Json::Value
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
	auto MakeJson_Part = [](
		std::string_view part_id,
		std::string_view text
	) -> Json::Value
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


auto tatzel::ClientUpdater::SetValue(
	std::string_view part_id,
	std::string_view value,
	ui::ValueUpdate update
) -> void
{
	auto json = Json::Value{ Json::objectValue };
	json[ "op" ] = "set_value";
	auto part = Json::Value{ Json::objectValue };
	part[ "part_id" ] = std::string{ part_id };
	part[ "value" ] = std::string{ value };
	part[ "epoch" ] = Json::UInt64{ update.epoch };
	part[ "base_epoch" ] = Json::UInt64{ update.base_epoch };
	part[ "client_revision" ] = Json::UInt64{ update.client_revision };
	part[ "replace" ] = update.replace;
	json[ "data" ] = Json::Value{ Json::arrayValue };
	json[ "data" ].append( std::move( part ) );
	SendJSON( json );
}


auto tatzel::ClientUpdater::SetModalOpen(
	std::string_view part_id,
	bool open
) -> void
{
	auto MakeJson_Part = [](
		std::string_view part_id,
		bool open
	) -> Json::Value
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
	if( closed ) return;
	if( !json[ "data" ].isArray() || JsonToString( json ).size() + 128 <= MaximumBatchBytes )
	{
		QueueOperation( json );
		return;
	}

	auto operation = json;
	operation[ "data" ] = Json::Value{ Json::arrayValue };
	auto bytes = JsonToString( operation ).size();
	for( const auto& part : json[ "data" ] )
	{
		const auto part_bytes = JsonToString( part ).size() + 1;
		if( bytes + part_bytes + 128 > MaximumBatchBytes && !operation[ "data" ].empty() )
		{
			QueueOperation( operation );
			operation[ "data" ].clear();
			bytes = JsonToString( operation ).size();
		}
		operation[ "data" ].append( part );
		bytes += part_bytes;
	}
	if( !operation[ "data" ].empty() ) QueueOperation( std::move( operation ) );
}


auto tatzel::ClientUpdater::SetSelectOptions(
	std::string_view part_id,
	std::span<const dom::SelectOption> options,
	std::string_view placeholder,
	std::string_view value,
	ui::ValueUpdate update
) -> void
{
	auto part = Json::Value{ Json::objectValue };
	part[ "part_id" ] = std::string{ part_id };
	part[ "placeholder" ] = std::string{ placeholder };
	part[ "value" ] = std::string{ value };
	part[ "epoch" ] = Json::UInt64{ update.epoch };
	part[ "base_epoch" ] = Json::UInt64{ update.base_epoch };
	part[ "client_revision" ] = Json::UInt64{ update.client_revision };
	part[ "replace" ] = update.replace;
	part[ "options" ] = Json::Value{ Json::arrayValue };
	for( const auto& option : options )
	{
		auto item = Json::Value{ Json::objectValue };
		item[ "value" ] = option.value;
		item[ "label" ] = option.label;
		item[ "disabled" ] = option.disabled;
		part[ "options" ].append( std::move( item ) );
	}

	auto json = Json::Value{ Json::objectValue };
	json[ "op" ] = "set_select_options";
	json[ "data" ] = Json::Value{ Json::arrayValue };
	json[ "data" ].append( std::move( part ) );
	SendJSON( json );
}

auto tatzel::ClientUpdater::SetRadioOptions(
	std::string_view part_id,
	std::span<const dom::SelectOption> options,
	std::string_view value,
	ui::ValueUpdate update
) -> void
{
	auto part = Json::Value{ Json::objectValue };
	part[ "part_id" ] = std::string{ part_id };
	part[ "value" ] = std::string{ value };
	part[ "epoch" ] = Json::UInt64{ update.epoch };
	part[ "base_epoch" ] = Json::UInt64{ update.base_epoch };
	part[ "client_revision" ] = Json::UInt64{ update.client_revision };
	part[ "replace" ] = update.replace;
	part[ "options" ] = Json::Value{ Json::arrayValue };
	for( const auto& option : options )
	{
		auto item = Json::Value{ Json::objectValue };
		item[ "value" ] = option.value;
		item[ "label" ] = option.label;
		item[ "disabled" ] = option.disabled;
		part[ "options" ].append( std::move( item ) );
	}

	auto json = Json::Value{ Json::objectValue };
	json[ "op" ] = "set_radio_options";
	json[ "data" ] = Json::Value{ Json::arrayValue };
	json[ "data" ].append( std::move( part ) );
	SendJSON( json );
}


auto tatzel::ClientUpdater::SetLocation( std::string_view hash ) -> void
{
	auto json = Json::Value{ Json::objectValue };
	json[ "op" ] = "set_location";
	json[ "data" ][ "hash" ] = std::string{ hash };
	SendJSON( json );
}


tatzel::ClientUpdater::ClientUpdater( std::function<void( const Json::Value& )> sink ) :
	sink{ std::move( sink ) }
{
	if( !this->sink ) throw Exception{ "An update sender requires a frame sink" };
}

tatzel::ClientUpdateScope::ClientUpdateScope( ClientUpdater& updater ) :
	updater{ &updater }
{
	++updater.batch_depth;
}

tatzel::ClientUpdateScope::~ClientUpdateScope() noexcept { updater->EndBatch(); }

auto tatzel::ClientUpdater::SendFrame( const Json::Value& frame ) -> void
{
	if( sink ) sink( frame );
	else ws_connection->send( JsonToString( frame ) );
}

auto tatzel::ClientUpdater::QueueOperation( Json::Value operation ) -> void
{
	const auto bytes = JsonToString( operation ).size() + 1;
	if( bytes + 128 > MaximumBatchBytes )
	{
		Close();
		if( ws_connection ) ws_connection->forceClose();
		throw Exception{ "One UI operation exceeds the 1 MiB batch limit" };
	}
	if( pending.size() >= MaximumBatchOperations || pending_bytes + bytes > MaximumBatchBytes ) Flush();
	pending.append( std::move( operation ) );
	pending_bytes += bytes;
	if( batch_depth == 0 ) Flush();
}

auto tatzel::ClientUpdater::Flush() -> void
{
	if( closed || pending.empty() ) return;
	if( sequence == MaximumWireRevision ) throw Exception{ "UI batch sequence exhausted" };

	auto frame = Json::Value{ Json::objectValue };
	frame[ "op" ] = "update_batch";
	frame[ "data" ][ "sequence" ] = Json::UInt64{ sequence + 1 };
	frame[ "data" ][ "operations" ] = std::move( pending );
	pending = Json::Value{ Json::arrayValue };
	pending_bytes = 128;

	try
	{
		SendFrame( frame );
		++sequence;
	}
	catch( ... )
	{
		Close();
		if( ws_connection ) ws_connection->forceClose();
		throw;
	}
}

auto tatzel::ClientUpdater::EndBatch() noexcept -> void
{
	assert( batch_depth > 0 );
	--batch_depth;
	if( batch_depth != 0 || closed ) return;
	try { Flush(); }
	catch( ... )
	{
		LogException( std::current_exception(), { {}, "flush_updates" } );
		Close();
		try { if( ws_connection ) ws_connection->forceClose(); }
		catch( ... ) { LogException( std::current_exception(), { {}, "close_failed_transport" } ); }
	}
}

auto tatzel::ClientUpdater::Close() noexcept -> void
{
	closed = true;
	pending.clear();
	pending_bytes = 128;
}

auto tatzel::ClientUpdater::SendError(
	std::string_view code,
	std::string_view message,
	bool fatal
) -> void
{
	if( closed ) return;
	Flush();
	auto frame = Json::Value{ Json::objectValue };
	frame[ "op" ] = "error";
	frame[ "data" ][ "code" ] = std::string{ code };
	frame[ "data" ][ "message" ] = std::string{ message };
	frame[ "data" ][ "fatal" ] = fatal;
	SendFrame( frame );
	if( fatal ) Close();
}

auto tatzel::ClientUpdater::SendReady() -> void
{
	if( closed ) throw Exception{ "Cannot acknowledge a closed page" };
	Flush();
	auto frame = Json::Value{ Json::objectValue };
	frame[ "op" ] = "page_ready";
	frame[ "data" ][ "sequence" ] = Json::UInt64{ sequence };
	SendFrame( frame );
}
