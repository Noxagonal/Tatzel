#include "Showcases.hpp"


namespace example {

auto BuildFailureShowcase( tatzel::PageBuilder<> ui ) -> void
{
	ui.Heading( "Partial page" );
	throw tatzel::Exception{ "Expected page preparation failure" };
}

}
