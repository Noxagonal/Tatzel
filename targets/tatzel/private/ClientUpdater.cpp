module;

#include <algorithm>
#include <cassert>
#include <exception>
#include <functional>
#include <cstdint>
#include <json/json.h>
#include <string_view>

#include <string>
#include <utility>

module UI.ClientUpdater;

import UI.Diagnostics;
import UI.Logging;

import UI.DOM.ElementPart;
import UI.DOM.SelectOption;
import UI.Utility;
import UI.Internal.JSON;







/// @brief Owns JSON protocol state behind the installed sender interface.
struct [[gnu::visibility( "hidden" )]] tatzel::ClientUpdater::Implementation
{
	std::function<void( std::string_view )> sink;
	std::function<void()> close_transport;
	Json::Value pending{ Json::arrayValue };
	std::size_t pending_bytes = 128;
	std::size_t batch_depth = 0;
	std::uint64_t sequence = 0;
	bool closed = false;

	/// @brief Serializes a complete control or batch frame.
	/// @param frame Borrowed complete frame.
	/// @pre The sink is usable on the owning loop.
	/// @post The sink has consumed or copied the serialized frame.
	auto SendFrame( const Json::Value& frame ) -> void { sink( JsonToString( frame ) ); }

	/// @brief Partitions oversized array operations before queuing them.
	/// @param updater Owning sender used to flush and close.
	/// @param json Borrowed op/data command.
	/// @pre Called on the owning loop.
	/// @post Operations have been queued or the sender was already closed.
	auto SendJSON(
		ClientUpdater& updater,
		const Json::Value& json
	) -> void;

	/// @brief Adds one complete operation and flushes at protocol limits.
	/// @param updater Owning sender used to flush and close.
	/// @param operation Owned command.
	/// @pre Called on the owning loop.
	/// @post The command is queued or delivered; an oversized command closes the stream.
	/// @throws tatzel::Exception If one command exceeds the byte limit.
	auto QueueOperation(
		ClientUpdater& updater,
		Json::Value operation
	) -> void;
};

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

	implementation->SendJSON( *this, json );
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

	implementation->SendJSON( *this, json );
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

	implementation->SendJSON( *this, json );
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

	implementation->SendJSON( *this, json );
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

	implementation->SendJSON( *this, json );
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
	implementation->SendJSON( *this, json );
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

	implementation->SendJSON( *this, json );
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

	implementation->SendJSON( *this, json );
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

	implementation->SendJSON( *this, json );
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

	implementation->SendJSON( *this, json );
}


auto tatzel::ClientUpdater::Implementation::SendJSON(
	ClientUpdater& updater,
	const Json::Value& json
) -> void
{
	if( closed ) return;
	if( !json[ "data" ].isArray() || JsonToString( json ).size() + 128 <= MaximumBatchBytes )
	{
		QueueOperation( updater, json );
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
			QueueOperation( updater, operation );
			operation[ "data" ].clear();
			bytes = JsonToString( operation ).size();
		}
		operation[ "data" ].append( part );
		bytes += part_bytes;
	}
	if( !operation[ "data" ].empty() ) QueueOperation( updater, std::move( operation ) );
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
	implementation->SendJSON( *this, json );
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
	implementation->SendJSON( *this, json );
}


auto tatzel::ClientUpdater::SetLocation( std::string_view hash ) -> void
{
	auto json = Json::Value{ Json::objectValue };
	json[ "op" ] = "set_location";
	json[ "data" ][ "hash" ] = std::string{ hash };
	implementation->SendJSON( *this, json );
}


tatzel::ClientUpdater::ClientUpdater(
	std::function<void( std::string_view )> sink,
	std::function<void()> close_transport
) :
	implementation{ std::make_unique<Implementation>() }
{
	if( !sink ) throw Exception{ "An update sender requires a frame sink" };
	implementation->sink = std::move( sink );
	implementation->close_transport = std::move( close_transport );
}

tatzel::ClientUpdater::~ClientUpdater() = default;

auto tatzel::ClientUpdater::GetLastSequence() const noexcept -> std::uint64_t { return implementation->sequence; }

tatzel::ClientUpdateScope::ClientUpdateScope( ClientUpdater& updater ) :
	updater{ &updater }
{
	++updater.implementation->batch_depth;
}

tatzel::ClientUpdateScope::~ClientUpdateScope() noexcept { updater->EndBatch(); }

auto tatzel::ClientUpdater::Implementation::QueueOperation(
	ClientUpdater& updater,
	Json::Value operation
) -> void
{
	const auto bytes = JsonToString( operation ).size() + 1;
	if( bytes + 128 > MaximumBatchBytes )
	{
		updater.Close();
		if( close_transport ) close_transport();
		throw Exception{ "One UI operation exceeds the 1 MiB batch limit" };
	}
	if( pending.size() >= MaximumBatchOperations || pending_bytes + bytes > MaximumBatchBytes ) updater.Flush();
	pending.append( std::move( operation ) );
	pending_bytes += bytes;
	if( batch_depth == 0 ) updater.Flush();
}

auto tatzel::ClientUpdater::Flush() -> void
{
	if( implementation->closed || implementation->pending.empty() ) return;
	if( implementation->sequence == MaximumWireRevision ) throw Exception{ "UI batch sequence exhausted" };

	auto frame = Json::Value{ Json::objectValue };
	frame[ "op" ] = "update_batch";
	frame[ "data" ][ "sequence" ] = Json::UInt64{ implementation->sequence + 1 };
	frame[ "data" ][ "operations" ] = std::move( implementation->pending );
	implementation->pending = Json::Value{ Json::arrayValue };
	implementation->pending_bytes = 128;

	try
	{
		implementation->SendFrame( frame );
		++implementation->sequence;
	}
	catch( ... )
	{
		Close();
		if( implementation->close_transport ) implementation->close_transport();
		throw;
	}
}

auto tatzel::ClientUpdater::EndBatch() noexcept -> void
{
	assert( implementation->batch_depth > 0 );
	--implementation->batch_depth;
	if( implementation->batch_depth != 0 || implementation->closed ) return;
	try { Flush(); }
	catch( ... )
	{
		LogException( std::current_exception(), { {}, "flush_updates" } );
		Close();
		try { if( implementation->close_transport ) implementation->close_transport(); }
		catch( ... ) { LogException( std::current_exception(), { {}, "close_failed_transport" } ); }
	}
}

auto tatzel::ClientUpdater::Close() noexcept -> void
{
	implementation->closed = true;
	implementation->pending.clear();
	implementation->pending_bytes = 128;
}

auto tatzel::ClientUpdater::SendError(
	std::string_view code,
	std::string_view message,
	bool fatal
) -> void
{
	if( implementation->closed ) return;
	Flush();
	auto frame = Json::Value{ Json::objectValue };
	frame[ "op" ] = "error";
	frame[ "data" ][ "code" ] = std::string{ code };
	frame[ "data" ][ "message" ] = std::string{ message };
	frame[ "data" ][ "fatal" ] = fatal;
	implementation->SendFrame( frame );
	if( fatal ) Close();
}

auto tatzel::ClientUpdater::SendReady() -> void
{
	if( implementation->closed ) throw Exception{ "Cannot acknowledge a closed page" };
	Flush();
	auto frame = Json::Value{ Json::objectValue };
	frame[ "op" ] = "page_ready";
	frame[ "data" ][ "sequence" ] = Json::UInt64{ implementation->sequence };
	implementation->SendFrame( frame );
}


auto tatzel::ClientUpdater::SetChildOrder(
	std::string_view parent_id,
	std::span<const std::string> child_ids
) -> void
{
	auto operation = Json::Value{ Json::objectValue };
	operation[ "op" ] = "set_child_order";
	operation[ "data" ] = Json::Value{ Json::arrayValue };
	auto part = Json::Value{ Json::objectValue };
	part[ "parent_id" ] = std::string{ parent_id };
	part[ "child_ids" ] = Json::Value{ Json::arrayValue };
	for( const auto& id : child_ids ) part[ "child_ids" ].append( id );
	operation[ "data" ].append( std::move( part ) );
	implementation->SendJSON( *this, operation );
}
