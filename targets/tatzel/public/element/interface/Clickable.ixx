/**
 * @file
 * @brief Logical click event interface.
 */
module;

export module UI.Element.Interface.Clickable;

namespace tatzel::ui::interface {

/** @brief Interface implemented by logical controls accepting click events. */
export class Clickable
{
public:
	/** @brief Releases the logical event interface. @pre No callback is executing on this control. @post Interface resources are released. */
	virtual ~Clickable() = default;

	/**
	 * @brief Dispatches a click event to the logical control.
	 * @pre Called on the owning event loop after a DOM part resolves to this control.
	 * @post Application-defined event handling has completed.
	 */
	virtual auto InvokeOnClick(  ) -> void = 0;
};

}
