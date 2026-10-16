#pragma once

#include "GraphRegistry.h"

namespace reflection { class Registry; }

namespace Engine
{

// Creates Logic schemas only. Failure leaves the destination unchanged.
// Does not own reflection descriptors, bind scene objects, invoke functions or write properties.
[[nodiscard]] ENGINE_API GraphDiagnostic RegisterReflectedGraphNodes(GraphRegistry& graph,
	const reflection::Registry& reflection);

}
