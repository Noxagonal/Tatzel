module;

#include "PCH.hpp"

module UI.ClientElementTree;

import UI.ClientConnection;



tatzel::ClientElementTree::ClientElementTree() :
	root{
		"root",
		nullptr,
		std::span<const dom::ElementPart>{ {
			dom::ElementPart{
				"root|main",
				"",
				"main",
				std::span<const std::string_view>{ { "container" } } }
			} },
			0,
			0
		}
{}

tatzel::ClientElementTree::~ClientElementTree()
{}
