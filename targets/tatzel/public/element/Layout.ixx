/// @file
/// @brief Semantic linear layout settings shared by rows and columns.
module;

#include <string_view>

export module UI.Element.Layout;

export import UI.Element.LogicalElement;

namespace tatzel::ui {

/// @brief Direction of a linear logical layout.
export enum class LayoutDirection
{
	Row, ///< Children flow horizontally.
	Column, ///< Children flow vertically.
};

/// @brief Theme-independent spacing between neighboring children.
export enum class Spacing
{
	None, ///< No gap.
	Small, ///< Compact separation.
	Medium, ///< Normal separation.
	Large, ///< Generous separation.
};

/// @brief Alignment across the layout's main direction.
export enum class Alignment
{
	Start, ///< Align at the cross-axis start.
	Center, ///< Center along the cross axis.
	End, ///< Align at the cross-axis end.
	Stretch, ///< Stretch children along the cross axis.
};

/// @brief Distribution along the layout's main direction.
export enum class Justification
{
	Start, ///< Pack children at the start.
	Center, ///< Center the group.
	End, ///< Pack children at the end.
	SpaceBetween, ///< Distribute available space between children.
	SpaceAround, ///< Distribute space around children.
	SpaceEvenly, ///< Use equal space around and between children.
};

/// @brief Initial semantic layout settings; concrete CSS belongs to the adapter.
export struct LayoutOptions
{
	Spacing gap = Spacing::Medium; ///< Separation between children.
	Alignment alignment = Alignment::Stretch; ///< Cross-axis alignment.
	Justification justification = Justification::Start; ///< Main-axis distribution.
	bool wrap = false; ///< Whether children may flow onto additional lines.
};

/// @brief Page-owned base for linear layouts with stable child ownership.
/// @details Settings are values; mutations run on the owning page's event loop.
export class Layout : public LogicalElement
{
public:
	/// @brief Creates a logical linear layout before its children are built.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param direction Main direction of the layout.
	/// @param options Initial semantic layout settings.
	/// @pre parent is nonnull; enum values are defined values of their types.
	/// @post Settings are stored; the builder assigns the DOM representation.
	Layout(
		std::string_view id,
		LogicalElement* parent,
		LayoutDirection direction,
		LayoutOptions options
	) :
		LogicalElement{ id, parent },
		direction{ direction },
		options{ options }
	{}

	/// @brief Reads the spacing between children.
	/// @return Current spacing between children.
	/// @pre None.
	/// @post State is unchanged.
	auto GetGap() const noexcept -> Spacing { return options.gap; }

	/// @brief Changes the spacing between children.
	/// @param gap New spacing between children.
	/// @pre Called on the owning event loop; enum arguments are defined values.
	/// @post Settings are stored and a bound adapter queues changed layout styling.
	auto SetGap( Spacing gap ) -> void
	{
		if( options.gap == gap ) return;

		options.gap = gap;
		NotifyChanged();
	}

	/// @brief Reads the cross-axis alignment.
	/// @return Current cross-axis alignment.
	/// @pre None.
	/// @post State is unchanged.
	auto GetAlignment() const noexcept -> Alignment { return options.alignment; }

	/// @brief Changes the cross-axis alignment.
	/// @param alignment New cross-axis alignment.
	/// @pre Called on the owning event loop; enum arguments are defined values.
	/// @post Settings are stored and a bound adapter queues changed layout styling.
	auto SetAlignment( Alignment alignment ) -> void
	{
		if( options.alignment == alignment ) return;

		options.alignment = alignment;
		NotifyChanged();
	}

	/// @brief Reads the main-axis distribution.
	/// @return Current main-axis distribution.
	/// @pre None.
	/// @post State is unchanged.
	auto GetJustification() const noexcept -> Justification { return options.justification; }

	/// @brief Changes the main-axis distribution.
	/// @param justification New main-axis distribution.
	/// @pre Called on the owning event loop; enum arguments are defined values.
	/// @post Settings are stored and a bound adapter queues changed layout styling.
	auto SetJustification( Justification justification ) -> void
	{
		if( options.justification == justification ) return;

		options.justification = justification;
		NotifyChanged();
	}

	/// @brief Reads the wrapping policy.
	/// @return Current wrapping policy.
	/// @pre None.
	/// @post State is unchanged.
	auto GetWrap() const noexcept -> bool { return options.wrap; }

	/// @brief Changes the wrapping policy.
	/// @param wrap New wrapping policy.
	/// @pre Called on the owning event loop; enum arguments are defined values.
	/// @post Settings are stored and a bound adapter queues changed layout styling.
	auto SetWrap( bool wrap ) -> void
	{
		if( options.wrap == wrap ) return;

		options.wrap = wrap;
		NotifyChanged();
	}

	/// @brief Reads the layout direction.
	/// @return The immutable main direction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetDirection() const noexcept -> LayoutDirection { return direction; }

private:
	LayoutDirection direction;
	LayoutOptions options;
};

}
