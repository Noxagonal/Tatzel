module;

#include <vector>
#include <string>
#include <string_view>

export module UI.ElementAdapter.ElementAdapter;

export import UI.UI.LogicalElement;

export import UI.UI.LogicalElement;
export import UI.DOM.Label;
export import UI.DOM.Container;
export import UI.DOM.Button;
export import UI.DOM.Heading;
export import UI.DOM.Paragraph;
export import UI.DOM.Span;
export import UI.DOM.Link;
export import UI.DOM.Image;
export import UI.DOM.HorizontalRule;
export import UI.DOM.Modal;
export import UI.DOM.Card;
export import UI.DOM.Header;
export import UI.DOM.Footer;


namespace tatzel {


export
class ElementAdapter
{
public:

	virtual ~ElementAdapter() = default;

	virtual auto CreatePartsFor( ui::LogicalElement& element ) const -> void = 0;
	virtual auto CreatePartsFor( ui::Heading& element ) const -> void = 0;

	/*
	virtual auto CreateElement( ui::LogicalElement* element ) -> ui::LogicalElement* = 0;
	//virtual auto CreateLabel( ui::Label* element ) -> ui::LogicalElement = 0;
	//virtual auto CreateContainer( ui::Container* element ) -> ui::LogicalElement = 0;
	//virtual auto CreateButton( ui::Button* element ) -> ui::LogicalElement = 0;
	virtual auto CreateHeading( ui::Heading* element ) -> ui::Heading* = 0;
	//virtual auto CreateParagraph( ui::Paragraph* element ) -> ui::LogicalElement = 0;
	//virtual auto CreateSpan( ui::Span* element ) -> ui::LogicalElement = 0;
	//virtual auto CreateLink( ui::Link* element ) -> ui::LogicalElement = 0;
	//virtual auto CreateImage( ui::Image* element ) -> ui::LogicalElement = 0;
	//virtual auto CreateHorizontalRule( ui::HorizontalRule* element ) -> ui::LogicalElement = 0;
	//virtual auto CreateModal( ui::Modal* element ) -> ui::LogicalElement = 0;
	//virtual auto CreateCard( ui::Card* element ) -> ui::LogicalElement = 0;
	//virtual auto CreateHeader( ui::Header* element ) -> ui::LogicalElement = 0;
	//virtual auto CreateFooter( ui::Footer* element ) -> ui::LogicalElement = 0;
	*/

	static auto MakePartID( ui::LogicalElement& element, std::string_view name ) -> std::string
	{
		return std::string{ element.GetID() } + "|" + std::string{ name };
	}
};


}
