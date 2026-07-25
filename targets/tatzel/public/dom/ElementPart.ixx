module;

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <utility>

export module UI.DOM.ElementPart;


namespace tatzel::dom {


export
class ElementPart
{
public:

	using Attribute = std::pair<std::string_view, std::string_view>;

	inline ElementPart(
		std::string_view part_id,
		std::string_view parent_id,
		std::string_view tag,
		std::span<const std::string_view> classes = {},
		std::span<const Attribute> attributes = {},
		std::optional<std::size_t> parent_index = std::nullopt
	) :
		part_id{ std::string{ part_id } },
		parent_id{ std::string{ parent_id } },
		tag{ std::string{ tag } },
		classes{ classes.begin(), classes.end() },
		attributes{ attributes.begin(), attributes.end() }
	{}

	auto GetPartID() const -> std::string_view { return part_id; }
	auto GetParentID() const -> std::string_view { return parent_id; }
	auto GetTag() const -> std::string_view { return tag; }
	auto GetClasses() const -> std::span<const std::string> { return classes; }
	auto GetAttributes() const -> std::span<const Attribute> { return attributes; }
	auto GetTextContent() const -> std::string_view { return text_content; }

	auto AddClass( std::string_view class_name ) -> void { this->classes.push_back( std::string{ class_name } ); }
	auto AddAttribute( std::string_view attribute, std::string_view value ) -> void { this->attributes.push_back( { attribute, value } ); }
	auto SetTextContent( std::string_view text ) -> void { this->text_content = text; }

	std::string part_id;
	std::string parent_id;
	std::string tag;
	std::vector<std::string> classes;
	std::vector<Attribute> attributes;
	std::string text_content;
};


}
