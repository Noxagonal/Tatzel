/// @file
/// @brief Scheme validation shared by logical navigation and image resources.
module;

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
) -> bool;

}
