#pragma once

#include <reflection/Annotations.h>
#include "ExampleMetadata.h"

namespace example::extra
{
    STRUCT(NAME("Counter"))
        struct Counter
    {
        PROPERTY(NAME("Value"))
            int Value = 3;
    };

    FUNCTION(CATEGORY("Math"))
        inline int Twice(int value)
    {
        return value * 2;
    }
}