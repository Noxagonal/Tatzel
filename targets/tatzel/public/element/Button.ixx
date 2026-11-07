/// @file
/// @brief Application-facing button state and click callback.
module;

#include <functional>
#include <string>
#include <string_view>
#include <utility>

export module UI.Element.Button;

export import UI.Element.LogicalElement;
export import UI.Element.Style;
export import UI.Element.Interface.Clickable;

namespace tatzel::ui {

/// @brief Logical activation behavior chosen independently of HTML.
export enum class ButtonRole
{
	Action, ///< Invoke a server-side click callback.
	Submit, ///< Request coordinated submission of the enclosing form.
};

/// @brief Semantic emphasis for a logical button.
export enum class ButtonVariant
{
	Primary, ///< Main action in its context.
	Secondary, ///< Supporting or cancellation action.
	Danger, ///< Destructive action requiring clear emphasis.
};

/// @brief Logical button with owned text and a server-side callback.
export class Button : public LogicalElement, public interface::Clickable
{
public:
	/// @brief Creates a logical button.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param text Initial button label.
	/// @param on_click Server-side action callback; empty or submit role means no click action.
	/// @param role Whether activation invokes a click callback or submits a form.
	/// @pre parent is nonnull; callback captures remain valid for the page lifetime.
	/// @post The button owns its text and callback; the builder assigns DOM parts.
	Button(
		std::string_view id,
		LogicalElement* parent,
		std::string_view text,
		std::function<void()> on_click,
		ButtonRole role = ButtonRole::Action
	) :
		LogicalElement{ id, parent },
		text{ text },
		on_click{ std::move( on_click ) },
		role{ role }
	{}

	/// @brief Reads the button label.
	/// @return Borrowed logical text.
	/// @pre None.
	/// @post State is unchanged.
	auto GetText() const noexcept -> std::string_view { return text; }

	/// @brief Replaces the label and notifies the adapter.
	/// @param text New logical label.
	/// @pre Called on the owning event loop.
	/// @post The button owns text; a bound adapter queues its DOM text update.
	auto SetText( std::string_view text ) -> void;

	/// @brief Reads button emphasis.
	/// @return Current logical button emphasis.
	/// @pre None.
	/// @post State is unchanged.
	auto GetVariant() const noexcept -> ButtonVariant { return variant; }

	/// @brief Changes button emphasis without altering appearance or value state.
	/// @param variant New logical button emphasis.
	/// @pre Called on the owning loop; enum arguments are defined values.
	/// @post Changed state is stored and a bound adapter queues its presentation.
	auto SetVariant( ButtonVariant variant ) -> void;

	/// @brief Reads control size.
	/// @return Current logical control size.
	/// @pre None.
	/// @post State is unchanged.
	auto GetSize() const noexcept -> ControlSize { return size; }

	/// @brief Changes control size without altering appearance or value state.
	/// @param size New logical control size.
	/// @pre Called on the owning loop; enum arguments are defined values.
	/// @post Changed state is stored and a bound adapter queues its presentation.
	auto SetSize( ControlSize size ) -> void;

	/// @brief Reports whether this button rejects activation.
	/// @return Whether the button is explicitly disabled.
	/// @pre None.
	/// @post State is unchanged.
	auto IsDisabled() const noexcept -> bool { return disabled; }

	/// @brief Enables or disables button activation.
	/// @param disabled Whether native and server-side activation are suppressed.
	/// @pre Called on the owning loop.
	/// @post State is stored and the adapter synchronizes native disabled state.
	auto SetDisabled( bool disabled ) -> void;

	/// @brief Replaces the server-side click callback.
	/// @param on_click New callback, or empty to make clicks no-ops.
	/// @pre Called on the owning event loop; captured objects outlive their use.
	/// @post Future action-role clicks invoke the new callback; submit-role behavior is unchanged.
	auto SetOnClick( std::function<void()> on_click ) -> void { this->on_click = std::move( on_click ); }

	/// @brief Dispatches a browser click to application code.
	/// @pre Called on the owning event loop after the part ID resolves to this button.
	/// @post An active action-role button invokes its installed callback; removed and submit-role buttons ignore this dispatch.
	auto InvokeOnClick() -> void override;

	/// @brief Reads the immutable activation role.
	/// @return Action or form submission behavior.
	/// @pre None.
	/// @post State is unchanged.
	auto GetRole() const noexcept -> ButtonRole { return role; }

private:
	std::string text;
	std::function<void()> on_click;
	ButtonRole role;
	ButtonVariant variant = ButtonVariant::Primary;
	ControlSize size = ControlSize::Medium;
	bool disabled = false;
};

}
