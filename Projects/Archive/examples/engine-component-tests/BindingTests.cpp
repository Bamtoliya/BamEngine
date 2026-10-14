#include "TestSupport.h"
#include "EngineComponents.gen.h"

#include "Components/NameComponent.h"
#include "Components/TagComponent.h"
#include "Components/IDComponent.h"
#include "Components/FlagComponent.h"
#include "Components/TransformComponent.h"
#include "Components/WorldTransformComponent.h"
#include "Reflection/ComponentReflectionBindings.h"

#include <reflection/Reflection.h>

#include <cstddef>
#include <iostream>
#include <string>

namespace archive_engine_component_tests
{
    int CheckEngineComponentBindings(const reflection::Registry& registry)
    {
        using VisitResult = Engine::ComponentVisitResult;
        using AccessError = reflection::PropertyAccessError;

        Engine::ComponentReflectionBindings bindings(registry);

        if (!bindings.Register<Engine::NameComponent>() ||
            !bindings.Register<Engine::TagComponent>() ||
            !bindings.Register<Engine::IDComponent>() ||
            !bindings.Register<Engine::FlagComponent>() ||
            !bindings.Register<Engine::TransformComponent>() ||
            !bindings.Register<Engine::WorldTransformComponent>())
        {
            return Fail("Engine ECS binding registration failed.");
        }

        struct UnregisteredComponent
        {
            int Value = 0;
        };

        if (bindings.Register<Engine::NameComponent>() || bindings.Register<UnregisteredComponent>())
        {
            return Fail("Invalid engine ECS binding registration was accepted.");
        }

        entt::registry world;
        const auto entity = world.create();
        world.emplace<Engine::NameComponent>(entity);
        world.emplace<Engine::TransformComponent>(entity);

        const std::wstring expectedName = L"리플렉션 엔티티";
        const glm::vec3 expectedPosition{ 1.0f, 2.0f, 3.0f };
        std::size_t visited = 0;
        bool valid = true;

        const auto mutableResult = bindings.ForEach(world, entity,
            [&](const reflection::TypeInfo& type, const reflection::ObjectView& object)
            {
                ++visited;
                valid = valid && !object.IsReadOnly() && type.GetCppType() == object.GetCppType();

                if (object.Is<Engine::NameComponent>())
                {
                    const auto* property = type.FindProperty("name");
                    valid = valid && property != nullptr &&
                        property->TryWriteValue(object, reflection::ValueView::From(expectedName)) == AccessError::None;
                }
                else if (object.Is<Engine::TransformComponent>())
                {
                    const auto* property = type.FindProperty("position");
                    valid = valid && property != nullptr &&
                        property->TryWriteValue(object, reflection::ValueView::From(expectedPosition)) == AccessError::None;
                }
                else
                {
                    valid = false;
                }

                return valid;
            });

        const auto& position = world.get<Engine::TransformComponent>(entity).position;

        if (mutableResult != VisitResult::Completed || !valid || visited != 2 ||
            world.get<Engine::NameComponent>(entity).name != expectedName ||
            position.x != expectedPosition.x || position.y != expectedPosition.y ||
            position.z != expectedPosition.z)
        {
            return Fail("Engine ECS reflected editing failed.");
        }

        const entt::registry& constWorld = world;
        const std::wstring replacement = L"변경되면 안 됨";
        visited = 0;
        valid = true;

        const auto constResult = bindings.ForEach(constWorld, entity,
            [&](const reflection::TypeInfo& type, const reflection::ObjectView& object)
            {
                ++visited;
                valid = valid && object.IsReadOnly();

                if (object.Is<Engine::NameComponent>())
                {
                    const auto* property = type.FindProperty("name");

                    if (property == nullptr)
                    {
                        valid = false;
                        return false;
                    }

                    const auto read = property->TryReadValue(object);
                    const auto* name = read.Value.Get<std::wstring>();

                    valid = valid && read.Error == AccessError::None && name != nullptr && *name == expectedName;
                    valid = valid &&
                        property->TryWriteValue(object, reflection::ValueView::From(replacement)) ==
                        AccessError::ReadOnlyObject;
                }

                return valid;
            });

        if (constResult != VisitResult::Completed || !valid || visited != 2 ||
            world.get<Engine::NameComponent>(entity).name != expectedName)
        {
            return Fail("Engine ECS const access checks failed.");
        }

        visited = 0;
        const auto stopped = bindings.ForEach(world, entity,
            [&](const reflection::TypeInfo&, const reflection::ObjectView&)
            {
                ++visited;
                return false;
            });

        if (stopped != VisitResult::Stopped || visited != 1)
        {
            return Fail("Engine ECS visitor stopping failed.");
        }

        world.remove<Engine::NameComponent>(entity);
        visited = 0;
        valid = true;

        const auto afterRemoval = bindings.ForEach(world, entity,
            [&](const reflection::TypeInfo&, const reflection::ObjectView& object)
            {
                ++visited;
                valid = valid && object.Is<Engine::TransformComponent>();
                return valid;
            });

        if (afterRemoval != VisitResult::Completed || !valid || visited != 1)
        {
            return Fail("Engine ECS component removal checks failed.");
        }

        const auto emptyEntity = world.create();
        visited = 0;

        auto countVisitor = [&](const reflection::TypeInfo&, const reflection::ObjectView&)
            {
                ++visited;
                return true;
            };

        if (bindings.ForEach(world, emptyEntity, countVisitor) != VisitResult::Completed || visited != 0)
        {
            return Fail("Engine ECS empty entity checks failed.");
        }

        world.destroy(entity);

        if (bindings.ForEach(world, entity, countVisitor) != VisitResult::InvalidEntity || visited != 0)
        {
            return Fail("Engine ECS invalid entity checks failed.");
        }

        std::cout << "Engine ECS reflection binding checks passed.\n";
        std::cout << "Engine ECS const access checks passed.\n";
        std::cout << "Engine ECS lifetime boundary checks passed.\n";
        return 0;
    }
}
