#pragma once

#include "ComponentAnnotations.h"
#include "ScriptableEntity.h"
#include <memory>
#include <functional>

namespace Engine
{
    STRUCT(COMPONENT, DISPLAY_NAME("STRUCT_NATIVE_SCRIPT"), CATEGORY("CATEGORY_CORE"))
        struct NativeScriptComponent
    {
        REFLECT_BODY()

            std::unique_ptr<ScriptableEntity> Instance = nullptr;
        std::function<std::unique_ptr<ScriptableEntity>()> InstantiateScript;

        template<typename T>
        void Bind()
        {
            static_assert(std::is_base_of_v<ScriptableEntity, T>, "T must derive from ScriptableEntity");
            InstantiateScript = []() { return std::make_unique<T>(); };
        }
    };
}