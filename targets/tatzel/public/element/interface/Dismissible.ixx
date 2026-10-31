/// @file
/// @brief Logical browser-dismissal event interface.
export module UI.Element.Interface.Dismissible;

namespace tatzel::ui::interface {

/// @brief Framework interface for controls accepting browser dismissal requests.
export class Dismissible
{
public:
	/// @brief Releases the interface.
	/// @pre No callback is executing on this control.
	/// @post Interface resources are released.
	virtual ~Dismissible() = default;

	/// @brief Accepts a browser request to dismiss this control.
	/// @pre Called on the owning event loop for a registered dismissal source.
	/// @post The logical control has processed the request.
	virtual auto InvokeOnDismiss() -> void = 0;
};

}
