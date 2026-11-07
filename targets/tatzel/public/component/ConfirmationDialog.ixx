/// @file
/// @brief Reusable confirmation component with exactly one explicit outcome.
module;

#include <functional>
#include <string>
#include <string_view>
#include <utility>

export module UI.Component.ConfirmationDialog;

export import UI.PageBuilder;

namespace tatzel::ui {

/// @brief Owned accessible wording and emphasis for a confirmation instance.
export struct ConfirmationOptions
{
	std::string title = "Confirm action"; ///< Accessible dialog title.
	std::string message; ///< Plain-text explanation.
	std::string confirm_label = "Confirm"; ///< Acceptance button text.
	std::string cancel_label = "Cancel"; ///< Cancellation button text.
	ButtonVariant variant = ButtonVariant::Danger; ///< Emphasis of the acceptance action.
};

/// @brief Composes a native Modal with acceptance and cancellation actions.
/// @details One pending request is allowed. Explicit cancellation and every native
/// dismissal deliver false once; acceptance delivers true once after closure.
/// Disposal silently drops the pending callback. Callbacks run on the owning loop
/// and may remove this component or start a new request.
export class ConfirmationDialog : public Component
{
public:
	/// @brief Owns configuration before the component is attached and built.
	/// @param id Unique logical identity.
	/// @param parent Owning logical parent.
	/// @param options Owned dialog wording and emphasis.
	/// @pre parent is nonnull and button labels are accessible descriptions.
	/// @post The component is configured without an open request.
	ConfirmationDialog(
		std::string_view id,
		LogicalElement* parent,
		ConfirmationOptions options = {}
	) :
		Component{ id, parent, "confirmation-dialog" },
		options{ std::move( options ) }
	{}

	/// @brief Builds the reusable native dialog and its owned actions.
	/// @tparam BuilderT Factory-supplied scoped builder type.
	/// @param ui Factory-supplied builder scoped to this component.
	/// @pre Called once after attaching this component.
	/// @post The modal is closed and all dismissal paths share one cancellation boundary.
	template<typename BuilderT>
	auto Build( BuilderT ui ) -> void
	{
		auto self = GetHandle();
		auto* modal = ui.Modal( options.title, [ this, self ]( auto content ) {
			content.Paragraph( options.message );
			content.Row( [ this, self ]( auto actions ) {
				actions.Button( options.cancel_label, [ self ] { if( auto* owner = self.Get() ) owner->Cancel(); } )->SetVariant( ButtonVariant::Secondary );
				actions.Button( options.confirm_label, [ self ] { if( auto* owner = self.Get() ) owner->Resolve( true ); } )->SetVariant( options.variant );
			}, { .alignment = Alignment::Center, .wrap = true } );
		} );
		dialog = modal->GetHandle();
		modal->SetOnClose( [ self ] { if( auto* owner = self.Get() ) owner->Resolve( false ); } );
	}

	/// @brief Opens one confirmation request with an owned result callback.
	/// @param on_result Callback receiving true for acceptance or false for cancellation.
	/// @pre Called on the owning loop after Build; captures outlive invocation.
	/// @post A new request is pending and the native modal is offered to the browser.
	/// @throws tatzel::Exception If a request is already pending, callback is empty, or the modal is inactive.
	auto Request( std::function<void( bool )> on_result ) -> void;

	/// @brief Reports whether this instance has an unanswered request.
	/// @return Whether a live request is pending.
	/// @pre Called on the owning loop.
	/// @post State is unchanged.
	auto IsPending() const noexcept -> bool { return !IsRemoved() && pending; }

	/// @brief Cancels a pending request through the shared outcome boundary.
	/// @pre Called on the owning loop.
	/// @post An active pending request is closed and reports false exactly once.
	auto Cancel() -> void { Resolve( false ); }

private:
	/// @brief Closes a request before invoking its retained outcome callback.
	/// @param accepted Whether the explicit acceptance action was chosen.
	/// @pre Called on the owning loop.
	/// @post The old callback is consumed before reentrant close or application code runs.
	auto Resolve( bool accepted ) -> void;

	ConfirmationOptions options;
	ElementHandle<Modal> dialog;
	std::function<void( bool )> on_result;
	bool pending = false;
};

}
