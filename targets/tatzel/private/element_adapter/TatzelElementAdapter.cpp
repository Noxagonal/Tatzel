module;

#include <string_view>
#include <utility>
#include <vector>
#include <stdexcept>

module UI.ElementAdapter.TatzelElementAdapter;

import UI.DOM.ElementPart;
import UI.ClientUpdater;
import UI.Element.Heading;
import UI.Element.Button;

namespace {

/// @brief Converts a logical heading level at the DOM adapter boundary.
/// @param style Semantic heading level.
/// @return HTML tag for the level.
/// @pre style is a defined HeadingStyle value.
/// @post No logical or browser state is changed.
/// @throws std::invalid_argument If style is not a defined value.
auto HeadingStyleToTag( tatzel::ui::HeadingStyle style ) -> std::string_view
{
	switch( style )
	{
	case tatzel::ui::HeadingStyle::H1: return "h1";
	case tatzel::ui::HeadingStyle::H2: return "h2";
	case tatzel::ui::HeadingStyle::H3: return "h3";
	case tatzel::ui::HeadingStyle::H4: return "h4";
	case tatzel::ui::HeadingStyle::H5: return "h5";
	case tatzel::ui::HeadingStyle::H6: return "h6";
	}
	throw std::invalid_argument{ "Invalid heading style" };
}

}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::LogicalElement& element ) const -> void
{
	auto& parent_part = element.GetParent()->GetChildRootPart();
	element.SetParts( { dom::ElementPart{ MakePartID( element, "base" ), parent_part.GetPartID(), "div" } }, 0, 0 );
}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::Heading& element ) const -> void
{
	auto& parent_part = element.GetParent()->GetChildRootPart();
	auto parts = std::vector<dom::ElementPart>{
		dom::ElementPart{ MakePartID( element, "base" ), parent_part.GetPartID(), HeadingStyleToTag( element.GetHeadingStyle() ) }
	};
	parts.front().SetTextContent( element.GetText() );
	element.SetParts( std::move( parts ), 0, 0 );
	element.SetChangeHandler( [ this, &element ] {
		auto& part = element.GetBasePart();
		part.SetTextContent( element.GetText() );
		client_updater->SetText( part.GetPartID(), element.GetText() );
	} );
}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::Button& element ) const -> void
{
	auto& parent_part = element.GetParent()->GetChildRootPart();
	auto parts = std::vector<dom::ElementPart>{
		dom::ElementPart{ MakePartID( element, "base" ), parent_part.GetPartID(), "button" }
	};
	parts.front().AddAttribute( "type", "button" );
	parts.front().SetTextContent( element.GetText() );
	element.SetParts( std::move( parts ), 0, 0 );
	element.SetChangeHandler( [ this, &element ] {
		auto& part = element.GetBasePart();
		part.SetTextContent( element.GetText() );
		client_updater->SetText( part.GetPartID(), element.GetText() );
	} );
}
