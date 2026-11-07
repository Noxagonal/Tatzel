/// @file
/// @brief Private JsonCpp parsing and wire serialization helpers.
module;

#include <json/json.h>
#include <string>
#include <string_view>

export module UI.Internal.JSON;

namespace tatzel {

/// @brief Parses a complete JSON document.
/// @param text UTF-8 JSON text.
/// @return Parsed JSON value.
/// @pre text is a JSON document.
/// @post The result owns its JSON values; input text is unchanged.
/// @throws tatzel::Exception If parsing fails.
export [[gnu::visibility( "hidden" )]] auto ParseJson( std::string_view text ) -> Json::Value;

/// @brief Serializes JSON as compact text.
/// @param json JSON value to serialize.
/// @return Owned compact JSON text.
/// @pre None.
/// @post The input JSON is unchanged.
export [[gnu::visibility( "hidden" )]] auto JsonToString( const Json::Value& json ) -> std::string;

}
