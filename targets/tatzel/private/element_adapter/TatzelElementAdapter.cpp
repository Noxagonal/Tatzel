module;

#include <string_view>
#include <format>
#include <utility>
#include <vector>
#include <optional>
#include <array>
#include <cstddef>
#include <string>

module UI.ElementAdapter.TatzelElementAdapter;

import UI.Diagnostics;

import UI.DOM.ElementPart;
import UI.DOM.SelectOption;
import UI.ClientUpdater;
import UI.Element.Heading;
import UI.Element.Button;

namespace {

/// @brief Resolves semantic child placement at the adapter boundary.
/// @param element Logical child whose parent is already mapped.
/// @return The parent's concrete insertion part.
/// @pre element has a nonnull mapped parent.
/// @post Logical and DOM state are unchanged.
/// @throws tatzel::Exception If an actions slot is requested from an unsupported parent.
auto ParentPartFor( tatzel::ui::LogicalElement& element ) -> tatzel::dom::ElementPart&
{
	if( element.GetParentSlot() == tatzel::ui::ChildSlot::Content )
	{
		return element.GetParent()->GetChildRootPart();
	}

	auto* card = dynamic_cast<tatzel::ui::Card*>( element.GetParent() );
	if( !card || !card->HasActions() ) throw tatzel::Exception{ "Parent has no actions slot" };
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


/// @brief Initializes shared field labels, error parts, and editor attributes.
/// @param field Logical field whose parts are being mapped.
/// @param label Text-bearing label part.
/// @param editor Native form-control part.
/// @param error Dedicated error-message part.
/// @pre The parts have not yet been sent to the browser.
/// @post Parts store initial accessible field state without sending operations.
auto InitializeFieldState(
	tatzel::ui::FormField& field,
	tatzel::dom::ElementPart& label,
	tatzel::dom::ElementPart& editor,
	tatzel::dom::ElementPart& error
) -> void
{
	label.SetTextContent( field.GetLabel() );
	editor.AddAttribute( "data-ui-form-field", "true" );
	editor.AddAttribute( "data-ui-on-change", "true" );
	editor.AddAttribute( "aria-describedby", error.GetPartID() );
	if( field.IsDisabled() ) editor.AddAttribute( "disabled", "" );
	if( field.IsRequired() ) editor.AddAttribute( "required", "" );
	if( !field.GetError().empty() ) editor.AddAttribute( "aria-invalid", "true" );

	error.AddAttribute( "aria-live", "polite" );
	error.AddAttribute( "style", "color: var(--pico-del-color, #b42318)" );
	error.SetTextContent( field.GetError() );
	if( field.GetError().empty() ) error.AddAttribute( "hidden", "" );
}

/// @brief Synchronizes shared field state without replacing native editor values.
/// @param field Mapped logical field.
/// @param updater Sender for the same page.
/// @pre Label, editor, and error parts have been created.
/// @post Changed label/error text and accessibility flags are queued.
auto SyncFieldState(
	tatzel::ui::FormField& field,
	tatzel::ClientUpdater& updater
) -> void
{
	auto& editor = *field.FindPart( "editor" );
	auto& error = *field.FindPart( "error" );
	SyncText( *field.FindPart( "label" ), field.GetLabel(), updater );
	SyncText( error, field.GetError(), updater );
	SyncAttribute( editor, "disabled", BooleanAttribute( field.IsDisabled() ), updater );
	SyncAttribute( editor, "required", BooleanAttribute( field.IsRequired() ), updater );
	if( dynamic_cast<tatzel::ui::RadioGroup*>( &field ) )
		SyncAttribute( editor, "aria-required", field.IsRequired() ? "true" : "false", updater );
	SyncAttribute( editor, "aria-invalid", field.GetError().empty()
		? std::nullopt : std::optional<std::string_view>{ "true" }, updater );
	SyncAttribute( error, "hidden", BooleanAttribute( field.GetError().empty() ), updater );
}

/// @brief Maps shared single-line and multiline text-field behavior.
/// @param element Logical editor whose value API is shared with TextInput.
/// @param updater Sender that outlives the field.
/// @pre The parent is mapped and the editor has no parts.
/// @post The native editor and error parts are stored and explicit setters synchronize.
auto MapTextEditor(
	tatzel::ui::TextInput& element,
	tatzel::ClientUpdater& updater
) -> void
{
	auto* area = dynamic_cast<tatzel::ui::TextArea*>( &element );
	auto* number = dynamic_cast<tatzel::ui::NumberInput*>( &element );
	auto base_id = tatzel::ElementAdapter::MakePartID( element, "base" );
	auto editor_id = tatzel::ElementAdapter::MakePartID( element, "editor" );
	auto parts = std::vector<tatzel::dom::ElementPart>{
		{ base_id, ParentPartFor( element ).GetPartID(), "div" },
		{ tatzel::ElementAdapter::MakePartID( element, "label" ), base_id, "label" },
		{ editor_id, base_id, area ? "textarea" : "input" },
		{ tatzel::ElementAdapter::MakePartID( element, "error" ), base_id, "small" }
	};
	parts[ 1 ].AddAttribute( "for", editor_id );
	InitializeFieldState( element, parts[ 1 ], parts[ 2 ], parts[ 3 ] );
	auto& editor = parts[ 2 ];
	if( area )
	{
		editor.SetTextContent( element.GetValue() );
		editor.AddAttribute( "rows", std::to_string( area->GetRows() ) );
	}
	else
	{
		editor.AddAttribute( "type", number ? "number" : "text" );
		if( !number ) editor.AddAttribute( "value", element.GetValue() );
		else
		{
			if( number->GetMinimum() ) editor.AddAttribute( "min", std::format( "{}", *number->GetMinimum() ) );
			if( number->GetMaximum() ) editor.AddAttribute( "max", std::format( "{}", *number->GetMaximum() ) );
			editor.AddAttribute( "step", number->GetStep() ? std::format( "{}", *number->GetStep() ) : "any" );
		}
	}
	editor.AddAttribute( "placeholder", element.GetPlaceholder() );
	editor.AddAttribute( "data-ui-on-input", "true" );
	editor.AddAttribute( "data-ui-on-submit", "true" );
	if( element.IsReadOnly() ) editor.AddAttribute( "readonly", "" );
	element.SetParts( std::move( parts ), 0, 0 );

	element.SetChangeHandler( [ &element, &updater, area, number, revision = element.GetValueRevision() ]() mutable {
		SyncFieldState( element, updater );
		auto& editor = *element.FindPart( "editor" );
		SyncAttribute( editor, "placeholder", element.GetPlaceholder(), updater );
		SyncAttribute( editor, "readonly", BooleanAttribute( element.IsReadOnly() ), updater );
		if( area )
		{
			SyncAttribute( editor, "rows", std::to_string( area->GetRows() ), updater );
			editor.SetTextContent( element.GetValue() );
		}
		else if( !number ) editor.AddAttribute( "value", element.GetValue() );
		if( number )
		{
			const auto minimum = number->GetMinimum() ? std::format( "{}", *number->GetMinimum() ) : std::string{};
			const auto maximum = number->GetMaximum() ? std::format( "{}", *number->GetMaximum() ) : std::string{};
			SyncAttribute( editor, "min", number->GetMinimum() ? std::optional<std::string_view>{ minimum } : std::nullopt, updater );
			SyncAttribute( editor, "max", number->GetMaximum() ? std::optional<std::string_view>{ maximum } : std::nullopt, updater );
			SyncAttribute( editor, "step", number->GetStep() ? std::format( "{}", *number->GetStep() ) : "any", updater );
		}

		if( revision != element.GetValueRevision() )
		{
			revision = element.GetValueRevision();
			updater.SetValue( editor.GetPartID(), element.GetValue() );
		}
	} );
}

/// @brief Converts semantic layout settings into adapter-owned CSS.
/// @param layout Logical row or column.
/// @return Owned CSS declaring the linear arrangement.
/// @pre All enum settings are defined values.
/// @post No state is changed.
auto LayoutToStyle( const tatzel::ui::Layout& layout ) -> std::string
{
	const auto gaps = std::array{ "0", "0.5rem", "1rem", "1.5rem" };
	const auto alignments = std::array{ "flex-start", "center", "flex-end", "stretch" };
	const auto justifications = std::array{ "flex-start", "center", "flex-end", "space-between", "space-around", "space-evenly" };

	return std::string{ "display:flex;min-width:0;flex-direction:" }
		+ ( layout.GetDirection() == tatzel::ui::LayoutDirection::Row ? "row" : "column" )
		+ ";gap:" + gaps.at( static_cast<size_t>( layout.GetGap() ) )
		+ ";align-items:" + alignments.at( static_cast<size_t>( layout.GetAlignment() ) )
		+ ";justify-content:" + justifications.at( static_cast<size_t>( layout.GetJustification() ) )
		+ ";flex-wrap:" + ( layout.GetWrap() ? "wrap" : "nowrap" );
}

/// @brief Maps a logical linear layout and binds setting changes.
/// @param layout Logical row or column.
/// @param updater Sender that outlives the layout.
/// @pre The parent is mapped; layout has no parts.
/// @post One child-bearing part is stored and future layout changes queue styling updates.
auto MapLayout(
	tatzel::ui::Layout& layout,
	tatzel::ClientUpdater& updater
) -> void
{
	auto parts = std::vector<tatzel::dom::ElementPart>{
		{ tatzel::ElementAdapter::MakePartID( layout, "base" ), ParentPartFor( layout ).GetPartID(), "div" }
	};
	parts.front().AddAttribute( "style", LayoutToStyle( layout ) );
	parts.front().AddClass( layout.GetDirection() == tatzel::ui::LayoutDirection::Row ? "tatzel-row" : "tatzel-column" );
	layout.SetParts( std::move( parts ), 0, 0 );
	layout.SetChangeHandler( [ &layout, &updater ] {
		SyncAttribute( layout.GetBasePart(), "style", LayoutToStyle( layout ), updater );
	} );
}

/// @brief Queues a native single-choice field's options and selected state.
/// @param select Logical selector or radio group with mapped editor parts.
/// @param updater Sender for the same page.
/// @pre Editor DOM creation has been queued.
/// @post Native choices and current selection are queued as one operation.
auto SendChoiceOptions(
	tatzel::ui::Select& select,
	tatzel::ClientUpdater& updater
) -> void
{
	auto options = std::vector<tatzel::dom::SelectOption>{};
	for( const auto& option : select.GetOptions() ) options.push_back( { option.id, option.label, option.disabled } );
	if( dynamic_cast<tatzel::ui::RadioGroup*>( &select ) )
		updater.SetRadioOptions( select.FindPart( "editor" )->GetPartID(), options, select.GetValue() );
	else updater.SetSelectOptions( select.FindPart( "editor" )->GetPartID(), options, select.GetPlaceholder(), select.GetValue() );
}

/// @brief Converts a logical heading level at the DOM adapter boundary.
/// @param style Semantic heading level.
/// @return HTML tag for the level.
/// @pre style is a defined HeadingStyle value.
/// @post No logical or browser state is changed.
/// @throws tatzel::Exception If style is not a defined value.
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
	throw tatzel::Exception{ "Invalid heading style" };
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
	parts.front().AddAttribute( "type", element.GetRole() == ui::ButtonRole::Submit ? "submit" : "button" );
	if( element.GetRole() == ui::ButtonRole::Action ) parts.front().AddAttribute( "data-ui-on-click", "true" );
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
	MapTextEditor( element, *client_updater );
}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::TextArea& element ) const -> void
{
	MapTextEditor( element, *client_updater );
}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::Checkbox& element ) const -> void
{
	auto base_id = MakePartID( element, "base" );
	auto control_id = MakePartID( element, "control" );
	auto editor_id = MakePartID( element, "editor" );
	auto parts = std::vector<dom::ElementPart>{
		{ base_id, ParentPartFor( element ).GetPartID(), "div" },
		{ control_id, base_id, "label" },
		{ editor_id, control_id, "input" },
		{ MakePartID( element, "label" ), control_id, "span" },
		{ MakePartID( element, "error" ), base_id, "small" }
	};
	parts[ 1 ].AddAttribute( "for", editor_id );
	InitializeFieldState( element, parts[ 3 ], parts[ 2 ], parts[ 4 ] );
	parts[ 2 ].AddAttribute( "type", "checkbox" );
	if( element.IsChecked() ) parts[ 2 ].AddAttribute( "checked", "" );
	element.SetParts( std::move( parts ), 0, 0 );

	element.SetChangeHandler( [ this, &element, revision = element.GetValueRevision() ]() mutable {
		SyncFieldState( element, *client_updater );
		auto& editor = *element.FindPart( "editor" );
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


auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::Row& element ) const -> void
{
	MapLayout( element, *client_updater );
}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::Column& element ) const -> void
{
	MapLayout( element, *client_updater );
}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::Select& element ) const -> void
{
	auto base_id = MakePartID( element, "base" );
	auto editor_id = MakePartID( element, "editor" );
	auto parts = std::vector<dom::ElementPart>{
		{ base_id, ParentPartFor( element ).GetPartID(), "div" },
		{ MakePartID( element, "label" ), base_id, "label" },
		{ editor_id, base_id, "select" },
		{ MakePartID( element, "error" ), base_id, "small" }
	};
	parts[ 1 ].AddAttribute( "for", editor_id );
	InitializeFieldState( element, parts[ 1 ], parts[ 2 ], parts[ 3 ] );
	element.SetParts( std::move( parts ), 0, 0 );

	element.SetChangeHandler( [ this, &element, value_revision = element.GetValueRevision(),
		options_revision = element.GetOptionsRevision() ]() mutable {
		SyncFieldState( element, *client_updater );
		if( options_revision != element.GetOptionsRevision() )
		{
			options_revision = element.GetOptionsRevision();
			value_revision = element.GetValueRevision();
			SendChoiceOptions( element, *client_updater );
		}
		else if( value_revision != element.GetValueRevision() )
		{
			value_revision = element.GetValueRevision();
			client_updater->SetValue( element.FindPart( "editor" )->GetPartID(), element.GetValue() );
		}
	} );
}

auto tatzel::TatzelElementAdapter::InitializePartsFor( ui::LogicalElement& element ) const -> void
{
	if( auto* select = dynamic_cast<ui::Select*>( &element ) ) SendChoiceOptions( *select, *client_updater );
	if( auto* number = dynamic_cast<ui::NumberInput*>( &element ) )
		client_updater->SetValue( number->FindPart( "editor" )->GetPartID(), number->GetTextValue() );
}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::Form& element ) const -> void
{
	auto base_id = MakePartID( element, "base" );
	auto parts = std::vector<dom::ElementPart>{
		{ base_id, ParentPartFor( element ).GetPartID(), "form" },
		{ MakePartID( element, "content" ), base_id, "fieldset" }
	};
	parts[ 0 ].AddAttribute( "novalidate", "" );
	parts[ 0 ].AddAttribute( "data-ui-on-submit", "true" );
	if( !element.GetLabel().empty() ) parts[ 0 ].AddAttribute( "aria-label", element.GetLabel() );
	parts[ 1 ].AddAttribute( "style", "margin:0;padding:0;border:0;min-width:0" );
	if( element.IsDisabled() ) parts[ 1 ].AddAttribute( "disabled", "" );
	element.SetParts( std::move( parts ), 0, 1 );
	element.SetChangeHandler( [ this, &element ] {
		SyncAttribute( element.GetBasePart(), "aria-label", element.GetLabel().empty()
			? std::nullopt : std::optional<std::string_view>{ element.GetLabel() }, *client_updater );
		SyncAttribute( *element.FindPart( "content" ), "disabled", BooleanAttribute( element.IsDisabled() ), *client_updater );
	} );
}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::RadioGroup& element ) const -> void
{
	auto base_id = MakePartID( element, "base" );
	auto editor_id = MakePartID( element, "editor" );
	auto parts = std::vector<dom::ElementPart>{
		{ base_id, ParentPartFor( element ).GetPartID(), "div" },
		{ editor_id, base_id, "fieldset" },
		{ MakePartID( element, "label" ), editor_id, "legend" },
		{ MakePartID( element, "options" ), editor_id, "div" },
		{ MakePartID( element, "error" ), base_id, "small" }
	};
	InitializeFieldState( element, parts[ 2 ], parts[ 1 ], parts[ 4 ] );
	parts[ 1 ].AddAttribute( "data-ui-radio-group", "true" );
	parts[ 1 ].AddAttribute( "role", "radiogroup" );
	parts[ 1 ].AddAttribute( "aria-required", element.IsRequired() ? "true" : "false" );
	parts[ 1 ].AddAttribute( "aria-labelledby", parts[ 2 ].GetPartID() );
	parts[ 1 ].AddAttribute( "tabindex", "-1" );
	parts[ 3 ].AddAttribute( "data-ui-radio-options", "true" );

	element.SetParts( std::move( parts ), 0, 0 );

	element.SetChangeHandler( [ this, &element, value_revision = element.GetValueRevision(),
		options_revision = element.GetOptionsRevision() ]() mutable {
		SyncFieldState( element, *client_updater );
		if( options_revision != element.GetOptionsRevision() )
		{
			options_revision = element.GetOptionsRevision();
			value_revision = element.GetValueRevision();
			SendChoiceOptions( element, *client_updater );
		}
		else if( value_revision != element.GetValueRevision() )
		{
			value_revision = element.GetValueRevision();
			client_updater->SetValue( element.FindPart( "editor" )->GetPartID(), element.GetValue() );
		}
	} );
}


auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::NumberInput& element ) const -> void
{
	MapTextEditor( element, *client_updater );
}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::Link& element ) const -> void
{
	auto parts = std::vector<dom::ElementPart>{
		{ MakePartID( element, "base" ), ParentPartFor( element ).GetPartID(), "a" }
	};
	parts[ 0 ].SetTextContent( element.GetText() );
	parts[ 0 ].AddAttribute( "href", element.GetURL() );
	if( element.GetTarget() == ui::LinkTarget::NewTab )
	{
		parts[ 0 ].AddAttribute( "target", "_blank" );
		parts[ 0 ].AddAttribute( "rel", "noopener noreferrer" );
	}
	element.SetParts( std::move( parts ), 0, 0 );
	element.SetChangeHandler( [ this, &element ] {
		auto& part = element.GetBasePart();
		SyncText( part, element.GetText(), *client_updater );
		SyncAttribute( part, "href", element.GetURL(), *client_updater );
		const auto new_tab = element.GetTarget() == ui::LinkTarget::NewTab;
		SyncAttribute( part, "target", new_tab ? std::optional<std::string_view>{ "_blank" } : std::nullopt, *client_updater );
		SyncAttribute( part, "rel", new_tab ? std::optional<std::string_view>{ "noopener noreferrer" } : std::nullopt, *client_updater );
	} );
}

auto tatzel::TatzelElementAdapter::CreatePartsFor( ui::Image& element ) const -> void
{
	auto parts = std::vector<dom::ElementPart>{
		{ MakePartID( element, "base" ), ParentPartFor( element ).GetPartID(), "img" }
	};
	auto& part = parts[ 0 ];
	if( !element.GetSource().empty() ) part.AddAttribute( "src", element.GetSource() );
	part.AddAttribute( "alt", element.GetAlternativeText() );
	part.AddAttribute( "loading", element.IsLazyLoading() ? "lazy" : "eager" );
	if( element.GetWidth() ) part.AddAttribute( "width", std::to_string( *element.GetWidth() ) );
	if( element.GetHeight() ) part.AddAttribute( "height", std::to_string( *element.GetHeight() ) );
	element.SetParts( std::move( parts ), 0, 0 );
	element.SetChangeHandler( [ this, &element ] {
		auto& part = element.GetBasePart();
		SyncAttribute( part, "src", element.GetSource().empty() ? std::nullopt : std::optional<std::string_view>{ element.GetSource() }, *client_updater );
		SyncAttribute( part, "alt", element.GetAlternativeText(), *client_updater );
		SyncAttribute( part, "loading", element.IsLazyLoading() ? "lazy" : "eager", *client_updater );
		const auto width = element.GetWidth() ? std::to_string( *element.GetWidth() ) : std::string{};
		const auto height = element.GetHeight() ? std::to_string( *element.GetHeight() ) : std::string{};
		SyncAttribute( part, "width", element.GetWidth() ? std::optional<std::string_view>{ width } : std::nullopt, *client_updater );
		SyncAttribute( part, "height", element.GetHeight() ? std::optional<std::string_view>{ height } : std::nullopt, *client_updater );
	} );
}
