/// @file
/// @brief Shared wire limits and logical field update identities.
module;

#include <cstddef>
#include <cstdint>
#include <string>

export module UI.Protocol;

namespace tatzel {

export inline constexpr unsigned ProtocolVersion = 2; ///< Incompatible clients must start a fresh page with matching assets.
export inline constexpr std::uint64_t MaximumWireRevision = 9007199254740991ULL; ///< Largest exactly representable JavaScript integer.
export inline constexpr std::size_t MaximumBatchOperations = 128; ///< Maximum operations in one update batch.
export inline constexpr std::size_t MaximumBatchBytes = 1024 * 1024; ///< Serialized batch size limit, including framing.

}

namespace tatzel::ui {

/// @brief Policy for programmatic changes to an editable field.
export enum class ValueUpdatePolicy
{
	PreserveNewerInput, ///< Apply only while the browser has no newer edit than the server has observed.
	Replace, ///< Begin a new value generation and deliberately replace pending browser input.
};

/// @brief Identity of one browser edit within a server-owned value generation.
export struct ClientEdit
{
	std::uint64_t epoch = 0; ///< Generation established by explicit replacements or choice-list changes.
	std::uint64_t revision = 0; ///< Browser edit counter within that generation.
};

/// @brief Owned token for applying asynchronous results to an unchanged logical field.
export struct EditRevision
{
	std::string element_id; ///< Logical control identity; tokens cannot accidentally match another control.
	std::uint64_t epoch = 0; ///< Value generation when captured.
	std::uint64_t client_revision = 0; ///< Last accepted browser edit when captured.
	std::uint64_t server_revision = 0; ///< Explicit server value/choice revision when captured.

	/// @brief Compares field identity and all captured revisions.
	/// @param other Token to compare.
	/// @return Whether both tokens describe the same logical state.
	/// @pre None.
	/// @post Neither token changes.
	auto operator==( const EditRevision& other ) const -> bool = default;
};

/// @brief Adapter metadata accompanying a value or native-choice replacement.
export struct ValueUpdate
{
	std::uint64_t epoch = 0; ///< Resulting value generation.
	std::uint64_t base_epoch = 0; ///< Generation against which browser edits are compared.
	std::uint64_t client_revision = 0; ///< Last browser edit known when this update was produced.
	bool replace = false; ///< Whether newer browser drafts must deliberately be replaced.
};

/// @brief Result of checking untrusted browser edit metadata.
export enum class EditAcceptance
{
	Current, ///< The edit belongs to the current generation and does not regress its counter.
	Stale, ///< A replacement or newer edit has superseded this message.
	Invalid, ///< The metadata is from the future, out of range, or reuses a revision for another value.
};

}
