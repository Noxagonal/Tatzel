/// @file
/// @brief Logical card with a title, content, and optional actions.
module;

#include <string>
#include <string_view>

export module UI.Element.Card;

export import UI.Element.LogicalElement;

namespace tatzel::ui {

/// @brief Page-owned card whose logical children occupy content or action slots.
/// @details The title is owned; mutations run on the owning page's event loop.
export class Card : public LogicalElement
{
public:
	/// @brief Creates a card before its children are built.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param title Optional heading, copied before returning.
	/// @param has_actions Whether the builder will populate the actions slot.
	/// @pre parent is nonnull and outlives this control.
	/// @post Title and slot availability are stored; the builder assigns DOM parts.
	Card(
		std::string_view id,
		LogicalElement* parent,
		std::string_view title,
		bool has_actions
	) :
		LogicalElement{ id, parent },
		title{ title },
		has_actions{ has_actions }
	{}

	/// @brief Reads the card heading.
	/// @return Borrowed title, invalidated by title mutation or destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetTitle() const noexcept -> std::string_view { return title; }

	/// @brief Replaces or hides the card heading.
	/// @param title New heading; an empty string hides the heading area.
	/// @pre Called on the owning event loop.
	/// @post Title is owned and a bound adapter queues the corresponding update.
	auto SetTitle( std::string_view title ) -> void
	{
		if( this->title == title ) return;

		this->title = title;
		NotifyChanged();
	}

	/// @brief Reports whether this card has an actions area.
	/// @return Whether an actions builder was supplied.
	/// @pre None.
	/// @post State is unchanged.
	auto HasActions() const noexcept -> bool { return has_actions; }

private:
	std::string title;
	bool has_actions;
};

}
