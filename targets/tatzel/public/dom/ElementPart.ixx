/// @file
/// @brief Internal representation of one concrete browser DOM element.
module;

#include <span>
#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module UI.DOM.ElementPart;

namespace tatzel::dom {

/// @brief Owns the initial DOM state for one part of a logical element.
/// @details Part IDs identify browser nodes; they are distinct from logical IDs.
/// All strings are owned. Returned views remain valid until the part is mutated
/// or destroyed. Applications normally work with logical controls instead.
export class ElementPart
{
public:

	/// @brief An owned HTML attribute name and value.
	using Attribute = std::pair<std::string, std::string>;

	/// @brief Copies the initial state of a concrete DOM node.
	/// @param part_id Unique browser node ID.
	/// @param parent_id Browser ID of the parent; empty for the template root.
	/// @param tag HTML tag chosen by the element adapter.
	/// @param classes Initial CSS class names, copied before returning.
	/// @param attributes Initial HTML attributes, copied before returning.
	/// @pre part_id and tag are nonempty; part_id is unique within the client page.
	/// @post The part owns all supplied strings and has empty text content.
	ElementPart(
		std::string_view part_id,
		std::string_view parent_id,
		std::string_view tag,
		std::span<const std::string_view> classes = {},
		std::span<const Attribute> attributes = {}
	) :
		part_id{ part_id },
		parent_id{ parent_id },
		tag{ tag },
		classes{ classes.begin(), classes.end() },
		attributes{ attributes.begin(), attributes.end() }
	{}

	/// @brief Reads the browser node ID.
	/// @return Borrowed part ID.
	/// @pre None.
	/// @post State is unchanged.
	auto GetPartID() const noexcept -> std::string_view { return part_id; }

	/// @brief Reads the browser parent ID.
	/// @return Borrowed parent ID.
	/// @pre None.
	/// @post State is unchanged.
	auto GetParentID() const noexcept -> std::string_view { return parent_id; }

	/// @brief Reads the HTML tag.
	/// @return Borrowed tag.
	/// @pre None.
	/// @post State is unchanged.
	auto GetTag() const noexcept -> std::string_view { return tag; }

	/// @brief Reads initial CSS classes.
	/// @return Borrowed class list.
	/// @pre None.
	/// @post State is unchanged.
	auto GetClasses() const noexcept -> std::span<const std::string> { return classes; }

	/// @brief Reads initial HTML attributes.
	/// @return Borrowed attribute list.
	/// @pre None.
	/// @post State is unchanged.
	auto GetAttributes() const noexcept -> std::span<const Attribute> { return attributes; }

	/// @brief Reads one stored HTML attribute.
	/// @param attribute Attribute name to find.
	/// @return Borrowed value, or no value if absent; mutation invalidates the view.
	/// @pre None.
	/// @post State is unchanged.
	auto GetAttribute( std::string_view attribute ) const -> std::optional<std::string_view>
	{
		auto found = std::ranges::find_if( attributes, [ attribute ]( const auto& entry ) { return entry.first == attribute; } );
		if( found == attributes.end() ) return std::nullopt;
		return found->second;
	}

	/// @brief Reads initial text content.
	/// @return Borrowed text.
	/// @pre None.
	/// @post State is unchanged.
	auto GetTextContent() const noexcept -> std::string_view { return text_content; }

	/// @brief Copies a CSS class into this part's state.
	/// @param class_name Class to append.
	/// @pre class_name is a nonempty CSS class token.
	/// @post The class list contains the new value; no browser message is sent.
	auto AddClass( std::string_view class_name ) -> void { classes.emplace_back( class_name ); }

	/// @brief Copies or replaces an HTML attribute in this part's state.
	/// @param attribute Attribute name.
	/// @param value Attribute value.
	/// @pre attribute is a nonempty HTML attribute name.
	/// @post Both strings are owned by the part; no browser message is sent.
	auto AddAttribute(
		std::string_view attribute,
		std::string_view value
	) -> void
	{
		auto found = std::ranges::find_if( attributes, [ attribute ]( const auto& entry ) { return entry.first == attribute; } );
		if( found == attributes.end() ) attributes.emplace_back( std::string{ attribute }, std::string{ value } );
		else found->second = value;
	}

	/// @brief Removes an attribute from stored DOM state.
	/// @param attribute Attribute name to remove.
	/// @pre None.
	/// @post The attribute is absent; no browser message is sent.
	auto RemoveAttribute( std::string_view attribute ) -> void
	{
		std::erase_if( attributes, [ attribute ]( const auto& entry ) { return entry.first == attribute; } );
	}

	/// @brief Replaces the stored DOM text.
	/// @param text New text content.
	/// @pre None.
	/// @post The part owns a copy of text; no browser message is sent.
	auto SetTextContent( std::string_view text ) -> void { text_content = text; }

private:
	std::string part_id;
	std::string parent_id;
	std::string tag;
	std::vector<std::string> classes;
	std::vector<Attribute> attributes;
	std::string text_content;
};

}
