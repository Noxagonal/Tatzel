#pragma once


import UI;

namespace example {

/// @brief Registers showcase routes and a links-only index from one route list.
/// @param app Application receiving the routes before startup.
/// @param background Queue used by asynchronous showcase callbacks.
/// @pre Run has not started; background outlives app and its page callbacks.
/// @post The index links to every registered showcase, including the expected failure example.
auto RegisterShowcases(
	tatzel::App& app,
	tatzel::TaskQueue& background
) -> void;

/// @brief Builds the controls showcase for one browser connection.
/// @param ui Builder for the active logical page.
/// @pre Called on the page owning loop; runtime assets are available.
/// @post The page owns its showcase controls and per-client state.
auto BuildControlsShowcase( tatzel::PageBuilder<> ui ) -> void;

/// @brief Builds the forms showcase for one browser connection.
/// @param ui Builder for the active logical page.
/// @pre Called on the page owning loop; runtime assets are available.
/// @post The page owns its showcase controls and per-client state.
auto BuildFormsShowcase( tatzel::PageBuilder<> ui ) -> void;

/// @brief Builds the elements showcase for one browser connection.
/// @param ui Builder for the active logical page.
/// @pre Called on the page owning loop; runtime assets are available.
/// @post The page owns its showcase controls and per-client state.
auto BuildElementsShowcase( tatzel::PageBuilder<> ui ) -> void;

/// @brief Builds the lifecycle showcase for one browser connection.
/// @param ui Builder for the active logical page.
/// @pre Called on the page owning loop; runtime assets are available.
/// @post The page owns its showcase controls and per-client state.
auto BuildLifecycleShowcase( tatzel::PageBuilder<> ui ) -> void;

/// @brief Builds the dispatcher showcase for one browser connection.
/// @param ui Builder for the active logical page.
/// @param background Application-owned queue for asynchronous example work.
/// @pre Called on the page owning loop; runtime assets are available.
/// @post The page owns its showcase controls and per-client state.
auto BuildDispatcherShowcase(
	tatzel::PageBuilder<> ui,
	tatzel::TaskQueue& background
) -> void;

/// @brief Builds the protocol showcase for one browser connection.
/// @param ui Builder for the active logical page.
/// @param background Application-owned queue for asynchronous example work.
/// @pre Called on the page owning loop; runtime assets are available.
/// @post The page owns its showcase controls and per-client state.
auto BuildProtocolShowcase(
	tatzel::PageBuilder<> ui,
	tatzel::TaskQueue& background
) -> void;

/// @brief Builds the components showcase for one browser connection.
/// @param ui Builder for the active logical page.
/// @param background Application-owned queue for asynchronous example work.
/// @pre Called on the page owning loop; runtime assets are available.
/// @post The page owns its showcase controls and per-client state.
auto BuildComponentsShowcase(
	tatzel::PageBuilder<> ui,
	tatzel::TaskQueue& background
) -> void;

/// @brief Builds the tables showcase for one browser connection.
/// @param ui Builder for the active logical page.
/// @pre Called on the page owning loop; runtime assets are available.
/// @post The page owns its showcase controls and per-client state.
auto BuildTablesShowcase( tatzel::PageBuilder<> ui ) -> void;

/// @brief Builds the failure showcase for one browser connection.
/// @param ui Builder for the active logical page.
/// @pre Called on the page owning loop; runtime assets are available.
/// @post A partial heading is queued before the intentional initialization failure.
/// @throws tatzel::Exception Always, to demonstrate terminal page recovery.
auto BuildFailureShowcase( tatzel::PageBuilder<> ui ) -> void;

}
