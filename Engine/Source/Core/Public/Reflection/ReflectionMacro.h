#pragma once

#include "Types.h"
#include "Functions.h"
#include "Macro.h"
#include "Interface/Result.h"
#include "Interface/EnumBit.h"
#include "TypeIdentityInterface.h"

#include <reflection/Annotations.h>

#define REFLECT_CLASS() \
    REFLECT_BODY() \
    DECLARE_TYPE_ID()

#define REFLECT_BASE() REFLECT_CLASS()

#define COLOR COLOR_PROPERTY