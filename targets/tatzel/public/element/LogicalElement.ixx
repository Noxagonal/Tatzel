/** @file @brief Logical control identity, ownership, and internal adapter state. */
module;

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module UI.Element.LogicalElement;

import UI.DOM.ElementPart;

namespace tatzel::ui {

/**
 * @brief Base of application-facing logical controls.
 * @details A logical control owns its logical children and can have several
 * concrete DOM parts. The adapter manages those parts. Identity and addresses
 * stay fixed for the lifetime of the page; copying and moving are disabled.
 * Mutations must run on the owning WebSocket event loop.
 */
export class LogicalElement
{
public:
	/**
	 * @brief Creates a logical control before its DOM parts are assigned.
	 * @param id Unique logical ID within the client page.
	 * @param parent Owning logical parent, or nullptr for the root.
	 * @pre id is nonempty; parent outlives this control when supplied.
	 * @post The control has no children, DOM parts, or change handler.
	 */
	LogicalElement( std::string_view id, LogicalElement* parent ) : id{ id }, parent{ parent } {}

	/**
	 * @brief Creates a logical control with an existing DOM representation.
	 * @param id Unique logical ID.
	 * @param parent Owning logical parent, or nullptr for the root.
	 * @param parts DOM parts whose storage is transferred to this control.
	 * @param base_part_index Index of the control's main browser node.
	 * @param child_root_part_index Index of the browser node receiving children.
	 * @pre id is nonempty; both indices refer to entries in parts.
	 * @post Parts are owned by this control and both internal indices are set.
	 */
	LogicalElement( std::string_view id, LogicalElement* parent,
		std::vector<dom::ElementPart>&& parts, size_t base_part_index, size_t child_root_part_index ) :
		LogicalElement{ id, parent }
	{
		SetParts( std::move( parts ), base_part_index, child_root_part_index );
	}

	/** @brief Prevents duplicate logical ownership. @param other Source control. @pre None. @post Copy construction is unavailable. */
	LogicalElement( const LogicalElement& other ) = delete;
	/** @brief Preserves control addresses. @param other Source control. @pre None. @post Move construction is unavailable. */
	LogicalElement( LogicalElement&& other ) = delete;
	/** @brief Destroys this control and its logical descendants. @pre No callback is using the subtree. @post Owned children and DOM state are released. */
	virtual ~LogicalElement() = default;
	/** @brief Prevents duplicate logical ownership. @param other Source control. @return No value; operation is deleted. @pre None. @post Copy assignment is unavailable. */
	auto operator=( const LogicalElement& other ) -> LogicalElement& = delete;
	/** @brief Preserves control addresses. @param other Source control. @return No value; operation is deleted. @pre None. @post Move assignment is unavailable. */
	auto operator=( LogicalElement&& other ) -> LogicalElement& = delete;

	/** @brief Reads the logical ID. @return Borrowed logical ID, not a DOM part ID. @pre None. @post State is unchanged. */
	auto GetID() const noexcept -> std::string_view { return id; }
	/** @brief Reads the logical parent. @return Parent, or nullptr for the root. @pre None. @post State is unchanged. */
	auto GetParent() const noexcept -> LogicalElement* { return parent; }

	/**
	 * @brief Assigns the adapter-managed DOM representation once.
	 * @param parts DOM parts to own.
	 * @param base_part_index Index of the main browser node.
	 * @param child_root_part_index Index of the browser node receiving children.
	 * @pre No parts have been assigned; both indices are valid in parts.
	 * @post The representation is stored without later resizing.
	 * @note Framework integration API; application code uses logical setters.
	 */
	auto SetParts( std::vector<dom::ElementPart>&& parts, size_t base_part_index, size_t child_root_part_index ) -> void
	{
		assert( part_list.empty() );
		assert( base_part_index < parts.size() );
		assert( child_root_part_index < parts.size() );
		part_list = std::move( parts );
		this->base_part_index = base_part_index;
		this->child_root_part_index = child_root_part_index;
	}

	/** @brief Reads internal DOM parts. @return Borrowed immutable part list. @pre None. @post State is unchanged. */
	auto GetParts() const noexcept -> std::span<const dom::ElementPart> { return part_list; }
	/** @brief Accesses the adapter's main DOM part. @return Main part. @pre Parts have been assigned. @post State is unchanged. */
	auto GetBasePart() -> dom::ElementPart& { assert( !part_list.empty() ); return part_list[ base_part_index ]; }
	/** @brief Accesses the DOM parent for new children. @return Child root part. @pre Parts have been assigned. @post State is unchanged. */
	auto GetChildRootPart() -> dom::ElementPart& { assert( !part_list.empty() ); return part_list[ child_root_part_index ]; }

	/**
	 * @brief Transfers a logical child into this control's ownership.
	 * @tparam ElementT Logical control type.
	 * @param child Child allocated at its final stable address.
	 * @return Borrowed pointer to the inserted child.
	 * @pre child is nonnull and its logical parent is this control.
	 * @post The child is owned by this control; its address is unchanged.
	 */
	template<typename ElementT> requires std::derived_from<ElementT, LogicalElement>
	auto InsertChild( std::unique_ptr<ElementT> child ) -> ElementT*
	{
		assert( child && child->GetParent() == this );
		auto* result = child.get();
		child_list.push_back( std::move( child ) );
		return result;
	}

	/**
	 * @brief Finds an internal DOM part by its adapter-local name.
	 * @param part_name Local part name such as base.
	 * @return Matching part, or nullptr.
	 * @pre None.
	 * @post State is unchanged.
	 */
	auto FindPart( std::string_view part_name ) -> dom::ElementPart*
	{
		auto part_id = id + "|" + std::string{ part_name };
		auto found = std::ranges::find_if( part_list, [ &part_id ]( const auto& part ) { return part.GetPartID() == part_id; } );
		return found == part_list.end() ? nullptr : &*found;
	}

	/** @brief Reads logical children. @return Borrowed owned-child list. @pre None. @post State is unchanged. */
	auto GetChildren() const noexcept -> const std::vector<std::unique_ptr<LogicalElement>>& { return child_list; }

	/**
	 * @brief Installs the framework callback that synchronizes logical changes.
	 * @param handler Adapter callback, or an empty function to clear it.
	 * @pre Captured adapter state outlives this control.
	 * @post Future logical changes invoke the new handler synchronously.
	 */
	auto SetChangeHandler( std::function<void()> handler ) -> void { change_handler = std::move( handler ); }

protected:
	/**
	 * @brief Notifies the adapter after a logical setter changes state.
	 * @pre Called on the owning event loop after the logical state is valid.
	 * @post The installed handler has run, if present.
	 */
	auto NotifyChanged() -> void { if( change_handler ) change_handler(); }

private:
	std::string id;
	LogicalElement* parent = nullptr;
	std::vector<dom::ElementPart> part_list;
	std::vector<std::unique_ptr<LogicalElement>> child_list;
	size_t base_part_index = 0;
	size_t child_root_part_index = 0;
	std::function<void()> change_handler;
};

/** @brief Constrains builder types to logical controls. @tparam T Candidate control type. */
export template<typename T>
concept LogicalElementDerived = std::derived_from<T, LogicalElement>;

}
