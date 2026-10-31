/// @file
/// @brief Logical row layout.
module;

#include <string_view>

export module UI.Element.Row;

export import UI.Element.Layout;

namespace tatzel::ui {

/// @brief Page-owned row whose adapter arranges logical children.
/// @details Settings and event-loop mutation rules are inherited from Layout.
export class Row : public Layout
{
public:
	/// @brief Creates an empty logical row.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param options Initial semantic spacing, alignment, distribution, and wrapping.
	/// @pre parent is nonnull; enum settings use defined values.
	/// @post The layout stores its settings; the builder assigns its parts and children.
	Row(
		std::string_view id,
		LogicalElement* parent,
		LayoutOptions options = {}
	) :
		Layout{ id, parent, LayoutDirection::Row, options }
	{}
};

}
