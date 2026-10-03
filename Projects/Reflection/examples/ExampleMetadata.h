#pragma once

#include "ExampleExtraMetadata.h"

#define NAME(value) META("DisplayName", value)
#define CATEGORY(value) META("Category", value)
#define EDITABLE META("Editable", true)

#define RANGE(min, max) \
    META("RangeMin", min), META("RangeMax", max)