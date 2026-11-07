module;

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

module UI.DOM.ElementPart;

namespace tatzel::dom {

auto ElementPart::GetAttribute( std::string_view attribute ) const -> std::optional<std::string_view>
{
	auto found = std::ranges::find_if( attributes, [ attribute ]( const auto& entry ) { return entry.first == attribute; } );
	if( found == attributes.end() ) return std::nullopt;
	return found->second;
}

auto ElementPart::AddAttribute(
	std::string_view attribute,
	std::string_view value
) -> void
{
	auto found = std::ranges::find_if( attributes, [ attribute ]( const auto& entry ) { return entry.first == attribute; } );
	if( found == attributes.end() ) attributes.emplace_back( std::string{ attribute }, std::string{ value } );
	else found->second = value;
}

auto ElementPart::RemoveAttribute( std::string_view attribute ) -> void
{
	std::erase_if( attributes, [ attribute ]( const auto& entry ) { return entry.first == attribute; } );
}

}
