#pragma once

#include <reflection/Annotations.h>

#define COMPONENT META("Component", true)
#define REQUIRE_COMPONENTS(...) META("RequireComponents", #__VA_ARGS__)