module;

#include <drogon/utils/Utilities.h>
#include <json/json.h>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <cstdint>
#include <sstream>
#include <stdexcept>

module UI.Utility;

auto tatzel::GetTemplateDocument( const std::filesystem::path& path ) -> std::string
{
	if( !std::filesystem::is_regular_file( path ) ) throw std::runtime_error{ "Template file does not exist" };
	auto file = std::ifstream{ path, std::ios::binary | std::ios::ate };
	if( !file ) throw std::runtime_error{ "Could not open template file" };
	auto length = file.tellg();
	if( length <= 0 ) throw std::runtime_error{ "Template file is empty or unreadable" };
	file.seekg( 0 );
	auto result = std::string( static_cast<size_t>( length ), '\0' );
	if( !file.read( result.data(), static_cast<std::streamsize>( result.size() ) ) )
		throw std::runtime_error{ "Could not read complete template file" };
	return result;
}

auto tatzel::ParseJson( std::string_view text ) -> Json::Value
{
	Json::CharReaderBuilder builder;
	Json::Value result;
	std::string errors;
	std::istringstream stream{ std::string{ text } };
	if( !Json::parseFromStream( builder, stream, &result, &errors ) )
		throw std::runtime_error{ "Failed to parse JSON string" };
	return result;
}

auto tatzel::JsonToString( const Json::Value& json ) -> std::string
{
	Json::StreamWriterBuilder writer;
	writer[ "indentation" ] = "";
	return Json::writeString( writer, json );
}

auto tatzel::GenerateUUID() -> std::string
{
	return drogon::utils::getUuid();
}
