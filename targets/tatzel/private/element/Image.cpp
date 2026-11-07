module;

#include <cstddef>
#include <optional>
#include <string_view>

module UI.Element.Image;

import UI.Diagnostics;
import UI.ResourceURL;

namespace tatzel::ui {

Image::Image(
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

auto Image::SetSource( std::string_view source ) -> void
{
	ValidateSource( source );
	if( this->source == source ) return;

	this->source = source;
	NotifyChanged();
}

auto Image::SetAlternativeText( std::string_view alternative_text ) -> void
{
	if( this->alternative_text == alternative_text ) return;

	this->alternative_text = alternative_text;
	NotifyChanged();
}

auto Image::SetSize(
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

auto Image::SetLazyLoading( bool lazy_loading ) -> void
{
	if( this->lazy_loading == lazy_loading ) return;

	this->lazy_loading = lazy_loading;
	NotifyChanged();
}

auto Image::ValidateSource( std::string_view source ) -> void
{
	if( !detail::IsSafeResourceURL( source, false ) ) throw tatzel::Exception{ "Unsupported Image source" };
}

}
