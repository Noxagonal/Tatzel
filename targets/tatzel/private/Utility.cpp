module;

#include <drogon/utils/Utilities.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <cstddef>

module UI.Utility;

import UI.Diagnostics;

auto tatzel::GetTemplateDocument( const std::filesystem::path& path ) -> std::string
{
	if( !std::filesystem::is_regular_file( path ) ) throw tatzel::Exception{ "Template file does not exist" };
	auto file = std::ifstream{ path, std::ios::binary | std::ios::ate };
	if( !file ) throw tatzel::Exception{ "Could not open template file" };
	auto length = file.tellg();
	if( length <= 0 ) throw tatzel::Exception{ "Template file is empty or unreadable" };
	file.seekg( 0 );
	auto result = std::string( static_cast<std::size_t>( length ), '\0' );
	if( !file.read( result.data(), static_cast<std::streamsize>( result.size() ) ) )
		throw tatzel::Exception{ "Could not read complete template file" };
	return result;
}

auto tatzel::GenerateUUID() -> std::string
{
	return drogon::utils::getUuid();
}
