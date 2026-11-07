/// @file
/// @brief Coordinated logical form submission and field validation.
module;

#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module UI.Element.Form;

export import UI.Element.FormField;
export import UI.Element.Interface.Submittable;

namespace tatzel::ui {

/// @brief One framework-resolved field value in a browser form snapshot.
export struct FieldSubmission
{
	FormField* field; ///< Borrowed logical field, resolved from an exact registered editor part.
	FieldValue value; ///< Owned submitted value.
	std::optional<ClientEdit> edit; ///< Wire metadata; omitted for trusted programmatic snapshots.
};

/// @brief Page-owned form that validates all fields before invoking its submit callback.
/// @details Fields may be nested inside layouts or cards. Submission snapshots
/// are checked in full before values are applied. Edit callbacks are not invoked
/// while applying a snapshot. Nested forms are rejected. All operations and
/// callbacks run on the owning page's event loop.
export class Form : public LogicalElement, public interface::Submittable
{
public:
	/// @brief Creates an enabled logical form with an optional accessible label.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param label Optional accessible form name, copied before returning.
	/// @param on_submit Optional callback invoked only after successful validation.
	/// @pre parent is nonnull; callback captures outlive their invocation.
	/// @post The form owns its label and callback; the builder supplies parts and children.
	/// @throws tatzel::Exception If a logical ancestor is already a Form.
	Form(
		std::string_view id,
		LogicalElement* parent,
		std::string_view label,
		std::function<void()> on_submit
	);

	/// @brief Reads the accessible form name.
	/// @return Borrowed label, invalidated by label mutation or destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetLabel() const noexcept -> std::string_view { return label; }

	/// @brief Replaces the accessible form name.
	/// @param label New name, copied before returning; empty removes the name.
	/// @pre Called on the owning event loop.
	/// @post State is stored and a bound adapter queues the accessibility update.
	auto SetLabel( std::string_view label ) -> void;

	/// @brief Reports whether the form is disabled.
	/// @return Whether editing and submission are blocked for the form subtree.
	/// @pre None.
	/// @post State is unchanged.
	auto IsDisabled() const noexcept -> bool { return disabled; }

	/// @brief Changes the form-wide enabled state without changing individual field flags.
	/// @param disabled Whether the form and its controls should be disabled.
	/// @pre Called on the owning event loop.
	/// @post State is stored and the adapter queues a subtree-disabled presentation update.
	auto SetDisabled( bool disabled ) -> void;

	/// @brief Collects borrowed logical fields throughout this form's child tree.
	/// @return Fields in logical traversal order, including disabled fields.
	/// @pre Called on the owning event loop; the form remains alive while pointers are used.
	/// @post Ownership and logical state are unchanged.
	auto GetFields() const -> std::vector<FormField*>;

	/// @brief Validates snapshot membership, uniqueness, types, and choices before applying values.
	/// @param submission Exactly one value for every enabled descendant field.
	/// @return Whether the snapshot was accepted; invalid snapshots do not change any field.
	/// @pre Called on the owning event loop; field pointers have been resolved within the current page.
	/// @post Accepted values are applied without edit callbacks; read-only fields retain server values.
	/// @note Framework integration API; form validation and submit callbacks run separately.
	auto ApplySubmission( std::span<const FieldSubmission> submission ) -> bool;

	/// @brief Validates all current field values, including read-only fields.
	/// @return Whether every enabled field is valid; disabled forms succeed and removed forms cancel validation.
	/// @pre Called on the owning event loop; validators do not mutate values or structure.
	/// @post Every checked field displays its current error; all fields are checked even after a failure.
	auto Validate() -> bool;

	/// @brief Reports whether an enabled field currently has a validation error.
	/// @return Whether enabled-field validation errors exist; false for a disabled form.
	/// @pre Called on the owning event loop.
	/// @post No validators run and no state changes.
	auto HasErrors() const -> bool;

	/// @brief Validates current logical values and invokes the appropriate form callback.
	/// @return Whether enabled valid submission occurred.
	/// @pre Called on the owning event loop; callbacks and validators obey page lifetime rules.
	/// @post A surviving form invokes its valid/invalid callback if installed; removal cancels both callbacks.
	/// @note Programmatic submission uses current server values, not browser edits still in flight.
	auto Submit() -> bool;

	/// @brief Dispatches browser submission after its complete snapshot is applied.
	/// @pre Called on the owning event loop after ApplySubmission succeeds.
	/// @post Validation and the appropriate form callback have completed.
	auto InvokeOnSubmit() -> void override { Submit(); }

	/// @brief Replaces the successful-submission callback.
	/// @param on_submit New callback, or empty to clear it.
	/// @pre Called on the owning event loop; captures outlive their invocation.
	/// @post Future valid submissions invoke this callback.
	auto SetOnSubmit( std::function<void()> on_submit ) -> void { this->on_submit = std::move( on_submit ); }

	/// @brief Replaces the invalid-submission callback.
	/// @param on_invalid New callback, or empty to clear it.
	/// @pre Called on the owning event loop; captures outlive their invocation.
	/// @post Future validation failures invoke this callback after field errors are updated.
	auto SetOnInvalid( std::function<void()> on_invalid ) -> void { this->on_invalid = std::move( on_invalid ); }

private:
	/// @brief Collects logical field descendants without inspecting DOM identities.
	/// @param parent Logical subtree to visit.
	/// @param fields Borrowed-field output list.
	/// @pre The subtree remains alive on its owning event loop.
	/// @post Descendant field pointers are appended in logical traversal order.
	static auto CollectFields(
		const LogicalElement& parent,
		std::vector<FormField*>& fields
	) -> void;

	std::string label;
	bool disabled = false;
	std::function<void()> on_submit;
	std::function<void()> on_invalid;
};

}
