#include "Showcases.hpp"
#include "../TableDemo.hpp"


namespace example {

auto BuildTablesShowcase( tatzel::PageBuilder<> ui ) -> void
{
	ui.Component<example::TableDemo>();
}

}
