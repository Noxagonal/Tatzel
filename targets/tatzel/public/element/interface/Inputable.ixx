/// @file
/// @brief Logical text-edit event interface.
module;

#include <string_view>

export module UI.Element.Interface.Inputable;

namespace tatzel::ui::interface {

/// @brief Framework interface for controls accepting uncommitted browser edits.
export class Inputable
{
public:
	/// @brief Releases the interface.
	/// @pre No callback is executing on this control.
	/// @post Interface resources are released.
	virtual ~Inputable() = default;

	/// @brief Accepts a browser edit before invoking application code.
	/// @param value Edited text, borrowed for this call.
	/// @pre Called on the owning event loop for a registered input source.
	/// @post Accepted input is stored and application-defined handling has completed.
	virtual auto InvokeOnInput( std::string_view value ) -> void = 0;
};

}
