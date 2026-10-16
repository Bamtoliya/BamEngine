#pragma once

#include <reflection/Annotations.h>

// Use inside FUNCTION(...) / PROPERTY(...); these are not declaration markers.
#define GRAPH_ID(value) META("Graph.Id", value)
#define GRAPH_PURE(id) META("Graph.Pure", true), GRAPH_ID(id)
#define GRAPH_CALLABLE(id) META("Graph.Callable", true), GRAPH_ID(id)
#define GRAPH_READONLY META("Graph.ReadOnly", true)
#define GRAPH_READWRITE META("Graph.ReadWrite", true)

// Parameter names and stable keys are ordinary string literals.
#define GRAPH_PIN_KEY(parameter, key) META("Graph.PinKey." parameter, key)
#define GRAPH_PIN_NAME(parameter, name) META("Graph.PinName." parameter, name)
#define GRAPH_PIN_DEFAULT(parameter, value) META("Graph.PinDefault." parameter, value)
