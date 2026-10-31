/// @file
/// @brief Owned plain text displayed inline by the default adapter.
module;

#include <string>
#include <string_view>

export module UI.Element.Text;

export import UI.Element.LogicalElement;

namespace tatzel::ui {

/// @brief Logical plain text with a stable page-owned identity.
/// @details Text is owned; mutations run on the owning page's event loop.
export class Text : public LogicalElement
{
public:
	/// @brief Creates a plain text control.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param text Initial plain text, copied before returning.
	/// @pre parent is nonnull and outlives this control.
	/// @post The control owns text; its adapter representation is not yet assigned.
	Text(
		std::string_view id,
		LogicalElement* parent,
		std::string_view text
	) :
		LogicalElement{ id, parent },
		text{ text }
	{}

	/// @brief Reads the current plain text.
	/// @return Borrowed text, invalidated by text mutation or destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetText() const noexcept -> std::string_view { return text; }

	/// @brief Replaces the text and synchronizes its adapter.
	/// @param text New plain text, copied before returning.
	/// @pre Called on the owning event loop.
	/// @post Changed text is owned and its bound adapter queues a plain text update.
	auto SetText( std::string_view text ) -> void
	{
		if( this->text == text ) return;

		this->text = text;
		NotifyChanged();
	}

private:
	std::string text;
};

}
