module;

#include <array>
#include <string_view>
#include <vector>

module UI.ClientElementTree;

import UI.DOM.ElementPart;

tatzel::ClientElementTree::ClientElementTree() :
	root{
		"root", nullptr,
		std::vector<dom::ElementPart>{
			dom::ElementPart{ "root|main", "", "main", std::array<std::string_view, 1>{ "container" } }
		},
		0, 0
	}
{}
