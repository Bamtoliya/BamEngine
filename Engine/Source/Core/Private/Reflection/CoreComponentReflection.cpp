#include "Reflection/CoreComponentReflection.h"

#include "Components/NameComponent.h"
#include "Components/TagComponent.h"
#include "Components/IDComponent.h"
#include "Components/FlagComponent.h"
#include "Components/TransformComponent.h"
#include "Components/WorldTransformComponent.h"
#include "BamCoreComponents.gen.h"
#include <string_view>

#include <iostream>

namespace
{
    struct CoreComponentReflectionState
    {
        reflection::Registry Registry;
        Engine::ComponentReflectionBindings Bindings{ Registry };
        bool Ready = false;

        CoreComponentReflectionState()
        {
            reflection::RegistrationResult failure;

            if (!reflection_generated::Register_BamCoreComponents(Registry, failure))
            {
                std::cerr << "Core component reflection registration failed: "
                    << reflection::ToString(failure.Error) << ", owner: " << failure.Owner
                    << ", member: " << failure.Member << '\n';
                return;
            }

            Ready = reflection_generated::ForEach_BamCoreComponents_Type([&]<typename T>()
            {
                return Bindings.Register<T>();
            });

            if (Ready)
            {
                Ready = reflection_generated::ForEach_BamCoreComponents_PropertyType(
                    [&]<typename Owner, typename Value>(std::string_view name)
                {
                    const auto* type = Registry.FindType<Owner>();
                    const auto* property = type != nullptr ? type->FindProperty(name) : nullptr;

                    if (property == nullptr || !property->IsValueType<Value>())
                    {
                        std::cerr << "Reflected property type mismatch: " << name << '\n';
                        return false;
                    }

                    return true;
                });
            }

            if (!Ready)
            {
                std::cerr << "Core component reflection binding failed.\n";
            }
        }
    };

    const CoreComponentReflectionState& GetState()
    {
        static const CoreComponentReflectionState state;
        return state;
    }
}

namespace Engine
{
    const reflection::Registry* GetCoreComponentReflectionRegistry()
    {
        const auto& state = GetState();
        return state.Ready ? &state.Registry : nullptr;
    }

    const ComponentReflectionBindings* GetCoreComponentReflectionBindings()
    {
        const auto& state = GetState();
        return state.Ready ? &state.Bindings : nullptr;
    }
}