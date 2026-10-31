/// @file
/// @brief Logical modal dialog with synchronized open and close state.
module;

#include <functional>
#include <string>
#include <string_view>
#include <utility>

export module UI.Element.Modal;

export import UI.Element.LogicalElement;
export import UI.Element.Interface.Dismissible;

namespace tatzel::ui {

/// @brief Page-owned modal whose adapter owns dialog structure and focus behavior.
/// @details Title and callback are owned. Mutations run on the owning event loop.
/// Closing stores state and queues the browser update before invoking on_close.
export class Modal : public LogicalElement, public interface::Dismissible
{
public:
	/// @brief Creates a closed modal before its content is built.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param title Accessible dialog title, copied before returning.
	/// @pre parent is nonnull and outlives this control.
	/// @post The modal is closed and owns its title; the builder assigns its parts.
	Modal(
		std::string_view id,
		LogicalElement* parent,
		std::string_view title
	) :
		LogicalElement{ id, parent },
		title{ title }
	{}

	/// @brief Reads the accessible dialog title.
	/// @return The accessible dialog title, borrowed until mutation or destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetTitle() const noexcept -> std::string_view { return title; }

	/// @brief Replaces the accessible dialog title and notifies the adapter.
	/// @param title New accessible dialog title, copied before returning.
	/// @pre Called on the owning event loop.
	/// @post Logical state is stored and a bound adapter queues any corresponding DOM changes.
	auto SetTitle( std::string_view title ) -> void
	{
		if( this->title == title ) return;

		this->title = title;
		NotifyChanged();
	}

	/// @brief Reads the logical open state.
	/// @return The logical open state.
	/// @pre None.
	/// @post State is unchanged.
	auto IsOpen() const noexcept -> bool { return open; }

	/// @brief Opens the logical modal and queues its browser presentation.
	/// @pre Called on the owning event loop after the modal's content is built.
	/// @post The modal is logically open; a bound adapter queues a modal-open update.
	auto Open() -> void
	{
		if( open ) return;

		open = true;
		NotifyChanged();
	}

	/// @brief Closes the modal and invokes its close callback once per transition.
	/// @pre Called on the owning event loop.
	/// @post Closed state and browser update precede the callback; callback effects may reopen it.
	auto Close() -> void
	{
		if( !open ) return;

		open = false;
		NotifyChanged();

		auto callback = on_close;
		if( callback ) callback();
	}

	/// @brief Replaces the close-transition callback.
	/// @param on_close New callback, or empty to clear it.
	/// @pre Called on the owning event loop; captured objects outlive callback invocations.
	/// @post Future accepted events use the new callback.
	auto SetOnClose( std::function<void()> on_close ) -> void { this->on_close = std::move( on_close ); }

	/// @brief Accepts an Escape, backdrop, close-button, or native-close request.
	/// @pre Called on the owning event loop for this modal's dismissal source.
	/// @post The close transition is processed exactly as a logical Close call.
	auto InvokeOnDismiss() -> void override { Close(); }

private:
	std::string title;
	bool open = false;
	std::function<void()> on_close;
};

}
