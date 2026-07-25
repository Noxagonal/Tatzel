module;

#include "PCH.hpp"

module UI.ClientDOMTree;

import UI.ClientConnection;



tatzel::ClientDOMTree::ClientDOMTree() :
	root{
		"root",
		nullptr,
		std::span<const dom::ElementPart>{ {
			dom::ElementPart{
				"root|base",
				"",
				"main",
				std::span<const std::string_view>{ { "container" } } }
			} },
			0,
			0
		}
{}

tatzel::ClientDOMTree::~ClientDOMTree()
{}
