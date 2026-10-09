#include <archive/JsonArchive.h>
#include <reflection/Reflection.h>
#include <reflection_archive/ReflectionArchiveAdapter.h>

#include "Components/NameComponent.h"
#include "Components/TagComponent.h"
#include "Components/IDComponent.h"
#include "Components/FlagComponent.h"
#include "EngineComponents.gen.h"
#include "Components/TransformComponent.h"
#include "Components/WorldTransformComponent.h"
#include "Adapters/GlmArchiveAdapter.h"
#include "Reflection/ComponentReflectionBindings.h"

#include <array>
#include <limits>
#include <filesystem>
#include <iostream>
#include <string>
#include <system_error>
#include <variant>
#include <unordered_set>

namespace
{
    int Fail(const char* message)
    {
        std::cerr << message << '\n';
        return 1;
    }
    int CheckEngineValueComponents(const reflection::Registry& registry, const std::filesystem::path& directory)
    {
        const auto* tagType = registry.FindType<Engine::TagComponent>();
        const auto* idType = registry.FindType<Engine::IDComponent>();
        const auto* flagType = registry.FindType<Engine::FlagComponent>();
        const auto* enumeration = registry.FindEnum<Engine::EEntityFlag>();

        if (tagType == nullptr || idType == nullptr || flagType == nullptr || enumeration == nullptr)
        {
            return Fail("Engine value component lookup failed.");
        }

        const auto* idProperty = idType->FindProperty("id");
        const auto* readOnlyMetadata = idProperty ? idProperty->Metadata.Find("ReadOnly") : nullptr;
        const auto* readOnly = readOnlyMetadata ? std::get_if<bool>(readOnlyMetadata) : nullptr;
        const auto* bitmaskMetadata = enumeration->Metadata.Find("Bitmask");
        const auto* bitmask = bitmaskMetadata ? std::get_if<bool>(bitmaskMetadata) : nullptr;

        if (readOnly == nullptr || !*readOnly || bitmask == nullptr || !*bitmask)
        {
            return Fail("Engine component metadata checks failed.");
        }

        Engine::TagComponent sourceTags;
        sourceTags.tags = { L"플레이어", L"테스트 \U0001F600" };

        Engine::IDComponent sourceID;
        sourceID.id = 123;

        Engine::FlagComponent sourceFlags;
        sourceFlags.flags = Engine::EEntityFlag::Active | Engine::EEntityFlag::Paused;

        if (!Engine::HasFlag(sourceFlags.flags, Engine::EEntityFlag::Active) ||
            !Engine::HasFlag(sourceFlags.flags, Engine::EEntityFlag::Paused) ||
            Engine::HasFlag(sourceFlags.flags, Engine::EEntityFlag::Visible))
        {
            return Fail("Engine bit flag operators failed.");
        }

        archive_reflection::PropertyCodecs codecs;

        if (!codecs.RegisterArray<std::unordered_set<std::wstring>>())
        {
            return Fail("Engine tag codec registration failed.");
        }

        archive::JsonArchive writer;

        if (!archive_reflection::WriteObject(
            writer, "Tags", *tagType, reflection::ObjectView::From(sourceTags), &registry, &codecs) ||
            !archive_reflection::WriteObject(
                writer, "ID", *idType, reflection::ObjectView::From(sourceID), &registry) ||
            !archive_reflection::WriteObject(
                writer, "Flags", *flagType, reflection::ObjectView::From(sourceFlags), &registry))
        {
            return Fail("Engine value component writing failed.");
        }

        const auto path = directory / "engine-value-components.json";
        archive::JsonArchive reader;
        Engine::TagComponent restoredTags;
        Engine::IDComponent restoredID;
        Engine::FlagComponent restoredFlags;
        restoredFlags.flags = Engine::EEntityFlag::None;

        if (!writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Tags", *tagType, restoredTags, &registry, &codecs) ||
            !archive_reflection::ReadObject(reader, "ID", *idType, restoredID, &registry) ||
            !archive_reflection::ReadObject(reader, "Flags", *flagType, restoredFlags, &registry) ||
            restoredTags.tags != sourceTags.tags || restoredID.id != sourceID.id ||
            restoredFlags.flags != sourceFlags.flags)
        {
            return Fail("Engine value component round-trip failed.");
        }

        archive::JsonArchive failureReader;

        if (!failureReader.Parse(R"({"Tags":{"tags":["Changed",42]},"Flags":{"flags":300}})"))
        {
            return Fail("Cannot prepare engine value component reading failure.");
        }

        const auto beforeTags = restoredTags.tags;
        const auto beforeFlags = restoredFlags.flags;
        const auto tagFailure = archive_reflection::ReadObject(
            failureReader, "Tags", *tagType, restoredTags, &registry, &codecs);
        const auto flagFailure = archive_reflection::ReadObject(
            failureReader, "Flags", *flagType, restoredFlags, &registry);

        if (tagFailure.Code != archive::ArchiveErrorCode::InvalidValue || tagFailure.Field != "tags" ||
            restoredTags.tags != beforeTags ||
            flagFailure.Code != archive::ArchiveErrorCode::InvalidValue || flagFailure.Field != "flags" ||
            restoredFlags.flags != beforeFlags)
        {
            return Fail("Failed engine value component reading changed the destination.");
        }

        std::cout << "Engine value component generation checks passed.\n";
        std::cout << "Engine value component archive checks passed.\n";
        std::cout << "Engine value component failure checks passed.\n";
        return 0;
    }

    bool SameTransform(const Engine::TransformComponent& lhs, const Engine::TransformComponent& rhs)
    {
        const auto sameVector = [](const glm::vec3& a, const glm::vec3& b)
            {
                return a.x == b.x && a.y == b.y && a.z == b.z;
            };

        return sameVector(lhs.position, rhs.position) && sameVector(lhs.scale, rhs.scale) &&
            lhs.rotation.x == rhs.rotation.x && lhs.rotation.y == rhs.rotation.y &&
            lhs.rotation.z == rhs.rotation.z && lhs.rotation.w == rhs.rotation.w;
    }

    bool SameMatrix(const glm::mat4& lhs, const glm::mat4& rhs)
    {
        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                if (lhs[column][row] != rhs[column][row])
                {
                    return false;
                }
            }
        }

        return true;
    }

    int CheckEngineTransformComponents(const reflection::Registry& registry, const std::filesystem::path& directory)
    {
        const auto* type = registry.FindType<Engine::TransformComponent>();
        const auto* worldType = registry.FindType<Engine::WorldTransformComponent>();

        if (type == nullptr || worldType == nullptr)
        {
            return Fail("Engine transform type lookup failed.");
        }

        const auto* position = type->FindProperty("position");
        const auto* rotation = type->FindProperty("rotation");
        const auto* worldMatrix = worldType->FindProperty("worldMatrix");

        if (position == nullptr || rotation == nullptr || worldMatrix == nullptr)
        {
            return Fail("Engine transform property lookup failed.");
        }

        const auto* metadata = worldMatrix->Metadata.Find("ReadOnly");
        const auto* readOnly = metadata ? std::get_if<bool>(metadata) : nullptr;

        if (readOnly == nullptr || !*readOnly)
        {
            return Fail("Engine world transform metadata lookup failed.");
        }

        Engine::TransformComponent source;

        if (source.rotation.x != 0.0f || source.rotation.y != 0.0f ||
            source.rotation.z != 0.0f || source.rotation.w != 1.0f)
        {
            return Fail("Engine default rotation is not identity.");
        }

        const auto object = reflection::ObjectView::From(source);
        const glm::vec3 desiredPosition{ 1.0f, 2.0f, 3.0f };
        const auto desiredRotation = glm::quat::wxyz(0.8f, 0.0f, 0.0f, 0.6f);

        if (position->TryWriteValue(object, reflection::ValueView::From(desiredPosition)) !=
            reflection::PropertyAccessError::None ||
            rotation->TryWriteValue(object, reflection::ValueView::From(desiredRotation)) !=
            reflection::PropertyAccessError::None)
        {
            return Fail("Generated engine transform property writing failed.");
        }

        source.scale = glm::vec3{ 2.0f, 3.0f, 4.0f };
        Engine::WorldTransformComponent sourceWorld;

        for (int column = 0; column < 4; ++column)
        {
            for (int row = 0; row < 4; ++row)
            {
                sourceWorld.worldMatrix[column][row] = static_cast<float>(column * 4 + row + 1);
            }
        }

        archive_reflection::PropertyCodecs codecs;

        if (!codecs.Register<glm::vec3>(archive_glm::WriteVec3, archive_glm::ReadVec3) ||
            !codecs.Register<glm::quat>(archive_glm::WriteQuat, archive_glm::ReadQuat) ||
            !codecs.Register<glm::mat4>(archive_glm::WriteMat4, archive_glm::ReadMat4))
        {
            return Fail("Engine transform codec registration failed.");
        }

        archive::JsonArchive writer;

        if (!archive_reflection::WriteObject(writer, "Transform", *type, object, &registry, &codecs) ||
            !archive_reflection::WriteObject(
                writer, "WorldTransform", *worldType, reflection::ObjectView::From(sourceWorld), &registry, &codecs))
        {
            return Fail("Engine transform writing failed.");
        }

        const auto path = directory / "engine-transform-components.json";
        archive::JsonArchive reader;
        Engine::TransformComponent restored;
        Engine::WorldTransformComponent restoredWorld;

        if (!writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
            !archive_reflection::ReadObject(reader, "Transform", *type, restored, &registry, &codecs) ||
            !archive_reflection::ReadObject(
                reader, "WorldTransform", *worldType, restoredWorld, &registry, &codecs) ||
            !SameTransform(restored, source) || !SameMatrix(restoredWorld.worldMatrix, sourceWorld.worldMatrix))
        {
            return Fail("Engine transform round-trip failed.");
        }

        if (!reader.BeginReadObject("WorldTransform"))
        {
            return Fail("Cannot inspect engine matrix layout.");
        }

        std::array<float, 16> matrixElements{};
        const auto matrixRead = reader.ReadArray("worldMatrix", matrixElements);
        const auto matrixEnd = reader.EndObject();

        if (!matrixRead || !matrixEnd ||
            matrixElements[1] != sourceWorld.worldMatrix[0][1] ||
            matrixElements[4] != sourceWorld.worldMatrix[1][0])
        {
            return Fail("Engine matrix column order changed.");
        }

        archive::JsonArchive failureReader;
        const std::string failureJson =
            R"({"Transform":{"position":[9,8,7],"rotation":[0,0,1]},)"
            R"("WorldTransform":{"worldMatrix":[1,2]}})";

        if (!failureReader.Parse(failureJson))
        {
            return Fail("Cannot prepare engine transform reading failure.");
        }

        const auto beforeTransform = restored;
        const auto beforeWorld = restoredWorld;
        const auto transformFailure = archive_reflection::ReadObject(
            failureReader, "Transform", *type, restored, &registry, &codecs);
        const auto worldFailure = archive_reflection::ReadObject(
            failureReader, "WorldTransform", *worldType, restoredWorld, &registry, &codecs);

        if (transformFailure.Code != archive::ArchiveErrorCode::InvalidValue ||
            transformFailure.Field != "rotation" || !SameTransform(restored, beforeTransform) ||
            worldFailure.Code != archive::ArchiveErrorCode::InvalidValue ||
            worldFailure.Field != "worldMatrix" ||
            !SameMatrix(restoredWorld.worldMatrix, beforeWorld.worldMatrix))
        {
            return Fail("Failed engine transform reading changed the destination.");
        }

        std::string beforeWriteFailure;

        if (!writer.ToJson(beforeWriteFailure))
        {
            return Fail("Cannot capture the engine transform document.");
        }

        auto invalid = source;
        invalid.position.x = std::numeric_limits<float>::infinity();
        const auto writeFailure = archive_reflection::WriteObject(
            writer, "Transform", *type, reflection::ObjectView::From(invalid), &registry, &codecs);
        std::string afterWriteFailure;

        if (writeFailure.Code != archive::ArchiveErrorCode::InvalidValue || writeFailure.Field != "position" ||
            !writer.ToJson(afterWriteFailure) || afterWriteFailure != beforeWriteFailure)
        {
            return Fail("Failed engine transform writing changed the document.");
        }

        std::cout << "Engine transform generation checks passed.\n";
        std::cout << "Engine transform archive checks passed.\n";
        std::cout << "Engine transform failure checks passed.\n";
        return 0;
    }

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

int main()
{
    reflection::Registry registry;

    if (!reflection_generated::Register_EngineComponents(registry))
    {
        return Fail("Engine component registration failed.");
    }

    const auto* type = registry.FindType<Engine::NameComponent>();
    const auto* property = type != nullptr ? type->FindProperty("name") : nullptr;

    if (property == nullptr)
    {
        return Fail("Engine name property lookup failed.");
    }

    const auto* metadata = property->Metadata.Find("Editable");
    const auto* editable = metadata != nullptr ? std::get_if<bool>(metadata) : nullptr;

    if (editable == nullptr || !*editable)
    {
        return Fail("Engine name metadata generation failed.");
    }

    Engine::NameComponent source;
    source.name = L"플레이어 \U0001F600";
    archive::JsonArchive writer;

    if (!archive_reflection::WriteObject(
        writer, "NameComponent", *type, reflection::ObjectView::From(source)))
    {
        return Fail("Engine name component writing failed.");
    }

    const auto directory = std::filesystem::current_path() / "archive-example";
    std::error_code directoryError;
    std::filesystem::create_directories(directory, directoryError);

    if (directoryError)
    {
        return Fail("Cannot create the engine component archive directory.");
    }

    const auto path = directory / "engine-name-component.json";
    archive::JsonArchive reader;
    Engine::NameComponent restored;
    restored.name = L"Before";

    if (!writer.SaveToFile(path) || !reader.LoadFromFile(path) ||
        !archive_reflection::ReadObject(reader, "NameComponent", *type, restored) ||
        restored.name != source.name)
    {
        return Fail("Engine name component round-trip failed.");
    }

    archive::JsonArchive failureReader;

    if (!failureReader.Parse(R"({"NameComponent":{"name":42}})"))
    {
        return Fail("Cannot prepare engine component reading failure.");
    }

    const std::wstring beforeFailure = restored.name;
    const auto result = archive_reflection::ReadObject(failureReader, "NameComponent", *type, restored);

    if (result.Code != archive::ArchiveErrorCode::InvalidValue ||
        result.Field != "name" || restored.name != beforeFailure)
    {
        return Fail("Failed engine component reading changed the destination.");
    }

    if (CheckEngineValueComponents(registry, directory) != 0)
    {
        return 1;
    }

    if (CheckEngineTransformComponents(registry, directory) != 0)
    {
        return 1;
    }

    if (CheckEngineComponentBindings(registry) != 0)
    {
        return 1;
    }

    std::cout << "Engine name component archive checks passed.\n";
    std::cout << "Engine name component archive failure checks passed.\n";
    return 0;
}