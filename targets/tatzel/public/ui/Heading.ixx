module;

#include <string_view>
#include <string>
#include <stdexcept>

export module UI.DOM.Heading;

export import UI.UI.LogicalElement;


namespace tatzel::ui {


export
enum class HeadingStyle
{
	H1,
	H2,
	H3,
	H4,
	H5,
	H6,
};


export
inline auto HeadingStyleToTag( HeadingStyle style ) -> std::string_view
{
	switch( style )
	{
	case HeadingStyle::H1:
		return "h1";
	case HeadingStyle::H2:
		return "h2";
	case HeadingStyle::H3:
		return "h3";
	case HeadingStyle::H4:
		return "h4";
	case HeadingStyle::H5:
		return "h5";
	case HeadingStyle::H6:
		return "h6";
	}

	throw std::runtime_error{ "Invalid heading style" };
}


export
class Heading : public LogicalElement
{
public:

	using LogicalElement::LogicalElement;

	inline Heading(
		std::string_view id,
		LogicalElement* parent,
		std::string_view text,
		HeadingStyle heading_style
	) :
		LogicalElement{ id, parent },
		text{ std::string{ text } },
		heading_style{ heading_style }
	{}

	auto GetText() -> std::string_view { return this->text; }
	auto SetText( std::string_view text )
	{
		// Invoke the element adapter to update the corresponding parts.
		this->text = text;
	}

	auto GetHeadingStyle() -> HeadingStyle { return this->heading_style; }

private:

	std::string text;
	HeadingStyle heading_style = HeadingStyle::H1;
};


}
