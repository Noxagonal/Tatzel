/// @file
/// @brief Logical grouping of child controls.
module;

#include <string_view>

export module UI.Element.Container;

export import UI.Element.LogicalElement;

namespace tatzel::ui {

/// @brief Logical container; its adapter decides which browser node holds children.
export class Container : public LogicalElement
{
public:
	/// @brief Creates an empty logical container.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @pre parent is nonnull and outlives this control; id is unique within the page.
	/// @post The container has no logical children; the builder assigns its DOM parts.
	Container(
		std::string_view id,
		LogicalElement* parent
	) :
		LogicalElement{ id, parent }
	{}
};

}
