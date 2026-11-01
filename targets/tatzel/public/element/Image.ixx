/// @file
/// @brief Logical image with owned resource and accessibility state.
module;

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

export module UI.Element.Image;

import UI.Diagnostics;
import UI.ResourceURL;
export import UI.Element.LogicalElement;

namespace tatzel::ui {

/// @brief Page-owned native image with explicit alternative text.
/// @details Strings are owned. Relative, HTTP, and HTTPS sources are supported.
/// Empty alternative text marks decoration. Mutations run on the owning event loop.
export class Image : public LogicalElement
{
public:
	/// @brief Creates an image with explicit alternative text.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param source Resource URL, copied before returning; empty omits the source.
	/// @param alternative_text Accessible description; empty for decoration.
	/// @pre parent is nonnull.
	/// @post Strings are owned; dimensions are unspecified and loading is eager.
	/// @throws tatzel::Exception If source uses an unsupported URL.
	Image(
		std::string_view id,
		LogicalElement* parent,
		std::string_view source,
		std::string_view alternative_text
	) :
		LogicalElement{ id, parent },
		source{ source },
		alternative_text{ alternative_text }
	{
		ValidateSource( source );
	}

	/// @brief Reads the resource URL.
	/// @return Borrowed text, invalidated by mutation of this property or destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetSource() const noexcept -> std::string_view { return source; }

	/// @brief Replaces the resource URL.
	/// @param source New owned text; empty clears this property.
	/// @pre Called on the owning event loop.
	/// @post Changed state is stored and a bound adapter queues its browser update.
	/// @throws tatzel::Exception If the URL is unsupported; state is unchanged.
	auto SetSource( std::string_view source ) -> void
	{
		ValidateSource( source );
		if( this->source == source ) return;

		this->source = source;
		NotifyChanged();
	}

	/// @brief Reads the accessible description.
	/// @return Borrowed text, invalidated by mutation of this property or destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetAlternativeText() const noexcept -> std::string_view { return alternative_text; }

	/// @brief Replaces the accessible description.
	/// @param alternative_text New owned text; empty clears this property.
	/// @pre Called on the owning event loop.
	/// @post Changed state is stored and a bound adapter queues its browser update.
	auto SetAlternativeText( std::string_view alternative_text ) -> void
	{
		if( this->alternative_text == alternative_text ) return;

		this->alternative_text = alternative_text;
		NotifyChanged();
	}

	/// @brief Reads the intrinsic width.
	/// @return Positive CSS pixels, or nullopt when unspecified.
	/// @pre None.
	/// @post State is unchanged.
	auto GetWidth() const noexcept -> std::optional<size_t> { return width; }

	/// @brief Reads the intrinsic height.
	/// @return Positive CSS pixels, or nullopt when unspecified.
	/// @pre None.
	/// @post State is unchanged.
	auto GetHeight() const noexcept -> std::optional<size_t> { return height; }

	/// @brief Replaces intrinsic dimensions without imposing fixed display dimensions.
	/// @param width Positive CSS pixels, or nullopt to remove the width hint.
	/// @param height Positive CSS pixels, or nullopt to remove the height hint.
	/// @pre Called on the owning event loop.
	/// @post Dimensions are stored and their browser attributes are queued.
	/// @throws tatzel::Exception If a specified dimension is zero; state is unchanged.
	auto SetSize(
		std::optional<size_t> width,
		std::optional<size_t> height
	) -> void
	{
		if( ( width && !*width ) || ( height && !*height ) ) throw tatzel::Exception{ "Image dimensions must be positive" };
		if( this->width == width && this->height == height ) return;

		this->width = width;
		this->height = height;
		NotifyChanged();
	}

	/// @brief Reports whether offscreen loading may be deferred.
	/// @return Whether lazy loading is requested.
	/// @pre None.
	/// @post State is unchanged.
	auto IsLazyLoading() const noexcept -> bool { return lazy_loading; }

	/// @brief Changes the native loading hint.
	/// @param lazy_loading Whether offscreen loading may be deferred.
	/// @pre Called on the owning event loop.
	/// @post Changed state is stored and the loading attribute is queued.
	auto SetLazyLoading( bool lazy_loading ) -> void
	{
		if( this->lazy_loading == lazy_loading ) return;

		this->lazy_loading = lazy_loading;
		NotifyChanged();
	}

private:
	/// @brief Checks supported image resource schemes.
	/// @param source Candidate resource URL.
	/// @pre None.
	/// @post Candidate state is unchanged.
	/// @throws tatzel::Exception If source contains whitespace, controls, backslashes, or an unsupported scheme.
	static auto ValidateSource( std::string_view source ) -> void
	{
		if( !detail::IsSafeResourceURL( source, false ) ) throw tatzel::Exception{ "Unsupported Image source" };
	}

	std::string source;
	std::string alternative_text;
	std::optional<size_t> width;
	std::optional<size_t> height;
	bool lazy_loading = false;
};

}
