#pragma once

#include "GraphNode.h"
#include "GraphLink.h"
#include <reflection/Annotations.h>

namespace Engine
{

	STRUCT()
	struct GraphData
	{
		static constexpr std::uint32_t CurrentVersion = 1;

		PROPERTY()
		std::uint32_t Version = CurrentVersion;

		PROPERTY()
		EGraphDomain Domain = EGraphDomain::Logic;

		PROPERTY()
		GraphNodeId NextNodeId = 1;

		PROPERTY()
		GraphLinkId NextLinkId = 1;

		PROPERTY()
		std::vector<GraphNode> Nodes;

		PROPERTY()
		std::vector<GraphLink> Links;
	};

}