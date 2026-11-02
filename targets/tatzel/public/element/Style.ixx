/// @file
/// @brief Semantic control sizing and live-feedback policies.
export module UI.Element.Style;

namespace tatzel::ui {

/// @brief Theme-defined control dimensions, independent of concrete CSS units.
export enum class ControlSize
{
	Small, ///< Compact control padding and type.
	Medium, ///< Default control dimensions.
	Large, ///< Generous control padding and type.
};

/// @brief Announcement policy for changing plain text.
export enum class LiveRegion
{
	Off, ///< Ordinary text without automatic announcement.
	Polite, ///< Announce feedback after current speech finishes.
	Assertive, ///< Announce urgent feedback promptly.
};

}
