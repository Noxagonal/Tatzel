/// @file
/// @brief File and identity helpers without backend types.
module;

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

/// @brief Generates an ID for a logical control.
/// @return Owned UUID string.
/// @pre Drogon's UUID helper is available.
/// @post Existing identities and logical state are unchanged.
export auto GenerateUUID() -> std::string;

}
