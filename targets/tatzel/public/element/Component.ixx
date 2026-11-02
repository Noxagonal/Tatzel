/// @file
/// @brief Logical lifetime boundary for ordinary C++ composed controls.
module;

#include <string>
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
	/// @param component_name Stable adapter/theme identity, copied independently of appearance.
	/// @pre parent is nonnull and belongs to the same live page.
	/// @post The component has no children; PageBuilder attaches it before Build.
	Component(
		std::string_view id,
		LogicalElement* parent,
		std::string_view component_name = "component"
	) :
		Container{ id, parent },
		component_name{ component_name }
	{}

	/// @brief Reads the immutable theme identity shared by instances of this component type.
	/// @return Borrowed name valid for this component's lifetime.
	/// @pre None.
	/// @post State is unchanged; named appearance remains independent.
	auto GetComponentName() const noexcept -> std::string_view { return component_name; }

private:
	std::string component_name;
};

}
