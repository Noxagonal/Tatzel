/// @file
/// @brief Logical control identity, ownership, and internal adapter state.
module;

#include <algorithm>
#include <cassert>
#include <concepts>
#include <cstddef>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module UI.Element.LogicalElement;

import UI.DOM.ElementPart;
export import UI.Element.ElementHandle;
export import UI.Element.ElementLifecycle;

namespace tatzel::ui {

/// @brief Semantic placement of a child within a compound logical control.
export enum class ChildSlot
{
	Content, ///< Main content area; available on every child-bearing control.
	Actions, ///< Optional action area provided by a card.
};

export class LogicalElement;

/// @brief Shared non-owning page removal services inherited by each logical descendant.
/// @details These functions are disconnected before page destruction; they do not own the page.
struct ElementRemovalServices
{
	std::function<bool( LogicalElement& )> remove; ///< Detaches and retires one subtree.
	std::function<size_t( LogicalElement&, std::optional<ChildSlot> )> clear; ///< Removes selected children.
};

/// @brief Base of application-facing logical controls.
/// @details A logical control owns its logical children and can have several
/// concrete DOM parts. The adapter manages those parts. Identity and addresses
/// stay fixed until removal or page destruction; copying and moving are disabled.
/// Mutations must run on the owning WebSocket event loop.
export class LogicalElement
{
public:
	/// @brief Creates a logical control before its DOM parts are assigned.
	/// @param id Unique logical ID within the client page.
	/// @param parent Owning logical parent, or nullptr for the root.
	/// @pre id is nonempty; parent outlives this control when supplied.
	/// @post The control has no children, DOM parts, or change handler.
	LogicalElement(
		std::string_view id,
		LogicalElement* parent
	) :
		id{ id },
		parent{ parent },
		dispatch_context{ parent ? parent->dispatch_context : std::make_shared<ElementDispatchContext>() },
		removal_services{ parent ? parent->removal_services : std::make_shared<ElementRemovalServices>() }
	{}

	/// @brief Creates a logical control with an existing DOM representation.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent, or nullptr for the root.
	/// @param parts DOM parts whose storage is transferred to this control.
	/// @param base_part_index Index of the control's main browser node.
	/// @param child_root_part_index Index of the browser node receiving children.
	/// @pre id is nonempty; both indices refer to entries in parts.
	/// @post Parts are owned by this control and both internal indices are set.
	LogicalElement(
		std::string_view id,
		LogicalElement* parent,
		std::vector<dom::ElementPart>&& parts,
		size_t base_part_index,
		size_t child_root_part_index
	) :
		LogicalElement{ id, parent }
	{
		SetParts( std::move( parts ), base_part_index, child_root_part_index );
	}

	/// @brief Prevents duplicate logical ownership.
	/// @param other Source control.
	/// @pre None.
	/// @post Copy construction is unavailable.
	LogicalElement( const LogicalElement& other ) = delete;

	/// @brief Preserves control addresses.
	/// @param other Source control.
	/// @pre None.
	/// @post Move construction is unavailable.
	LogicalElement( LogicalElement&& other ) = delete;

	/// @brief Destroys this control and its logical descendants.
	/// @pre No callback is using the subtree.
	/// @post Owned children and DOM state are released.
	virtual ~LogicalElement() { lifetime_state->Invalidate(); }

	/// @brief Prevents duplicate logical ownership.
	/// @param other Source control.
	/// @return No value; operation is deleted.
	/// @pre None.
	/// @post Copy assignment is unavailable.
	auto operator=( const LogicalElement& other ) -> LogicalElement& = delete;

	/// @brief Preserves control addresses.
	/// @param other Source control.
	/// @return No value; operation is deleted.
	/// @pre None.
	/// @post Move assignment is unavailable.
	auto operator=( LogicalElement&& other ) -> LogicalElement& = delete;

	/// @brief Creates a checked handle retaining this control's concrete static type.
	/// @tparam ElementT Deduced type of the referenced control, including const qualification.
	/// @param element Explicit object parameter supplied by the language.
	/// @return Copyable non-owning handle, immediately empty for a removed control.
	/// @pre Called on the owning event loop while the object is alive.
	/// @post The handle does not retain the control or page.
	template<typename ElementT>
	auto GetHandle( this ElementT& element ) noexcept -> ElementHandle<ElementT>
	{
		return { &element, element.lifetime_state };
	}

	/// @brief Reports whether this object has left the active logical tree.
	/// @return Whether removal or page teardown has invalidated this control.
	/// @pre The object is still alive; raw pointers cannot be used after destruction.
	/// @post State is unchanged.
	auto IsRemoved() const noexcept -> bool { return removed; }

	/// @brief Protects a synchronous callback or framework operation from premature destruction.
	/// @return Noncopyable stack guard for this page's shared execution context.
	/// @pre Called on the owning event loop; the page outlives the scope.
	/// @post Deferred destruction waits for this and every enclosing scope to finish.
	/// @note Framework integration API; normal callbacks are protected automatically.
	auto BeginDispatch() const -> ElementDispatchScope { return ElementDispatchScope{ dispatch_context }; }

	/// @brief Removes this control and its logical descendants.
	/// @return Whether an owned active subtree was detached; false for roots or inactive/unowned controls.
	/// @pre Called on the owning event loop while this object is alive.
	/// @post Handles are invalid on success; a bound sender queues rendered-root removal; destruction occurs at outermost scope exit.
	/// @note Without an enclosing execution scope, the pointer is invalid when this method returns.
	auto Remove() -> bool
	{
		auto dispatch = BeginDispatch();
		if( removed || !parent || !removal_services->remove ) return false;
		return removal_services->remove( *this );
	}

	/// @brief Removes all logical children while preserving this control and its own parts.
	/// @return Number of detached direct children, including each child's descendants implicitly.
	/// @pre Called on the owning event loop while this object is alive.
	/// @post Child handles are invalid; this control's state, parts, and callbacks remain intact.
	auto ClearChildren() -> size_t { return ClearChildrenImpl( std::nullopt ); }

	/// @brief Removes direct children in one semantic content area.
	/// @param slot Content or actions slot whose children should be removed.
	/// @return Number of detached direct children.
	/// @pre Called on the owning event loop; slot is a defined ChildSlot value.
	/// @post Only selected child subtrees are detached; this control remains active.
	auto ClearChildren( ChildSlot slot ) -> size_t { return ClearChildrenImpl( slot ); }

	/// @brief Connects or disconnects the root's shared framework lifetime services.
	/// @param remove_handler Page-owned subtree removal operation, or empty to disconnect.
	/// @param clear_handler Page-owned child clearing operation, or empty to disconnect.
	/// @param on_idle Nonthrowing deferred-destruction drain, or empty to disconnect.
	/// @pre Called on the owning event loop on a logical root; handlers do not outlive their page.
	/// @post All descendants sharing this root's services use the supplied operations.
	/// @note Framework integration API.
	auto SetLifecycleHandlers(
		std::function<bool( LogicalElement& )> remove_handler,
		std::function<size_t( LogicalElement&, std::optional<ChildSlot> )> clear_handler,
		std::function<void()> on_idle
	) -> void
	{
		assert( !parent );
		removal_services->remove = std::move( remove_handler );
		removal_services->clear = std::move( clear_handler );
		dispatch_context->SetOnIdle( std::move( on_idle ) );
	}

	/// @brief Invalidates this entire subtree before releasing any owned objects.
	/// @pre Called on the owning event loop before removing or destroying the subtree.
	/// @post Handles and adapter updates are disabled immediately; ownership is unchanged.
	/// @note Framework integration API.
	auto InvalidateSubtree() noexcept -> void
	{
		removed = true;
		lifetime_state->Invalidate();
		for( const auto& child : child_list ) child->InvalidateSubtree();
	}

	/// @brief Extracts one owned direct child without destroying it.
	/// @param child Direct logical child to detach.
	/// @return Unique ownership of the child, or nullptr if it is not directly owned here.
	/// @pre Called on the owning event loop; the caller preserves lifetime until active scopes end.
	/// @post A detached child is absent from traversal and has no logical parent.
	/// @note Framework integration API; application code calls Remove instead.
	auto DetachChild( LogicalElement& child ) -> std::unique_ptr<LogicalElement>
	{
		auto found = std::ranges::find_if( child_list, [ &child ]( const auto& owned ) { return owned.get() == &child; } );
		if( found == child_list.end() ) return nullptr;

		auto result = std::move( *found );
		child_list.erase( found );
		result->parent = nullptr;
		return result;
	}

	/// @brief Reads the logical ID.
	/// @return Borrowed logical ID, not a DOM part ID.
	/// @pre None.
	/// @post State is unchanged.
	auto GetID() const noexcept -> std::string_view { return id; }

	/// @brief Reads the logical parent.
	/// @return Parent, or nullptr for the root.
	/// @pre None.
	/// @post State is unchanged.
	auto GetParent() const noexcept -> LogicalElement* { return parent; }

	/// @brief Reads the semantic slot used within the logical parent.
	/// @return The child's insertion slot.
	/// @pre None.
	/// @post State is unchanged.
	auto GetParentSlot() const noexcept -> ChildSlot { return parent_slot; }

	/// @brief Assigns semantic placement before the adapter maps this child.
	/// @param slot Parent slot selected by the scoped builder.
	/// @pre No DOM parts have been assigned; the logical parent supports slot.
	/// @post The adapter can resolve placement without exposing DOM IDs to application code.
	/// @note Framework integration API; placement is fixed after mapping.
	auto SetParentSlot( ChildSlot slot ) -> void
	{
		assert( part_list.empty() );
		parent_slot = slot;
	}

	/// @brief Assigns the adapter-managed DOM representation once.
	/// @param parts DOM parts to own.
	/// @param base_part_index Index of the main browser node.
	/// @param child_root_part_index Index of the browser node receiving children.
	/// @pre No parts have been assigned; both indices are valid in parts.
	/// @pre Rendered roots contain no browser nodes owned by unrelated logical subtrees.
	/// @post The representation is stored without later resizing.
	/// @note Framework integration API; application code uses logical setters.
	auto SetParts(
		std::vector<dom::ElementPart>&& parts,
		size_t base_part_index,
		size_t child_root_part_index
	) -> void
	{
		assert( part_list.empty() );
		assert( base_part_index < parts.size() );
		assert( child_root_part_index < parts.size() );
		part_list = std::move( parts );
		this->base_part_index = base_part_index;
		this->child_root_part_index = child_root_part_index;
	}

	/// @brief Reads internal DOM parts.
	/// @return Borrowed immutable part list.
	/// @pre None.
	/// @post State is unchanged.
	auto GetParts() const noexcept -> std::span<const dom::ElementPart> { return part_list; }

	/// @brief Accesses the adapter's main DOM part.
	/// @return Main part.
	/// @pre Parts have been assigned.
	/// @post State is unchanged.
	auto GetBasePart() -> dom::ElementPart& { assert( !part_list.empty() ); return part_list[ base_part_index ]; }

	/// @brief Accesses the DOM parent for new children.
	/// @return Child root part.
	/// @pre Parts have been assigned.
	/// @post State is unchanged.
	auto GetChildRootPart() -> dom::ElementPart& { assert( !part_list.empty() ); return part_list[ child_root_part_index ]; }

	/// @brief Transfers a logical child into this control's ownership.
	/// @tparam ElementT Logical control type.
	/// @param child Child allocated at its final stable address.
	/// @return Borrowed pointer to the inserted child.
	/// @pre child is nonnull, active, and its logical parent is this control.
	/// @post The child is owned by this control; its address is unchanged.
	/// @throws std::logic_error If this control was already removed.
	template<typename ElementT> requires std::derived_from<ElementT, LogicalElement>
	auto InsertChild( std::unique_ptr<ElementT> child ) -> ElementT*
	{
		if( removed ) throw std::logic_error{ "Cannot insert children into a removed control" };
		assert( child && !child->IsRemoved() && child->GetParent() == this );
		auto* result = child.get();
		child_list.push_back( std::move( child ) );
		return result;
	}

	/// @brief Finds an internal DOM part by its adapter-local name.
	/// @param part_name Local part name such as base.
	/// @return Matching part, or nullptr.
	/// @pre None.
	/// @post State is unchanged.
	auto FindPart( std::string_view part_name ) -> dom::ElementPart*
	{
		auto part_id = id + "|" + std::string{ part_name };
		auto found = std::ranges::find_if( part_list, [ &part_id ]( const auto& part ) { return part.GetPartID() == part_id; } );
		return found == part_list.end() ? nullptr : &*found;
	}

	/// @brief Reads logical children.
	/// @return Borrowed owned-child list.
	/// @pre None.
	/// @post State is unchanged.
	auto GetChildren() const noexcept -> const std::vector<std::unique_ptr<LogicalElement>>& { return child_list; }

	/// @brief Installs the framework callback that synchronizes logical changes.
	/// @param handler Adapter callback, or an empty function to clear it.
	/// @pre Captured adapter state outlives this control.
	/// @post Future logical changes invoke the new handler synchronously.
	auto SetChangeHandler( std::function<void()> handler ) -> void
	{
		change_handler = handler ? std::make_shared<std::function<void()>>( std::move( handler ) ) : nullptr;
	}

protected:
	/// @brief Notifies the adapter after a logical setter changes state.
	/// @pre Called on the owning event loop after the logical state is valid.
	/// @post An active control's installed handler has run if present; removed controls emit no updates.
	auto NotifyChanged() -> void
	{
		auto dispatch = BeginDispatch();
		if( removed ) return;

		auto handler = change_handler;
		if( handler ) ( *handler )();
	}

private:
	/// @brief Delegates child clearing to the owning page under execution protection.
	/// @param slot Optional semantic slot; no value selects every child.
	/// @return Number of detached direct children, or zero for removed/unbound controls.
	/// @pre Called on the owning event loop while this control is alive.
	/// @post Selected children are detached and queued for destruction at outermost scope exit.
	auto ClearChildrenImpl( std::optional<ChildSlot> slot ) -> size_t
	{
		auto dispatch = BeginDispatch();
		if( removed || !removal_services->clear ) return 0;
		return removal_services->clear( *this, slot );
	}

	std::string id;
	LogicalElement* parent = nullptr;
	std::shared_ptr<ElementLifetimeState> lifetime_state = std::make_shared<ElementLifetimeState>();
	std::shared_ptr<ElementDispatchContext> dispatch_context;
	std::shared_ptr<ElementRemovalServices> removal_services;
	bool removed = false;
	ChildSlot parent_slot = ChildSlot::Content;
	std::vector<dom::ElementPart> part_list;
	std::vector<std::unique_ptr<LogicalElement>> child_list;
	size_t base_part_index = 0;
	size_t child_root_part_index = 0;
	std::shared_ptr<std::function<void()>> change_handler;
};

/// @brief Constrains builder types to logical controls.
/// @tparam T Candidate control type.
export template<typename T>
concept LogicalElementDerived = std::derived_from<T, LogicalElement>;

}
