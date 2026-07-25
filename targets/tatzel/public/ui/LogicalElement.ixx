module;

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include <memory>

export module UI.UI.LogicalElement;

export import UI.DOM.ElementPart;


namespace tatzel::ui {


export
class LogicalElement
{
public:

	inline LogicalElement(
		std::string_view id,
		LogicalElement* parent
	) :
		id{ id },
		parent{ parent }
	{}

	inline LogicalElement(
		std::string_view id,
		LogicalElement* parent,
		std::span<const dom::ElementPart> parts,
		size_t base_part_index,
		size_t child_root_part_index
	) :
		id{ id },
		parent{ parent },
		part_list{ parts.begin(), parts.end() }
	{
		assert( base_part_index < this->part_list.size() );
		assert( child_root_part_index < this->part_list.size() );

		this->base_part = &this->part_list[ base_part_index ];
		this->child_root_part = &this->part_list[ child_root_part_index ];
	}

	LogicalElement( const LogicalElement& ) = default;
	LogicalElement( LogicalElement&& ) = default;
	virtual ~LogicalElement() = default;

	auto operator=( const LogicalElement& ) -> LogicalElement& = default;
	auto operator=( LogicalElement&& ) -> LogicalElement& = default;

	auto GetID() const noexcept -> std::string_view { return id; }
	auto GetParent() const noexcept -> LogicalElement* { return parent; }

	auto SetParts( std::vector<dom::ElementPart>&& part_list, size_t base_part_index, size_t child_root_part_index ) -> void
	{
		assert( this->part_list.empty() && "Parts can only be assigned once." );
		// WARNING: Parts can only be assigned once because the address for the parts must not change.
		// This is a limitation of current architecture, this may change later.
		this->part_list = std::move( part_list );

		assert( base_part_index < this->part_list.size() );
		assert( child_root_part_index < this->part_list.size() );

		this->base_part = &this->part_list[ base_part_index ];
		this->child_root_part = &this->part_list[ child_root_part_index ];
	}

	auto GetParts() const noexcept -> std::span<const dom::ElementPart> { return part_list; }
	auto GetBasePart() -> dom::ElementPart&
	{
		assert( this->base_part );
		return *this->base_part;
	}
	auto GetChildRootPart() -> dom::ElementPart&
	{
		assert( this->child_root_part );
		return *this->child_root_part;
	}

	template<typename ElementT>
	requires( std::derived_from<ElementT, LogicalElement> )
	auto InsertChild( std::unique_ptr<ElementT> child ) -> ElementT*
	{
		auto child_ptr = child.get();
		child_list.push_back( std::move( child ) );
		return child_ptr;
	}

	auto FindPart( std::string_view part_name ) -> dom::ElementPart*
	{
		auto result = std::ranges::find_if( this->part_list, [ this, part_name ]( const auto& part ){
			return part.GetPartID() == this->id + "|" + std::string{ part_name };
		} );
		if( result == this->part_list.end() ) return nullptr;
		return &*result;
	}

	auto GetChildren() const -> const std::vector<std::unique_ptr<LogicalElement>>& { return child_list; }

private:

	std::string id;
	LogicalElement* parent = nullptr;
	std::vector<dom::ElementPart> part_list;
	std::vector<std::unique_ptr<LogicalElement>> child_list;

	dom::ElementPart* base_part = nullptr;
	dom::ElementPart* child_root_part = nullptr;
};


export
template<typename T>
concept LogicalElementDerived = std::derived_from<T, LogicalElement>;


}
