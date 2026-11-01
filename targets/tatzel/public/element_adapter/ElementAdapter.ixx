/// @file
/// @brief Framework boundary between logical controls and concrete DOM parts.
module;

#include <string>
#include <string_view>

export module UI.ElementAdapter.ElementAdapter;

export import UI.Element.LogicalElement;
export import UI.Element.Heading;
export import UI.Element.Button;
export import UI.Element.Container;
export import UI.Element.Text;
export import UI.Element.Paragraph;
export import UI.Element.Card;
export import UI.Element.TextInput;
export import UI.Element.Checkbox;
export import UI.Element.Modal;
export import UI.Element.Row;
export import UI.Element.Column;
export import UI.Element.Select;
export import UI.Element.TextArea;
export import UI.Element.Form;
export import UI.Element.NumberInput;
export import UI.Element.RadioGroup;
export import UI.Element.Link;
export import UI.Element.Image;


namespace tatzel {

/// @brief Maps logical controls to browser DOM parts and binds logical update notifications.
export class ElementAdapter
{
public:
	/// @brief Releases adapter services.
	/// @pre Bound logical controls will no longer invoke this adapter.
	/// @post Adapter resources are released.
	virtual ~ElementAdapter() = default;

	/// @brief Creates a generic logical control's concrete DOM representation.
	/// @param element Logical control to map.
	/// @pre The control has a mapped logical parent and no assigned parts.
	/// @post The control owns its parts and future logical changes are bound as supported.
	virtual auto CreatePartsFor( ui::LogicalElement& element ) const -> void = 0;

	/// @brief Maps a logical heading and binds its text updates.
	/// @param element Logical heading to map.
	/// @pre The heading has a mapped logical parent and no assigned parts.
	/// @post The heading owns its concrete parts and its setter updates the browser.
	virtual auto CreatePartsFor( ui::Heading& element ) const -> void = 0;

	/// @brief Maps a logical button and binds its label updates.
	/// @param element Logical button to map.
	/// @pre The button has a mapped logical parent and no assigned parts.
	/// @post The button owns its concrete parts and its setter updates the browser.
	virtual auto CreatePartsFor( ui::Button& element ) const -> void = 0;

	/// @brief Maps a logical Text and binds its supported state updates.
	/// @param element Logical control to map.
	/// @pre The parent and insertion slot are mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	virtual auto CreatePartsFor( ui::Text& element ) const -> void = 0;

	/// @brief Maps a logical Paragraph and binds its supported state updates.
	/// @param element Logical control to map.
	/// @pre The parent and insertion slot are mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	virtual auto CreatePartsFor( ui::Paragraph& element ) const -> void = 0;

	/// @brief Maps a logical Card and binds its supported state updates.
	/// @param element Logical control to map.
	/// @pre The parent and insertion slot are mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	virtual auto CreatePartsFor( ui::Card& element ) const -> void = 0;

	/// @brief Maps a logical TextInput and binds its supported state updates.
	/// @param element Logical control to map.
	/// @pre The parent and insertion slot are mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	virtual auto CreatePartsFor( ui::TextInput& element ) const -> void = 0;

	/// @brief Maps a logical Checkbox and binds its supported state updates.
	/// @param element Logical control to map.
	/// @pre The parent and insertion slot are mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	virtual auto CreatePartsFor( ui::Checkbox& element ) const -> void = 0;

	/// @brief Maps a logical Modal and binds its supported state updates.
	/// @param element Logical control to map.
	/// @pre The parent and insertion slot are mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	virtual auto CreatePartsFor( ui::Modal& element ) const -> void = 0;

	/// @brief Maps a logical Row and binds its supported state updates.
	/// @param element Logical control to map.
	/// @pre The parent and insertion slot are mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	virtual auto CreatePartsFor( ui::Row& element ) const -> void = 0;

	/// @brief Maps a logical Column and binds its supported state updates.
	/// @param element Logical control to map.
	/// @pre The parent and insertion slot are mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	virtual auto CreatePartsFor( ui::Column& element ) const -> void = 0;

	/// @brief Maps a logical Select and binds its supported state updates.
	/// @param element Logical control to map.
	/// @pre The parent and insertion slot are mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	virtual auto CreatePartsFor( ui::Select& element ) const -> void = 0;

	/// @brief Maps a logical TextArea and binds its supported state updates.
	/// @param element Logical control to map.
	/// @pre The parent and insertion slot are mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	virtual auto CreatePartsFor( ui::TextArea& element ) const -> void = 0;

	/// @brief Maps a logical Form and binds its supported state updates.
	/// @param element Logical control to map.
	/// @pre The parent and insertion slot are mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	virtual auto CreatePartsFor( ui::Form& element ) const -> void = 0;

	/// @brief Maps a logical NumberInput and binds its supported updates.
	/// @param element Logical control to map.
	/// @pre The parent is mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	virtual auto CreatePartsFor( ui::NumberInput& element ) const -> void = 0;

	/// @brief Maps a logical RadioGroup and binds its supported updates.
	/// @param element Logical control to map.
	/// @pre The parent is mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	virtual auto CreatePartsFor( ui::RadioGroup& element ) const -> void = 0;

	/// @brief Maps a logical Link and binds its supported updates.
	/// @param element Logical control to map.
	/// @pre The parent is mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	virtual auto CreatePartsFor( ui::Link& element ) const -> void = 0;

	/// @brief Maps a logical Image and binds its supported updates.
	/// @param element Logical control to map.
	/// @pre The parent is mapped; no parts have been assigned.
	/// @post The control owns its DOM representation and is bound to the adapter.
	virtual auto CreatePartsFor( ui::Image& element ) const -> void = 0;

	/// @brief Queues control-specific initialization after browser parts are created.
	/// @param element Newly mapped logical control.
	/// @pre DOM creation for element's parts is already queued on the same page.
	/// @post Any initial native-control state is queued in creation order.
	virtual auto InitializePartsFor( ui::LogicalElement& element ) const -> void {}

	/// @brief Constructs a browser part ID from a logical ID and a local part name.
	/// @param element Logical owner of the browser part.
	/// @param name Adapter-local part name.
	/// @return Owned browser ID in logical-id|part-name form.
	/// @pre name is nonempty and unique within this logical control.
	/// @post The logical control is unchanged.
	static auto MakePartID(
		const ui::LogicalElement& element,
		std::string_view name
	) -> std::string
	{
		return std::string{ element.GetID() } + "|" + std::string{ name };
	}
};

}
