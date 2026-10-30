/** @file @brief Logical submit event interface. */
module;

export module UI.Element.Interface.Submittable;

namespace tatzel::ui::interface {

/** @brief Interface implemented by logical controls accepting submit events. */
export class Submittable
{
public:
	/** @brief Releases the logical event interface. @pre No callback is executing on this control. @post Interface resources are released. */
	virtual ~Submittable() = default;

	/**
	 * @brief Dispatches a submit event to the logical control.
	 * @pre Called on the owning event loop after a DOM part resolves to this control.
	 * @post Application-defined event handling has completed.
	 */
	virtual auto InvokeOnSubmit(  ) -> void = 0;
};

}
