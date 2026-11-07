module;

#include <cassert>

module UI.Element.ElementLifecycle;

namespace tatzel::ui {

auto ElementDispatchContext::Leave() noexcept -> void
{
	assert( depth > 0 );
	--depth;
	if( depth == 0 && on_idle ) on_idle();
}

}
