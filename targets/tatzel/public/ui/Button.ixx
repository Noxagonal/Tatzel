module;

#include <string_view>
#include <string>
#include <functional>
#include <span>

export module UI.DOM.Button;

export import UI.UI.LogicalElement;
export import UI.UI.Interface.Clickable;

import UI.Property;


namespace tatzel::ui {


export
class Button : public LogicalElement, public interface::Clickable
{
public:

	using LogicalElement::LogicalElement;

	inline Button(
		std::string_view id,
		LogicalElement* parent
	) :
		LogicalElement{ id, parent }
	{}

	inline virtual auto InvokeOnClick() -> void override
	{
		if( on_click ) on_click();
	}

	Property<std::string> text = std::string{ "Button" };
	std::function<void()> on_click;
};


}
