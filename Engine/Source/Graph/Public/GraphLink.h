#pragma once

#include "GraphPin.h"

namespace Engine
{

struct GraphLink
{
	GraphLinkId Id = 0;
	GraphPinReference From;
	GraphPinReference To;
};

}
