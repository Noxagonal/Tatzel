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
