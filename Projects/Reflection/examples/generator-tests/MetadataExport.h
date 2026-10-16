#pragma once

#include <reflection/Annotations.h>
#include <cfloat>

namespace metadata_export_test
{
    STRUCT(DISPLAY_NAME("TYPE_TEST"), CATEGORY("CATEGORY_TEST"), TOOLTIP("Line one,\n\"quoted\" text"))
    struct Sample
    {
        PROPERTY(DISPLAY_NAME("PROP_TEST"), RANGE(-FLT_MAX, FLT_MAX, 0.01f), META("Extended", 1.0L))
        float Value = 0.0f;

        PROPERTY(TOOLTIP("FORWARD_POINTER_HELP"))
        class Forward* Pointer = nullptr;

        FUNCTION(DISPLAY_NAME("FUNCTION_TEST"))
        void Reset() { Value = 0.0f; }
    };

    ENUM(DISPLAY_NAME("ENUM_TEST"))
    enum class Mode
    {
        First,
        Second
    };
}
