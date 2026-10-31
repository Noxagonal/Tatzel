/// @file
/// @brief Logical change event interface.
module;

#include <string_view>

export module UI.Element.Interface.Changeable;

namespace tatzel::ui::interface {

/// @brief Interface implemented by logical controls accepting change events.
export class Changeable
{
public:
	/// @brief Releases the logical event interface.
	/// @pre No callback is executing on this control.
	/// @post Interface resources are released.
	virtual ~Changeable() = default;

	/// @brief Dispatches a change event to the logical control.
	/// @param value Changed value; checkbox booleans are normalized to true or false strings.
	/// @pre Called on the owning event loop after a DOM part resolves to this control.
	/// @post Application-defined event handling has completed.
	virtual auto InvokeOnChange( std::string_view value ) -> void = 0;
};

}
