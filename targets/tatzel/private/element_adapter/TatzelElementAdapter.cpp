module;

#include "PCH.hpp"

module UI.ElementAdapter.TatzelElementAdapter;

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::LogicalElement& element ) const -> void
{
	auto* parent_element = element.GetParent();
	auto parent_element_child_root_part = parent_element->GetChildRootPart();
	auto parts = std::vector<dom::ElementPart>{
		dom::ElementPart{ this->MakePartID( element, "base" ), parent_element_child_root_part.GetPartID(), "div" }
	};
	element.SetParts( std::move( parts ), 0, 0 );
}


auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::Heading& element ) const -> void
{
	auto* parent_element = element.GetParent();
	auto parent_element_child_root_part = parent_element->GetChildRootPart();
	auto parts = std::vector<dom::ElementPart>{
		dom::ElementPart{ this->MakePartID( element, "base" ), parent_element_child_root_part.GetPartID(), ui::HeadingStyleToTag( element.GetHeadingStyle() ) }
	};
	parts[ 0 ].text_content = element.GetText();
	element.SetParts( std::move( parts ), 0, 0 );
}


/*

auto tatzel::TatzelElementAdapter::CreateLabel( ui::Label* element ) -> ui::LogicalElement
{
	return { element, {} };
}


auto tatzel::TatzelElementAdapter::CreateContainer( ui::Container* element ) -> ui::LogicalElement
{
	return { element, {} };
}


auto tatzel::TatzelElementAdapter::CreateButton( ui::Button* element ) -> ui::LogicalElement
{
	return { element, {} };
}


auto tatzel::TatzelElementAdapter::CreateHeading( ui::Heading* element ) -> ui::Heading*
{
	return {};
}


auto tatzel::TatzelElementAdapter::CreateParagraph( ui::Paragraph* element ) -> ui::LogicalElement
{
	return { element, {} };
}


auto tatzel::TatzelElementAdapter::CreateSpan( ui::Span* element ) -> ui::LogicalElement
{
	return { element, {} };
}


auto tatzel::TatzelElementAdapter::CreateLink( ui::Link* element ) -> ui::LogicalElement
{
	return { element, {} };
}


auto tatzel::TatzelElementAdapter::CreateImage( ui::Image* element ) -> ui::LogicalElement
{
	return { element, {} };
}


auto tatzel::TatzelElementAdapter::CreateHorizontalRule( ui::HorizontalRule* element ) -> ui::LogicalElement
{
	return { element, {} };
}


auto tatzel::TatzelElementAdapter::CreateModal( ui::Modal* element ) -> ui::LogicalElement
{
	return { element, {} };
}


auto tatzel::TatzelElementAdapter::CreateCard( ui::Card* element ) -> ui::LogicalElement
{
	return { element, {} };
}


auto tatzel::TatzelElementAdapter::CreateHeader( ui::Header* element ) -> ui::LogicalElement
{
	return { element, {} };
}


auto tatzel::TatzelElementAdapter::CreateFooter( ui::Footer* element ) -> ui::LogicalElement
{
	return { element, {} };
}
*/