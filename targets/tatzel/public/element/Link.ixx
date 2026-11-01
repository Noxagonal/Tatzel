/// @file
/// @brief Logical plain-text navigation link.
module;

#include <string>
#include <string_view>

export module UI.Element.Link;

import UI.Diagnostics;
import UI.ResourceURL;
export import UI.Element.Text;

namespace tatzel::ui {

/// @brief Browser context used for ordinary navigation.
export enum class LinkTarget
{
	SameContext, ///< Navigate in the current browsing context.
	NewTab, ///< Request a new context with opener access disabled.
};

/// @brief Page-owned plain-text link with native browser navigation.
/// @details Text and URL are owned. Relative, HTTP, HTTPS, mailto, and tel URLs
/// are accepted. Mutations run on the owning page's event loop.
export class Link : public Text
{
public:
	/// @brief Creates a native navigation link.
	/// @param id Unique logical ID.
	/// @param parent Owning logical parent.
	/// @param text Visible plain text, copied before returning.
	/// @param url Destination URL, copied before returning; empty is permitted.
	/// @param target Requested browsing context.
	/// @pre parent is nonnull.
	/// @post Text, destination, and target are stored; the adapter supplies parts.
	/// @throws tatzel::Exception If the URL or target is unsupported.
	Link(
		std::string_view id,
		LogicalElement* parent,
		std::string_view text,
		std::string_view url,
		LinkTarget target = LinkTarget::SameContext
	) :
		Text{ id, parent, text },
		url{ url },
		target{ target }
	{
		ValidateURL( url );
		ValidateTarget( target );
	}

	/// @brief Reads the destination.
	/// @return Borrowed URL, invalidated by URL mutation or destruction.
	/// @pre None.
	/// @post State is unchanged.
	auto GetURL() const noexcept -> std::string_view { return url; }

	/// @brief Replaces the destination without navigating immediately.
	/// @param url Supported destination URL, copied before returning.
	/// @pre Called on the owning event loop.
	/// @post Changed state is stored and a bound adapter queues its destination update.
	/// @throws tatzel::Exception If the URL is unsupported; existing state is unchanged.
	auto SetURL( std::string_view url ) -> void
	{
		ValidateURL( url );
		if( this->url == url ) return;

		this->url = url;
		NotifyChanged();
	}

	/// @brief Reads the requested browser context.
	/// @return Current semantic navigation target.
	/// @pre None.
	/// @post State is unchanged.
	auto GetTarget() const noexcept -> LinkTarget { return target; }

	/// @brief Replaces the requested browser context.
	/// @param target Supported navigation target.
	/// @pre Called on the owning event loop.
	/// @post Changed state is stored and the adapter queues target and opener policy updates.
	/// @throws tatzel::Exception If target is undefined; state is unchanged.
	auto SetTarget( LinkTarget target ) -> void
	{
		ValidateTarget( target );
		if( this->target == target ) return;

		this->target = target;
		NotifyChanged();
	}

private:
	/// @brief Validates supported navigation schemes.
	/// @param url Candidate destination.
	/// @pre None.
	/// @post Candidate state is unchanged.
	/// @throws tatzel::Exception If the URL contains whitespace, controls, backslashes, or an unsupported scheme.
	static auto ValidateURL( std::string_view url ) -> void
	{
		if( !detail::IsSafeResourceURL( url, true ) ) throw tatzel::Exception{ "Unsupported Link URL" };
	}

	/// @brief Validates a semantic navigation context.
	/// @param target Candidate target.
	/// @pre None.
	/// @post Candidate state is unchanged.
	/// @throws tatzel::Exception If target is undefined.
	static auto ValidateTarget( LinkTarget target ) -> void
	{
		if( target != LinkTarget::SameContext && target != LinkTarget::NewTab ) throw tatzel::Exception{ "Invalid Link target" };
	}

	std::string url;
	LinkTarget target;
};

}
