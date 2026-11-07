module;

#include <string_view>

module UI.Element.Link;

import UI.Diagnostics;
import UI.ResourceURL;

namespace tatzel::ui {

Link::Link(
	std::string_view id,
	LogicalElement* parent,
	std::string_view text,
	std::string_view url,
	LinkTarget target
) :
	Text{ id, parent, text },
	url{ url },
	target{ target }
{
	ValidateURL( url );
	ValidateTarget( target );
}

auto Link::SetURL( std::string_view url ) -> void
{
	ValidateURL( url );
	if( this->url == url ) return;

	this->url = url;
	NotifyChanged();
}

auto Link::SetTarget( LinkTarget target ) -> void
{
	ValidateTarget( target );
	if( this->target == target ) return;

	this->target = target;
	NotifyChanged();
}

auto Link::ValidateURL( std::string_view url ) -> void
{
	if( !detail::IsSafeResourceURL( url, true ) ) throw tatzel::Exception{ "Unsupported Link URL" };
}

auto Link::ValidateTarget( LinkTarget target ) -> void
{
	if( target != LinkTarget::SameContext && target != LinkTarget::NewTab ) throw tatzel::Exception{ "Invalid Link target" };
}

}
