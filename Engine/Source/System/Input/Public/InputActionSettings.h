#pragma once

#include <reflection/Annotations.h>
#include <string>
#include <vector>

namespace Engine
{
    STRUCT()
        struct InputAction
    {
        PROPERTY()
            std::string Id;

        PROPERTY()
            std::string Type = "Button";
    };

    STRUCT()
        struct InputBinding
    {
        PROPERTY()
            std::string ActionId;

        PROPERTY()
            std::string Key;
    };

    STRUCT()
        struct InputActionMap
    {
        PROPERTY()
            std::string Id;

        PROPERTY()
            std::vector<InputAction> Actions;

        PROPERTY()
            std::vector<InputBinding> Bindings;
    };

    STRUCT()
        struct InputActionSettings
    {
        PROPERTY()
            std::vector<InputActionMap> Maps;
    };
}