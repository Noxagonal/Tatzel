#include <cassert>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

import UI.DOM.ElementPart;
import UI.Element.Heading;
import UI.Element.Button;
import UI.ClientElementTree;

/// @brief Checks owned DOM strings and the distinction between logical and part identity.
/// @return Zero when all core assertions pass.
/// @pre The Tatzel module interfaces and library have been built.
/// @post Temporary trees and controls have been destroyed.
auto main() -> int
{
	auto part = tatzel::dom::ElementPart{ "logical|base", "root|main", "h1" };
	{
		auto temporary = std::string( 1024, 'v' );
		part.AddAttribute( "title", temporary );
	}
	assert( part.GetAttributes().front().second == std::string( 1024, 'v' ) );
	part.AddAttribute( "title", "Replacement" );
	assert( part.GetAttributes().size() == 1 && part.GetAttribute( "title" ) == "Replacement" );
	part.RemoveAttribute( "title" );
	assert( !part.GetAttribute( "title" ) );

	auto tree = tatzel::ClientElementTree{};
	auto heading = std::make_unique<tatzel::ui::Heading>(
		"logical-heading", &tree.root, "Before", tatzel::ui::HeadingStyle::H1 );
	heading->SetParts( {
		tatzel::dom::ElementPart{ "logical-heading|base", "root|main", "h1" },
		tatzel::dom::ElementPart{ "logical-heading|caption", "logical-heading|base", "span" }
	}, 0, 0 );
	auto* borrowed_heading = tree.root.InsertChild( std::move( heading ) );

	assert( tree.FindElementByID<tatzel::ui::Heading>( "logical-heading" ) == borrowed_heading );
	assert( tree.FindElementByID<tatzel::ui::Heading>( "logical-heading|base" ) == nullptr );
	assert( tree.FindElementByPartID<tatzel::ui::Heading>( "logical-heading|caption" ) == borrowed_heading );
	assert( tree.FindElementByPartID<tatzel::ui::Heading>( "missing" ) == nullptr );

	auto updates = 0;
	borrowed_heading->SetChangeHandler( [ &updates ] { ++updates; } );
	borrowed_heading->SetText( "After" );
	assert( borrowed_heading->GetText() == "After" && updates == 1 );
	borrowed_heading->SetText( "After" );
	assert( updates == 1 );

	auto clicks = 0;
	auto button = tatzel::ui::Button{ "logical-button", &tree.root, "Click", [ &clicks ] { ++clicks; } };
	button.InvokeOnClick();
	assert( clicks == 1 );
	return 0;
}
