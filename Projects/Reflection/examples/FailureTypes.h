#pragma once

#include <reflection/Annotations.h>

namespace batch_failure
{
    STRUCT()
        struct ShouldNotAppear
    {
        PROPERTY()
            int Value = 0;
    };

    FUNCTION()
        inline void Duplicate();

    FUNCTION()
        inline void Duplicate()
    {
    }
}