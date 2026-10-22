#pragma once

#include "GraphPin.h"
#include <reflection/Annotations.h>

namespace Engine
{

	STRUCT()
	struct GraphLink
	{
		PROPERTY()
		GraphLinkId Id = 0;

		PROPERTY()
		GraphPinReference From;

		PROPERTY()
		GraphPinReference To;
	};

}
