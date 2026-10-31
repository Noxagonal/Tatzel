/// @file
/// @brief Concrete native-select option payloads used by browser operations.
module;

#include <string>

export module UI.DOM.SelectOption;

namespace tatzel::dom {

/// @brief Owned browser option data; native option nodes do not emit logical events.
export struct SelectOption
{
	std::string value; ///< Native option value.
	std::string label; ///< Plain visible text.
	bool disabled; ///< Whether native selection is unavailable.
};

}
