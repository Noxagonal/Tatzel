/// @file
/// @brief Default HTML mapping for supported logical controls.
module;

export module UI.ElementAdapter.TatzelElementAdapter;

export import UI.ElementAdapter.ElementAdapter;
import UI.ClientUpdater;

namespace tatzel {

/// @brief Default adapter implementing the logical control to HTML mapping.
export class TatzelElementAdapter final : public ElementAdapter
{
public:
	/// @brief Binds the DOM protocol sender used by logical updates.
	/// @param client_updater Sender for the same client page as the mapped controls.
	/// @pre client_updater outlives this adapter and its bound controls.
	/// @post New mappings synchronize through client_updater.
	explicit TatzelElementAdapter( ClientUpdater& client_updater ) :
		client_updater{ &client_updater }
	{}

	/// @brief Maps a generic logical control to a DOM container.
	/// @param element Logical control to map.
	/// @pre The logical parent is mapped and the control has no parts.
	/// @post The control owns one div part that receives its children.
	auto CreatePartsFor( ui::LogicalElement& element ) const -> void override;

	/// @brief Maps a logical heading to a semantic HTML heading.
	/// @param element Logical heading to map.
	/// @pre The parent is mapped; the heading level is valid; no parts are assigned.
	/// @post The heading owns one heading part and logical text changes send DOM updates.
	auto CreatePartsFor( ui::Heading& element ) const -> void override;

	/// @brief Maps a logical button to an HTML button.
	/// @param element Logical button to map.
	/// @pre The logical parent is mapped and the button has no parts.
	/// @post The button owns one button part and logical label changes send DOM updates.
	auto CreatePartsFor( ui::Button& element ) const -> void override;

	/// @brief Maps a logical Text and binds its supported state updates.
	/// @param element Logical control to map.
	/// @pre The parent and insertion slot are mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	auto CreatePartsFor( ui::Text& element ) const -> void override;

	/// @brief Maps a logical Paragraph and binds its supported state updates.
	/// @param element Logical control to map.
	/// @pre The parent and insertion slot are mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	auto CreatePartsFor( ui::Paragraph& element ) const -> void override;

	/// @brief Maps a logical Card and binds its supported state updates.
	/// @param element Logical control to map.
	/// @pre The parent and insertion slot are mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	auto CreatePartsFor( ui::Card& element ) const -> void override;

	/// @brief Maps a logical TextInput and binds its supported state updates.
	/// @param element Logical control to map.
	/// @pre The parent and insertion slot are mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	auto CreatePartsFor( ui::TextInput& element ) const -> void override;

	/// @brief Maps a logical Checkbox and binds its supported state updates.
	/// @param element Logical control to map.
	/// @pre The parent and insertion slot are mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	auto CreatePartsFor( ui::Checkbox& element ) const -> void override;

	/// @brief Maps a logical Modal and binds its supported state updates.
	/// @param element Logical control to map.
	/// @pre The parent and insertion slot are mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	auto CreatePartsFor( ui::Modal& element ) const -> void override;

private:
	ClientUpdater* client_updater;
};

}
