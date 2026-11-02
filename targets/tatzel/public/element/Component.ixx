/// @file
/// @brief Logical lifetime boundary for ordinary C++ composed controls.
module;

#include <string_view>

export module UI.Element.Component;

export import UI.Element.Container;

namespace tatzel::ui {

/// @brief Page-owned base for controls composed from existing logical children.
/// @details Derived classes provide Build(builder), own their local state and
/// callbacks, and retain checked child handles. The logical tree owns the complete
/// subtree. All methods run on the owning UI loop; background work resolves a
/// checked component handle inside a dispatched callback.
export class Component : public Container
{
public:
	/// @brief Creates an empty composed control before adapter mapping and Build.
	/// @param id Unique logical identity.
	/// @param parent Owning logical parent.
	/// @pre parent is nonnull and belongs to the same live page.
	/// @post The component has no children; PageBuilder attaches it before Build.
	Component(
		std::string_view id,
		LogicalElement* parent
	) :
		Container{ id, parent }
	{}
};

}
