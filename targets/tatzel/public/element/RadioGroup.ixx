/// @file
/// @brief Native radio presentation of a logical stable-ID single choice.
module;

#include <functional>
#include <string_view>
#include <utility>
#include <vector>

export module UI.Element.RadioGroup;

export import UI.Element.Select;

namespace tatzel::ui {

/// @brief An owned radio choice with a unique ID, plain label, and disabled flag.
export using RadioOption = SelectOption;

/// @brief Page-owned single-choice field rendered as an accessible native radio group.
/// @details Inherits Select's stable IDs, validation, owned callbacks, option
/// replacement, and event-loop lifetime rules. Empty means no checked choice.
export class RadioGroup : public Select
{
public:
	/// @brief Creates a labeled group with an optional enabled initial choice.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param label Accessible group label, copied before returning.
	/// @param options Owned choices with unique nonempty IDs.
	/// @param value Enabled initial choice ID, or empty for no selection.
	/// @param on_change Optional accepted-selection callback.
	/// @pre parent is nonnull; callback captures outlive invocation.
	/// @post Choices, selected ID, and callback are owned; the adapter supplies parts.
	/// @throws tatzel::Exception If choices or initial selection are invalid.
	RadioGroup(
		std::string_view id,
		LogicalElement* parent,
		std::string_view label,
		std::vector<RadioOption> options,
		std::string_view value,
		std::function<void( std::string_view )> on_change
	) :
		Select{ id, parent, label, std::move( options ), value, std::move( on_change ) }
	{}
};

}
