/// @file
/// @brief Internal file, JSON, and identity helpers.
module;

#include <json/json.h>
#include <filesystem>
#include <string>
#include <string_view>

export module UI.Utility;

namespace tatzel {

/// @brief Reads an HTML template into an owned string.
/// @param path Runtime path to the template.
/// @return Complete template bytes.
/// @pre path identifies a nonempty readable regular file.
/// @post The file is closed and its contents are owned by the caller.
/// @throws tatzel::Exception If the file cannot be read completely.
export auto GetTemplateDocument( const std::filesystem::path& path ) -> std::string;

/// @brief Parses a complete JSON document.
/// @param text UTF-8 JSON text.
/// @return Parsed JSON value.
/// @pre text is a JSON document.
/// @post The result owns its JSON values; input text is unchanged.
/// @throws tatzel::Exception If parsing fails.
export auto ParseJson( std::string_view text ) -> Json::Value;

/// @brief Serializes JSON as compact text.
/// @param json JSON value to serialize.
/// @return Owned compact JSON text.
/// @pre None.
/// @post The input JSON is unchanged.
export auto JsonToString( const Json::Value& json ) -> std::string;

/// @brief Generates an ID for a logical control.
/// @return Owned UUID string.
/// @pre Drogon's UUID helper is available.
/// @post Existing identities and logical state are unchanged.
export auto GenerateUUID() -> std::string;

}
