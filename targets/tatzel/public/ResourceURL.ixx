/// @file
/// @brief Scheme validation shared by logical navigation and image resources.
module;

#include <algorithm>
#include <string>
#include <string_view>

export module UI.ResourceURL;

namespace tatzel::detail {

/// @brief Checks allowed resource schemes before browser interpretation.
/// @param url Candidate URL; empty and relative references are permitted.
/// @param navigation Whether mailto and tel schemes are permitted.
/// @return Whether the URL is relative or uses an allowed explicit scheme.
/// @pre None.
/// @post No state changes.
export auto IsSafeResourceURL(
	std::string_view url,
	bool navigation
) -> bool
{
	if( std::ranges::any_of( url, []( unsigned char character ) {
		return character <= 0x20 || character == 0x7f || character == '\\';
	} ) ) return false;

	const auto delimiter = url.find_first_of( ":/?#" );
	if( delimiter == std::string_view::npos || url[ delimiter ] != ':' ) return true;

	auto scheme = std::string{ url.substr( 0, delimiter ) };
	std::ranges::transform( scheme, scheme.begin(), []( unsigned char character ) {
		return static_cast<char>( character >= 'A' && character <= 'Z' ? character + ( 'a' - 'A' ) : character );
	} );
	return scheme == "https" || scheme == "http" || ( navigation && ( scheme == "mailto" || scheme == "tel" ) );
}

}
