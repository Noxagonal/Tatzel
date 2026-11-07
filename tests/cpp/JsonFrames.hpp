#pragma once

#include <cassert>
#include <json/json.h>
#include <memory>
#include <string>
#include <string_view>

/// @brief Decodes a frame emitted through the backend-independent sender interface.
/// @param text Complete borrowed JSON frame.
/// @return Owned protocol frame for structural assertions.
/// @pre The framework has emitted a valid JSON document.
/// @post No borrowed input storage is retained.
inline auto ParseFrame( std::string_view text ) -> Json::Value
{
	auto builder = Json::CharReaderBuilder{};
	auto reader = std::unique_ptr<Json::CharReader>{ builder.newCharReader() };
	auto frame = Json::Value{};
	auto errors = std::string{};
	const auto parsed = reader->parse( text.data(), text.data() + text.size(), &frame, &errors );
	assert( parsed && errors.empty() );
	return frame;
}
