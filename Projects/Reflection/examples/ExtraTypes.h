#pragma once

#include <reflection/Annotations.h>
#include "ExampleMetadata.h"

namespace example::extra
{
    STRUCT(DISPLAY_NAME("Counter"))
    struct Counter
    {
        PROPERTY(DISPLAY_NAME("Value"))
            int Value = 3;
    };

    FUNCTION(CATEGORY("Math"))
        inline int Twice(int value)
    {
        return value * 2;
    }
}