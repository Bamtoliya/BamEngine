#pragma once

#pragma push_macro("new")
#undef new

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <imgui.h>
#include <imgui_internal.h>

#pragma push_macro("IMVIEWGUIZMO_IMPLEMENTATION")
#undef IMVIEWGUIZMO_IMPLEMENTATION
#define IMVIEWGUIZMO_IMPLEMENTATION

#pragma push_macro("ImLengthSqr")
#undef ImLengthSqr
#define ImLengthSqr BamImViewGuizmoLengthSqr

#include "ImViewGuizmo.h"

#pragma pop_macro("ImLengthSqr")
#pragma pop_macro("IMVIEWGUIZMO_IMPLEMENTATION")
#pragma pop_macro("new")