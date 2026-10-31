/// @file
/// @brief Event-loop execution scopes used to defer removed-control destruction.
module;

#include <cassert>
#include <cstddef>
#include <functional>
#include <memory>
#include <utility>

export module UI.Element.ElementLifecycle;

namespace tatzel::ui {

/// @brief Weak-handle token; invalidation never owns or retains an element.
/// @details Access is confined to the owning page's event loop.
export class ElementLifetimeState
{
public:
	/// @brief Reports whether the associated element is still active.
	/// @return Whether checked handles may resolve their borrowed pointer.
	/// @pre Called on the owning event loop.
	/// @post State is unchanged.
	auto IsActive() const noexcept -> bool { return active; }

	/// @brief Permanently invalidates checked references to an element.
	/// @pre Called on the owning event loop before removal or destruction.
	/// @post Every handle using this token resolves to nullptr.
	auto Invalidate() noexcept -> void { active = false; }

private:
	bool active = true;
};

/// @brief Counts nested synchronous execution scopes for one logical page.
/// @details The page owns removed subtrees; this context only signals when their
/// final active execution scope has ended. It provides no cross-thread scheduler.
export class ElementDispatchContext
{
public:
	/// @brief Installs the page's deferred-destruction drain.
	/// @param handler Framework cleanup callback, or empty to disconnect page services.
	/// @pre Called on the owning event loop; handler never throws and outlives its use.
	/// @post The next transition to zero scopes invokes the installed handler.
	auto SetOnIdle( std::function<void()> handler ) -> void { on_idle = std::move( handler ); }

	/// @brief Enters one synchronous execution scope.
	/// @pre Called on the owning event loop.
	/// @post Nested depth is incremented.
	auto Enter() noexcept -> void { ++depth; }

	/// @brief Leaves a scope and drains removed elements when the outermost scope ends.
	/// @pre Called on the owning event loop with nonzero depth; cleanup does not throw.
	/// @post Depth is decremented and zero-depth cleanup has completed if installed.
	auto Leave() noexcept -> void
	{
		assert( depth > 0 );
		--depth;
		if( depth == 0 && on_idle ) on_idle();
	}

	/// @brief Reports whether page code is executing inside a protected scope.
	/// @return Whether at least one execution scope is active.
	/// @pre Called on the owning event loop.
	/// @post State is unchanged.
	auto IsExecuting() const noexcept -> bool { return depth != 0; }

private:
	size_t depth = 0;
	std::function<void()> on_idle;
};

/// @brief Stack guard retaining removed controls through nested callbacks and exception unwinding.
/// @details A scope retains the context, never an active element or page. The
/// owning page must outlive scopes; page teardown disconnects the cleanup hook.
export class ElementDispatchScope
{
public:
	/// @brief Protects one synchronous execution interval.
	/// @param context Shared page execution context, retained by this scope.
	/// @pre context is nonnull; called on the owning event loop.
	/// @post The context has one additional active scope.
	explicit ElementDispatchScope( std::shared_ptr<ElementDispatchContext> context ) :
		context{ std::move( context ) }
	{
		this->context->Enter();
	}

	/// @brief Prevents duplicate execution-scope ownership.
	/// @param other Source scope.
	/// @pre None.
	/// @post Copy construction is unavailable.
	ElementDispatchScope( const ElementDispatchScope& other ) = delete;

	/// @brief Keeps the scope attached to its lexical lifetime.
	/// @param other Source scope.
	/// @pre None.
	/// @post Move construction is unavailable.
	ElementDispatchScope( ElementDispatchScope&& other ) = delete;

	/// @brief Completes execution protection, including during exception unwinding.
	/// @pre The owning event loop is active; the cleanup callback does not throw.
	/// @post The outermost scope has released all retired page elements.
	~ElementDispatchScope() { context->Leave(); }

	/// @brief Prevents reassignment of execution-scope ownership.
	/// @param other Source scope.
	/// @return No value; the operation is deleted.
	/// @pre None.
	/// @post Copy assignment is unavailable.
	auto operator=( const ElementDispatchScope& other ) -> ElementDispatchScope& = delete;

	/// @brief Keeps the scope attached to its lexical lifetime.
	/// @param other Source scope.
	/// @return No value; the operation is deleted.
	/// @pre None.
	/// @post Move assignment is unavailable.
	auto operator=( ElementDispatchScope&& other ) -> ElementDispatchScope& = delete;

private:
	std::shared_ptr<ElementDispatchContext> context;
};

}
