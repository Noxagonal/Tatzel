/// @file
/// @brief Application-facing button state and click callback.
module;

#include <functional>
#include <string>
#include <string_view>
#include <utility>

export module UI.Element.Button;

export import UI.Element.LogicalElement;
export import UI.Element.Interface.Clickable;

namespace tatzel::ui {

/// @brief Logical button with owned text and a server-side callback.
export class Button : public LogicalElement, public interface::Clickable
{
public:
	/// @brief Creates a logical button.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param text Initial button label.
	/// @param on_click Server-side click callback; empty means no action.
	/// @pre parent is nonnull; callback captures remain valid for the page lifetime.
	/// @post The button owns its text and callback; the builder assigns DOM parts.
	Button(
		std::string_view id,
		LogicalElement* parent,
		std::string_view text,
		std::function<void()> on_click
	) :
		LogicalElement{ id, parent },
		text{ text },
		on_click{ std::move( on_click ) }
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
	auto SetText( std::string_view text ) -> void
	{
		if( this->text == text ) return;
		this->text = text;
		NotifyChanged();
	}

	/// @brief Replaces the server-side click callback.
	/// @param on_click New callback, or empty to make clicks no-ops.
	/// @pre Called on the owning event loop; captured objects outlive their use.
	/// @post Future clicks invoke the new callback, if present.
	auto SetOnClick( std::function<void()> on_click ) -> void { this->on_click = std::move( on_click ); }

	/// @brief Dispatches a browser click to application code.
	/// @pre Called on the owning event loop after the part ID resolves to this button.
	/// @post The installed callback has run, if present; callback effects are application-defined.
	auto InvokeOnClick() -> void override { if( on_click ) { auto callback = on_click; callback(); } }

private:
	std::string text;
	std::function<void()> on_click;
};

}
