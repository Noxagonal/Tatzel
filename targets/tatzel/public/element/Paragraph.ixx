/// @file
/// @brief Semantic paragraph using the plain text contract.
module;

#include <string_view>

export module UI.Element.Paragraph;

export import UI.Element.Text;

namespace tatzel::ui {

/// @brief Page-owned paragraph; its adapter chooses the block representation.
/// @details Text ownership and event-loop mutation rules are inherited from Text.
export class Paragraph : public Text
{
public:
	/// @brief Creates a semantic paragraph containing plain text.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param text Initial paragraph text, copied before returning.
	/// @pre parent is nonnull and outlives this control.
	/// @post The paragraph owns text; the builder assigns its representation.
	Paragraph(
		std::string_view id,
		LogicalElement* parent,
		std::string_view text
	) :
		Text{ id, parent, text }
	{}
};

}
