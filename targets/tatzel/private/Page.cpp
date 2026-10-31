module;

#include <functional>
#include <string_view>
#include <utility>

module UI.Page;

import UI.PageBuilder;

tatzel::Page::Page(
	std::string_view route_path,
	std::function<void( PageBuilder<> )>&& page_builder_fn
) :
	route_path{ route_path },
	page_builder_fn{ std::move( page_builder_fn ) }
{}
