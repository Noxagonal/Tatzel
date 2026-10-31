module;

#include <string_view>
#include <utility>
#include <vector>
#include <stdexcept>
#include <optional>

module UI.ElementAdapter.TatzelElementAdapter;

import UI.DOM.ElementPart;
import UI.ClientUpdater;
import UI.Element.Heading;
import UI.Element.Button;

namespace {

/// @brief Resolves semantic child placement at the adapter boundary.
/// @param element Logical child whose parent is already mapped.
/// @return The parent's concrete insertion part.
/// @pre element has a nonnull mapped parent.
/// @post Logical and DOM state are unchanged.
/// @throws std::invalid_argument If an actions slot is requested from an unsupported parent.
auto ParentPartFor( tatzel::ui::LogicalElement& element ) -> tatzel::dom::ElementPart&
{
	if( element.GetParentSlot() == tatzel::ui::ChildSlot::Content )
	{
		return element.GetParent()->GetChildRootPart();
	}

	auto* card = dynamic_cast<tatzel::ui::Card*>( element.GetParent() );
	if( !card || !card->HasActions() ) throw std::invalid_argument{ "Parent has no actions slot" };
	return *card->FindPart( "actions" );
}

/// @brief Synchronizes changed plain text without replacing neighboring parts.
/// @param part Adapter-owned browser part.
/// @param text New plain text.
/// @param updater Sender for the same page.
/// @pre The browser part has been created.
/// @post Changed text is stored and a browser update is queued.
auto SyncText(
	tatzel::dom::ElementPart& part,
	std::string_view text,
	tatzel::ClientUpdater& updater
) -> void
{
	if( part.GetTextContent() == text ) return;

	part.SetTextContent( text );
	updater.SetText( part.GetPartID(), text );
}

/// @brief Synchronizes one optional attribute and its stored DOM snapshot.
/// @param part Adapter-owned browser part.
/// @param name HTML attribute name.
/// @param value New value, or no value to remove the attribute.
/// @param updater Sender for the same page.
/// @pre The browser part has been created.
/// @post Only an actual attribute change queues a browser update.
auto SyncAttribute(
	tatzel::dom::ElementPart& part,
	std::string_view name,
	std::optional<std::string_view> value,
	tatzel::ClientUpdater& updater
) -> void
{
	if( part.GetAttribute( name ) == value ) return;

	if( value )
	{
		part.AddAttribute( name, *value );
		updater.SetAttribute( part.GetPartID(), name, *value );
	}
	else
	{
		part.RemoveAttribute( name );
		updater.RemoveAttribute( part.GetPartID(), name );
	}
}

/// @brief Converts a logical boolean into HTML boolean-attribute presence.
/// @param enabled Whether the attribute should be present.
/// @return An empty attribute value when enabled, otherwise no attribute.
/// @pre None.
/// @post No state is changed.
auto BooleanAttribute( bool enabled ) -> std::optional<std::string_view>
{
	return enabled ? std::optional<std::string_view>{ "" } : std::nullopt;
}

/// @brief Maps a plain text control to the adapter's chosen HTML tag.
/// @param element Logical text owner.
/// @param tag Inline or paragraph tag.
/// @param updater Sender that outlives the logical text.
/// @pre The logical parent is mapped and no parts have been assigned.
/// @post Plain text is owned by a part and future changes queue text updates.
auto MapText(
	tatzel::ui::Text& element,
	std::string_view tag,
	tatzel::ClientUpdater& updater
) -> void
{
	auto parts = std::vector<tatzel::dom::ElementPart>{
		{ tatzel::ElementAdapter::MakePartID( element, "base" ), ParentPartFor( element ).GetPartID(), tag }
	};
	parts.front().SetTextContent( element.GetText() );
	parts.front().AddAttribute( "style", "white-space: pre-wrap" );
	element.SetParts( std::move( parts ), 0, 0 );
	element.SetChangeHandler( [ &element, &updater ] {
		SyncText( element.GetBasePart(), element.GetText(), updater );
	} );
}

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
	auto& parent_part = ParentPartFor( element );
	element.SetParts( { dom::ElementPart{ MakePartID( element, "base" ), parent_part.GetPartID(), "div" } }, 0, 0 );
}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::Heading& element ) const -> void
{
	auto& parent_part = ParentPartFor( element );
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
	auto& parent_part = ParentPartFor( element );
	auto parts = std::vector<dom::ElementPart>{
		dom::ElementPart{ MakePartID( element, "base" ), parent_part.GetPartID(), "button" }
	};
	parts.front().AddAttribute( "type", "button" );
	parts.front().AddAttribute( "data-ui-on-click", "true" );
	parts.front().SetTextContent( element.GetText() );
	element.SetParts( std::move( parts ), 0, 0 );
	element.SetChangeHandler( [ this, &element ] {
		auto& part = element.GetBasePart();
		part.SetTextContent( element.GetText() );
		client_updater->SetText( part.GetPartID(), element.GetText() );
	} );
}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::Text& element ) const -> void
{
	MapText( element, "span", *client_updater );
}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::Paragraph& element ) const -> void
{
	MapText( element, "p", *client_updater );
}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::Card& element ) const -> void
{
	auto base_id = MakePartID( element, "base" );
	auto header_id = MakePartID( element, "header" );
	auto parts = std::vector<dom::ElementPart>{
		{ base_id, ParentPartFor( element ).GetPartID(), "article" },
		{ header_id, base_id, "header" },
		{ MakePartID( element, "title" ), header_id, "h2" },
		{ MakePartID( element, "content" ), base_id, "div" },
		{ MakePartID( element, "actions" ), base_id, "footer" }
	};
	parts[ 2 ].SetTextContent( element.GetTitle() );
	if( element.GetTitle().empty() ) parts[ 1 ].AddAttribute( "hidden", "" );
	if( !element.HasActions() ) parts[ 4 ].AddAttribute( "hidden", "" );
	element.SetParts( std::move( parts ), 0, 3 );
	element.SetChangeHandler( [ this, &element ] {
		SyncText( *element.FindPart( "title" ), element.GetTitle(), *client_updater );
		SyncAttribute( *element.FindPart( "header" ), "hidden", BooleanAttribute( element.GetTitle().empty() ), *client_updater );
	} );
}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::TextInput& element ) const -> void
{
	auto base_id = MakePartID( element, "base" );
	auto editor_id = MakePartID( element, "editor" );
	auto parts = std::vector<dom::ElementPart>{
		{ base_id, ParentPartFor( element ).GetPartID(), "div" },
		{ MakePartID( element, "label" ), base_id, "label" },
		{ editor_id, base_id, "input" }
	};
	parts[ 1 ].AddAttribute( "for", editor_id );
	parts[ 1 ].SetTextContent( element.GetLabel() );
	auto& editor = parts[ 2 ];
	editor.AddAttribute( "type", "text" );
	editor.AddAttribute( "value", element.GetValue() );
	editor.AddAttribute( "placeholder", element.GetPlaceholder() );
	editor.AddAttribute( "data-ui-on-input", "true" );
	editor.AddAttribute( "data-ui-on-change", "true" );
	editor.AddAttribute( "data-ui-on-submit", "true" );
	if( element.IsDisabled() ) editor.AddAttribute( "disabled", "" );
	if( element.IsReadOnly() ) editor.AddAttribute( "readonly", "" );
	element.SetParts( std::move( parts ), 0, 0 );

	element.SetChangeHandler( [ this, &element, revision = element.GetValueRevision() ]() mutable {
		SyncText( *element.FindPart( "label" ), element.GetLabel(), *client_updater );
		auto& editor = *element.FindPart( "editor" );
		SyncAttribute( editor, "placeholder", element.GetPlaceholder(), *client_updater );
		SyncAttribute( editor, "disabled", BooleanAttribute( element.IsDisabled() ), *client_updater );
		SyncAttribute( editor, "readonly", BooleanAttribute( element.IsReadOnly() ), *client_updater );

		editor.AddAttribute( "value", element.GetValue() );
		if( revision != element.GetValueRevision() )
		{
			revision = element.GetValueRevision();
			client_updater->SetValue( editor.GetPartID(), element.GetValue() );
		}
	} );
}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::Checkbox& element ) const -> void
{
	auto base_id = MakePartID( element, "base" );
	auto editor_id = MakePartID( element, "editor" );
	auto parts = std::vector<dom::ElementPart>{
		{ base_id, ParentPartFor( element ).GetPartID(), "label" },
		{ editor_id, base_id, "input" },
		{ MakePartID( element, "label" ), base_id, "span" }
	};
	parts[ 0 ].AddAttribute( "for", editor_id );
	parts[ 2 ].SetTextContent( element.GetLabel() );
	auto& editor = parts[ 1 ];
	editor.AddAttribute( "type", "checkbox" );
	editor.AddAttribute( "data-ui-on-change", "true" );
	if( element.IsChecked() ) editor.AddAttribute( "checked", "" );
	if( element.IsDisabled() ) editor.AddAttribute( "disabled", "" );
	element.SetParts( std::move( parts ), 0, 0 );

	element.SetChangeHandler( [ this, &element, revision = element.GetValueRevision() ]() mutable {
		SyncText( *element.FindPart( "label" ), element.GetLabel(), *client_updater );
		auto& editor = *element.FindPart( "editor" );
		SyncAttribute( editor, "disabled", BooleanAttribute( element.IsDisabled() ), *client_updater );
		if( element.IsChecked() ) editor.AddAttribute( "checked", "" );
		else editor.RemoveAttribute( "checked" );

		if( revision != element.GetValueRevision() )
		{
			revision = element.GetValueRevision();
			client_updater->SetValue( editor.GetPartID(), element.IsChecked() ? "true" : "false" );
		}
	} );
}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::Modal& element ) const -> void
{
	auto base_id = MakePartID( element, "base" );
	auto panel_id = MakePartID( element, "panel" );
	auto header_id = MakePartID( element, "header" );
	auto title_id = MakePartID( element, "title" );
	auto parts = std::vector<dom::ElementPart>{
		{ base_id, ParentPartFor( element ).GetPartID(), "dialog" },
		{ panel_id, base_id, "article" },
		{ header_id, panel_id, "header" },
		{ title_id, header_id, "h2" },
		{ MakePartID( element, "dismiss" ), header_id, "button" },
		{ MakePartID( element, "content" ), panel_id, "div" }
	};
	parts[ 0 ].AddAttribute( "aria-labelledby", title_id );
	parts[ 0 ].AddAttribute( "data-ui-modal", "true" );
	parts[ 0 ].AddAttribute( "data-ui-modal-open", "false" );
	parts[ 0 ].AddAttribute( "data-ui-on-dismiss", "true" );
	parts[ 3 ].SetTextContent( element.GetTitle() );
	parts[ 4 ].AddAttribute( "type", "button" );
	parts[ 4 ].AddAttribute( "aria-label", "Close" );
	parts[ 4 ].AddAttribute( "data-ui-on-dismiss", "true" );
	parts[ 4 ].SetTextContent( "×" );
	element.SetParts( std::move( parts ), 0, 5 );

	element.SetChangeHandler( [ this, &element, open = false ]() mutable {
		SyncText( *element.FindPart( "title" ), element.GetTitle(), *client_updater );
		if( open == element.IsOpen() ) return;

		open = element.IsOpen();
		element.GetBasePart().AddAttribute( "data-ui-modal-open", open ? "true" : "false" );
		client_updater->SetModalOpen( element.GetBasePart().GetPartID(), open );
	} );
}
