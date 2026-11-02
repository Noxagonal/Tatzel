#include <cassert>
#include <functional>
#include <json/json.h>
#include <memory>
#include <string>
#include <vector>

import UI.ClientUpdater;
import UI.ClientElementTree;
import UI.DOM.ElementPart;
import UI.Element.TextInput;
import UI.Element.Checkbox;
import UI.Element.Select;
import UI.Element.Form;
import UI.Diagnostics;

/// @brief Verifies ordered bounded update frames and logical edit identities.
/// @return Zero when batch and revision invariants hold.
/// @pre Assertions are enabled and the framework is linked.
/// @post Temporary senders and logical controls are destroyed.
auto main() -> int
{
	auto frames = std::vector<Json::Value>{};
	auto updater = tatzel::ClientUpdater{ [ &frames ]( const Json::Value& frame ) { frames.push_back( frame ); } };
	{
		auto outer = updater.BeginBatch();
		updater.SetText( "heading", "first" );
		{
			auto inner = updater.BeginBatch();
			updater.SetText( "heading", "second" );
		}
		assert( frames.empty() );
	}
	assert( frames.size() == 1 && updater.GetLastSequence() == 1 );
	assert( frames[ 0 ][ "op" ] == "update_batch" && frames[ 0 ][ "data" ][ "operations" ].size() == 2 );
	assert( frames[ 0 ][ "data" ][ "operations" ][ 1 ][ "data" ][ 0 ][ "text" ] == "second" );
	try
	{
		auto scope = updater.BeginBatch();
		updater.SetText( "heading", "before failure" );
		throw tatzel::Exception{ "Expected callback failure" };
	}
	catch( const tatzel::Exception& ) {}
	updater.SendError( "handler_failed", "Retry" );
	assert( frames[ 1 ][ "data" ][ "operations" ][ 0 ][ "data" ][ 0 ][ "text" ] == "before failure" );
	assert( frames[ 2 ][ "op" ] == "error" && frames[ 2 ][ "data" ][ "fatal" ] == false );
	frames.clear();
	{
		auto scope = updater.BeginBatch();
		for( auto index = 0; index < 300; ++index ) updater.SetText( "row", std::to_string( index ) );
	}
	assert( frames.size() == 3 );
	auto expected = 0;
	for( const auto& frame : frames )
	{
		assert( frame[ "data" ][ "operations" ].size() <= tatzel::MaximumBatchOperations );
		for( const auto& operation : frame[ "data" ][ "operations" ] )
			assert( operation[ "data" ][ 0 ][ "text" ].asString() == std::to_string( expected++ ) );
	}
	frames.clear();
	{
		auto scope = updater.BeginBatch();
		for( auto index = 0; index < 5; ++index ) updater.SetText( "large", std::string( 400000, 'x' ) );
	}
	assert( frames.size() == 3 );
	for( const auto& frame : frames )
	{
		auto writer = Json::StreamWriterBuilder{};
		writer[ "indentation" ] = "";
		assert( Json::writeString( writer, frame ).size() <= tatzel::MaximumBatchBytes );
	}
	frames.clear();
	auto parts = std::vector<tatzel::dom::ElementPart>{};
	for( auto index = 0; index < 5; ++index )
	{
		parts.emplace_back( "part" + std::to_string( index ), "root|main", "p" );
		parts.back().SetTextContent( std::string( 400000, 'x' ) );
	}
	{
		auto scope = updater.BeginBatch();
		updater.CreateElementParts( parts );
	}
	assert( frames.size() == 3 );
	expected = 0;
	for( const auto& frame : frames )
		for( const auto& operation : frame[ "data" ][ "operations" ] )
			for( const auto& part : operation[ "data" ] )
				assert( part[ "part_id" ].asString() == "part" + std::to_string( expected++ ) );
	assert( expected == 5 );
	updater.SendReady();
	assert( frames.back()[ "data" ][ "sequence" ].asUInt64() == updater.GetLastSequence() );
	updater.SendError( "page_failed", "Reload", true );
	const auto closed_count = frames.size();
	updater.SetText( "ignored", "closed" );
	assert( frames.size() == closed_count );

	auto oversized = tatzel::ClientUpdater{ [ &frames ]( const Json::Value& frame ) { frames.push_back( frame ); } };
	try
	{
		oversized.SetText( "huge", std::string( tatzel::MaximumBatchBytes, 'x' ) );
		assert( false );
	}
	catch( const tatzel::Exception& ) {}
	oversized.SetText( "ignored", "closed" );
	assert( frames.size() == closed_count );
	auto attempts = 0;
	auto broken = tatzel::ClientUpdater{ [ &attempts ]( const Json::Value& frame ) {
		++attempts;
		throw tatzel::Exception{ "Expected transport failure" };
	} };
	{
		auto scope = broken.BeginBatch();
		broken.SetText( "heading", "cannot send" );
	}
	broken.SetText( "heading", "no retry" );
	assert( attempts == 1 );

	auto tree = tatzel::ClientElementTree{};
	auto input = tatzel::ui::TextInput{ "input", &tree.root, "Name", "before", {} };
	auto other = tatzel::ui::TextInput{ "other", &tree.root, "Other", "before", {} };
	auto token = input.GetEditRevision();
	assert( !other.TrySetValue( "wrong control", token ) );
	assert( input.CheckClientEdit( { 0, 1 }, std::string{ "draft" } ) == tatzel::ui::EditAcceptance::Current );
	input.AcceptClientEdit( { 0, 1 } );
	input.AcceptFieldValue( std::string{ "draft" } );
	assert( !input.TrySetValue( "stale asynchronous result", token ) );
	assert( input.CheckClientEdit( { 0, 0 }, std::string{ "old" } ) == tatzel::ui::EditAcceptance::Stale );
	assert( input.CheckClientEdit( { 0, 1 }, std::string{ "forged" } ) == tatzel::ui::EditAcceptance::Invalid );
	assert( input.CheckClientEdit( { 1, 2 }, std::string{ "future" } ) == tatzel::ui::EditAcceptance::Invalid );
	token = input.GetEditRevision();
	assert( input.TrySetValue( "DRAFT", token ) );
	assert( !input.TrySetValue( "second result", token ) );
	assert( input.GetValueUpdate().client_revision == 1 && !input.GetValueUpdate().replace );
	input.SetValue( "DRAFT", tatzel::ui::ValueUpdatePolicy::Replace );
	assert( input.GetValueUpdate().epoch == 1 && input.GetValueUpdate().replace );
	assert( input.CheckClientEdit( { 0, 99 }, std::string{ "old generation" } ) == tatzel::ui::EditAcceptance::Stale );
	assert( input.GetClientRevision() == 0 );

	auto* form = tree.root.InsertChild( std::make_unique<tatzel::ui::Form>( "form", &tree.root, "Form", std::function<void()>{} ) );
	auto* first = form->InsertChild( std::make_unique<tatzel::ui::TextInput>( "first", form, "First", "a", std::function<void( std::string_view )>{} ) );
	auto* second = form->InsertChild( std::make_unique<tatzel::ui::TextInput>( "second", form, "Second", "b", std::function<void( std::string_view )>{} ) );
	second->SetValue( "reset", tatzel::ui::ValueUpdatePolicy::Replace );
	auto snapshot = std::vector<tatzel::ui::FieldSubmission>{
		{ first, std::string{ "new a" }, tatzel::ui::ClientEdit{ 0, 1 } },
		{ second, std::string{ "old b" }, tatzel::ui::ClientEdit{ 0, 1 } }
	};
	assert( !form->ApplySubmission( snapshot ) && first->GetValue() == "a" );
	snapshot[ 1 ] = { second, std::string{ "new b" }, tatzel::ui::ClientEdit{ 1, 1 } };
	assert( form->ApplySubmission( snapshot ) && first->GetValue() == "new a" && second->GetValue() == "new b" );
	return 0;
}
