#pragma once

#include <reflection/Annotations.h>

namespace inheritance_diagnostic
{
    STRUCT()
        struct Base {};

    CLASS()
        class MissingFriend : private Base {};
}
