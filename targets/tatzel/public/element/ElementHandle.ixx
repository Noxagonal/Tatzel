/// @file
/// @brief Checked non-owning references to logical controls.
module;

#include <memory>
#include <utility>

export module UI.Element.ElementHandle;

import UI.Element.ElementLifecycle;

namespace tatzel::ui {

/// @brief Copyable weak reference that becomes empty on logical removal or page destruction.
/// @tparam ElementT Concrete control type, possibly const-qualified.
/// @details Handles do not retain controls or pages. A weak token's bookkeeping
/// may remain until its last handle is released. Resolution is confined to the
/// owning page's event loop and returns a borrowed pointer, not a lifetime lease.
export template<typename ElementT>
class ElementHandle
{
public:
	/// @brief Creates an empty checked handle.
	/// @pre None.
	/// @post Get returns nullptr.
	ElementHandle() = default;

	/// @brief Creates a checked reference for framework-owned lifetime state.
	/// @param element Borrowed control pointer at its stable address.
	/// @param lifetime Non-owning token associated with exactly this control.
	/// @pre The pointer and token refer to the same live logical control.
	/// @post The handle retains neither the control nor its token's strong ownership.
	/// @note Framework integration API; application code uses GetHandle.
	ElementHandle(
		ElementT* element,
		std::weak_ptr<ElementLifetimeState> lifetime
	) :
		element{ element },
		lifetime{ std::move( lifetime ) }
	{}

	/// @brief Resolves this handle if its logical control is still active.
	/// @return Borrowed control, or nullptr after removal, destruction, or for an empty handle.
	/// @pre Called on the owning page's event loop; the result is not retained across dispatches.
	/// @post No control or page lifetime is extended by the returned pointer.
	auto Get() const noexcept -> ElementT*
	{
		auto state = lifetime.lock();
		return state && state->IsActive() ? element : nullptr;
	}

	/// @brief Tests whether this handle currently resolves.
	/// @return Whether Get returns a nonnull pointer.
	/// @pre Called on the owning page's event loop.
	/// @post State is unchanged.
	explicit operator bool() const noexcept { return Get() != nullptr; }

private:
	ElementT* element = nullptr;
	std::weak_ptr<ElementLifetimeState> lifetime;
};

}
