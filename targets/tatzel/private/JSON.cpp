module;

#include <json/json.h>
#include <sstream>
#include <string>
#include <string_view>

module UI.Internal.JSON;

import UI.Diagnostics;

auto tatzel::ParseJson( std::string_view text ) -> Json::Value
{
	Json::CharReaderBuilder builder;
	Json::Value result;
	std::string errors;
	std::istringstream stream{ std::string{ text } };
	if( !Json::parseFromStream( builder, stream, &result, &errors ) )
		throw tatzel::Exception{ "Failed to parse JSON string" };
	return result;
}

auto tatzel::JsonToString( const Json::Value& json ) -> std::string
{
	Json::StreamWriterBuilder writer;
	writer[ "indentation" ] = "";
	return Json::writeString( writer, json );
}
