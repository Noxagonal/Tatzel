/// @file
/// @brief Logical change event interface.
module;

#include <string_view>

export module UI.Element.Interface.Changeable;

namespace tatzel::ui::interface {

/// @brief Wire value accepted by a logical change-event source.
export enum class ChangeValueType
{
	Text, ///< String value from a text editor or similar control.
	Boolean, ///< Boolean value from a checkbox or similar control.
};

/// @brief Interface implemented by logical controls accepting change events.
export class Changeable
{
public:
	/// @brief Releases the logical event interface.
	/// @pre No callback is executing on this control.
	/// @post Interface resources are released.
	virtual ~Changeable() = default;

	/// @brief Reports the value type transport routing must validate.
	/// @return The type accepted by this logical control.
	/// @pre None.
	/// @post State is unchanged.
	virtual auto GetChangeValueType() const noexcept -> ChangeValueType = 0;

	/// @brief Dispatches a change event to the logical control.
	/// @param value Changed value; checkbox booleans are normalized to true or false strings.
	/// @pre Called on the owning event loop after a DOM part resolves to this control.
	/// @post Application-defined event handling has completed.
	virtual auto InvokeOnChange( std::string_view value ) -> void = 0;
};

}
