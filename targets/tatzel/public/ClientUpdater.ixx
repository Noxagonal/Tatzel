/// @file
/// @brief Internal sender for concrete browser DOM operations.
module;

#include <memory>
#include <span>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include <string_view>

export module UI.ClientUpdater;

export import UI.Protocol;

import UI.DOM.ElementPart;
import UI.DOM.SelectOption;

namespace tatzel {

export class ClientUpdater;

/// @brief Flushes an owning loop's accumulated updates when its outermost scope ends.
/// @details Nested scopes share the sender. Destruction also flushes completed
/// mutations during exception unwinding; it does not roll back logical changes.
export class ClientUpdateScope
{
public:
	/// @brief Enters a sender's nested update scope.
	/// @param updater Sender that outlives this guard.
	/// @pre Called on the sender's owning loop.
	/// @post Operations accumulate until the outermost scope ends or a size limit is reached.
	explicit ClientUpdateScope( ClientUpdater& updater );

	/// @brief Prevents duplicate scope completion.
	/// @param other Source guard.
	/// @pre None.
	/// @post Copy construction is unavailable.
	ClientUpdateScope( const ClientUpdateScope& other ) = delete;

	/// @brief Prevents duplicate scope completion.
	/// @param other Source guard.
	/// @return No value; assignment is unavailable.
	/// @pre None.
	/// @post Copy assignment is unavailable.
	auto operator=( const ClientUpdateScope& other ) -> ClientUpdateScope& = delete;

	/// @brief Flushes the outermost scope without letting transport failures escape.
	/// @pre The sender is alive on its owning loop.
	/// @post Completed operations are offered in order; a failed transport is closed.
	~ClientUpdateScope() noexcept;

private:
	ClientUpdater* updater;
};

/// @brief Serializes concrete DOM operations; all IDs accepted here are DOM part IDs.
export class ClientUpdater
{
public:
	/// @brief Binds owned transport callbacks without exposing a networking or JSON backend.
	/// @param sink Callback consuming or copying a borrowed complete JSON frame synchronously.
	/// @param close_transport Optional callback closing a failed browser transport.
	/// @pre sink is nonempty; callbacks and their captures remain usable until destruction.
	/// @post Frames use the sink; only the implementation retains protocol state.
	/// @throws tatzel::Exception If sink is empty.
	explicit ClientUpdater(
		std::function<void( std::string_view )> sink,
		std::function<void()> close_transport = {}
	);

	/// @brief Releases protocol state without sending more frames.
	/// @pre All update scopes have ended; called on the owning loop.
	/// @post Transport callbacks and queued operations are released.
	~ClientUpdater();

	/// @brief Prevents duplicate ownership of one protocol stream.
	/// @param other Source sender.
	/// @pre None.
	/// @post Copy construction is unavailable.
	ClientUpdater( const ClientUpdater& other ) = delete;

	/// @brief Prevents rebinding a live protocol stream.
	/// @param other Source sender.
	/// @return No value; assignment is unavailable.
	/// @pre None.
	/// @post Copy assignment is unavailable.
	auto operator=( const ClientUpdater& other ) -> ClientUpdater& = delete;

	/// @brief Enters a nested automatic batching boundary.
	/// @return Noncopyable guard borrowing this sender.
	/// @pre Called on the owning loop while this sender is alive.
	/// @post The returned scope controls ordered flushing.
	auto BeginBatch() -> ClientUpdateScope { return ClientUpdateScope{ *this }; }

	/// @brief Flushes queued operations as one bounded ordered batch.
	/// @pre Called on the owning loop; no frame sink is reentering this sender.
	/// @post Pending operations are consumed and the sequence advances after a successful send.
	/// @throws tatzel::Exception If serialization, sequence limits, or transport delivery fails.
	auto Flush() -> void;

	/// @brief Discards pending operations after a session or transport closes.
	/// @pre Called on the owning loop.
	/// @post New operations and scope completion cannot send to the closed transport.
	auto Close() noexcept -> void;

	/// @brief Reads the last successfully emitted batch sequence.
	/// @return Zero before the first batch, otherwise a JavaScript-safe integer.
	/// @pre Called on the owning loop.
	/// @post State is unchanged.
	auto GetLastSequence() const noexcept -> std::uint64_t;

	/// @brief Flushes prior updates before reporting an application or protocol failure.
	/// @param code Stable public failure code.
	/// @param message Public description, excluding server traces and exception details.
	/// @param fatal Whether this page requires replacement rather than continued interaction.
	/// @pre Called on the owning loop with an open sender.
	/// @post Prior mutations precede the error frame; fatal errors close update submission.
	auto SendError(
		std::string_view code,
		std::string_view message,
		bool fatal = false
	) -> void;

	/// @brief Flushes the initial page before acknowledging its readiness.
	/// @pre Called after the page factory and logical cleanup complete successfully.
	/// @post The readiness frame names the last emitted batch sequence.
	auto SendReady() -> void;


	/// @brief Queues creation of concrete DOM nodes.
	/// @param parts Parts ordered so parents precede their children.
	/// @pre Parent DOM nodes already exist or appear earlier in parts.
	/// @post One create_element_part message contains copied initial DOM state.
	auto CreateElementParts( std::span<const dom::ElementPart> parts ) -> void;

	/// @brief Queues removal of concrete DOM nodes.
	/// @param parts Concrete DOM parts to remove.
	/// @pre The caller separately manages logical ownership.
	/// @post One remove_element_part message is queued; logical ownership is unchanged.
	auto RemoveElementParts( std::span<const dom::ElementPart> parts ) -> void;

	/// @brief Queues an HTML attribute update.
	/// @param part_id Target DOM node ID.
	/// @param attribute HTML attribute name.
	/// @param attribute_value New HTML attribute value.
	/// @pre The target exists and attribute is not an inline event-handler attribute.
	/// @post A set_attribute message is queued.
	auto SetAttribute(
		std::string_view part_id,
		std::string_view attribute,
		std::string_view attribute_value
	) -> void;

	/// @brief Queues removal of an HTML attribute.
	/// @param part_id Target DOM node ID.
	/// @param attribute HTML attribute name.
	/// @pre The target exists.
	/// @post A remove_attribute message is queued.
	auto RemoveAttribute(
		std::string_view part_id,
		std::string_view attribute
	) -> void;

	/// @brief Queues replacement of a DOM node's text content.
	/// @param part_id Target DOM node ID.
	/// @param text New plain text content.
	/// @pre The target exists and replacing its descendants is intended.
	/// @post A set_text message is queued.
	auto SetText(
		std::string_view part_id,
		std::string_view text
	) -> void;

	/// @brief Queues an explicit server value replacement for a form control.
	/// @param part_id Editor DOM node ID.
	/// @param value New text value, or true/false for a checkbox.
	/// @param update Field revision and intentional-replacement policy.
	/// @pre The target is an existing supported form control.
	/// @post A set_value message is queued; accepted browser edits do not call this method.
	auto SetValue(
		std::string_view part_id,
		std::string_view value,
		ui::ValueUpdate update = {}
	) -> void;

	/// @brief Queues a concrete HTML dialog state change.
	/// @param part_id DOM ID of an HTML dialog.
	/// @param open Whether the dialog should be open.
	/// @pre The target is an existing HTML dialog.
	/// @post A set_modal_open message is queued for the logical modal's adapter.
	auto SetModalOpen(
		std::string_view part_id,
		bool open
	) -> void;

	/// @brief Marks a DOM node as a logical click event source.
	/// @param part_id Target DOM node ID.
	/// @pre The target exists and its logical owner accepts click events.
	/// @post A set_on_click message is queued.
	auto SetOnClick( std::string_view part_id ) -> void;

	/// @brief Marks a DOM node as a logical change event source.
	/// @param part_id Target DOM node ID.
	/// @pre The target exists and its logical owner accepts change events.
	/// @post A set_on_change message is queued.
	auto SetOnChange( std::string_view part_id ) -> void;

	/// @brief Marks a DOM node as a logical submit event source.
	/// @param part_id Target DOM node ID.
	/// @pre The target exists and its logical owner accepts submit events.
	/// @post A set_on_submit message is queued.
	auto SetOnSubmit( std::string_view part_id ) -> void;

	/// @brief Queues native select choices and selection as one ordered update.
	/// @param part_id Target select editor DOM ID.
	/// @param options Owned-payload choices, copied during serialization.
	/// @param placeholder Plain text for the empty-value choice.
	/// @param value Selected option value, or empty for no selection.
	/// @param update Field generation and browser-edit baseline.
	/// @pre The select editor has been created and value is available among its choices.
	/// @post One set_select_options message queues option replacement and selected state.
	auto SetSelectOptions(
		std::string_view part_id,
		std::span<const dom::SelectOption> options,
		std::string_view placeholder,
		std::string_view value,
		ui::ValueUpdate update = {}
	) -> void;

	/// @brief Queues native radio choices and selected state atomically.
	/// @param part_id Registered radio-group editor part ID.
	/// @param options Choices copied during serialization.
	/// @param value Enabled selected ID, or empty for no selection.
	/// @param update Field generation and browser-edit baseline.
	/// @pre The group and its options container have been created.
	/// @post One set_radio_options operation is queued.
	auto SetRadioOptions(
		std::string_view part_id,
		std::span<const dom::SelectOption> options,
		std::string_view value,
		ui::ValueUpdate update = {}
	) -> void;

	/// @brief Reorders a parent's complete set of existing element children.
	/// @param parent_id Concrete parent part identity.
	/// @param child_ids Complete unique child identities in final DOM order.
	/// @pre Children exist under parent_id when this operation reaches the browser.
	/// @post One set_child_order operation preserves existing nodes and their logical ownership.
	auto SetChildOrder(
		std::string_view parent_id,
		std::span<const std::string> child_ids
	) -> void;

	/// @brief Queues a same-page browser history update without echoing navigation.
	/// @param hash Validated empty or hash-prefixed location.
	/// @pre Called on the owning event loop with a live transport.
	/// @post The browser has been offered a history update; application navigation is separate.
	auto SetLocation( std::string_view hash ) -> void;

private:
	friend class ClientUpdateScope;

	/// @brief Ends one nested scope and contains flush failures at an unwinding boundary.
	/// @pre A matching scope exists on this sender's owning loop.
	/// @post Outermost updates are flushed, or the sender is closed and the failure logged.
	auto EndBatch() noexcept -> void;

	/// @brief Owns private wire values and transport callbacks.
	struct Implementation;
	std::unique_ptr<Implementation> implementation;

};

}
