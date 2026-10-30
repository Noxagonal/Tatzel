/** @file @brief Application-facing heading state. */
module;

#include <string>
#include <string_view>

export module UI.Element.Heading;

export import UI.Element.LogicalElement;

namespace tatzel::ui {

/** @brief Semantic heading level; the adapter chooses its HTML representation. */
export enum class HeadingStyle
{
	H1, ///< First-level heading.
	H2, ///< Second-level heading.
	H3, ///< Third-level heading.
	H4, ///< Fourth-level heading.
	H5, ///< Fifth-level heading.
	H6, ///< Sixth-level heading.
};

/** @brief Logical heading whose explicit setters synchronize through its adapter. */
export class Heading : public LogicalElement
{
public:
	/**
	 * @brief Creates a logical heading.
	 * @param id Unique logical ID.
	 * @param parent Owning logical parent.
	 * @param text Initial heading text.
	 * @param heading_style Semantic heading level.
	 * @pre parent is nonnull; id is unique; heading_style is a defined enum value.
	 * @post Text is owned by the heading; the builder assigns its DOM parts.
	 */
	Heading( std::string_view id, LogicalElement* parent, std::string_view text, HeadingStyle heading_style ) :
		LogicalElement{ id, parent }, text{ text }, heading_style{ heading_style }
	{}

	/** @brief Reads the logical text. @return Borrowed text. @pre None. @post State is unchanged. */
	auto GetText() const noexcept -> std::string_view { return text; }

	/**
	 * @brief Replaces the heading text and notifies its adapter.
	 * @param text New logical text.
	 * @pre Called on the owning event loop.
	 * @post The heading owns text; a bound adapter queues its DOM text update.
	 */
	auto SetText( std::string_view text ) -> void
	{
		if( this->text == text ) return;
		this->text = text;
		NotifyChanged();
	}

	/** @brief Reads the semantic heading level. @return Current heading style. @pre None. @post State is unchanged. */
	auto GetHeadingStyle() const noexcept -> HeadingStyle { return heading_style; }

private:
	std::string text;
	HeadingStyle heading_style;
};

}
