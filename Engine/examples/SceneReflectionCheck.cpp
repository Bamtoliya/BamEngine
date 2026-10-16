#include "Scene.h"
#include "Components/Core/NameComponent.h"
#include "Components/Core/TransformComponent.h"
#include "Reflection/ReflectionRegistry.h"
#include <archive/JsonArchive.h>
#include <archive/BeveArchive.h>
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "PhysicsComponent.h"

#include <iostream>
#include <memory>
#include <string>
#include <cmath>

#include "Components/Core/WorldTransformComponent.h"
#include "Adapters/GlmArchiveAdapter.h"

namespace
{
    struct SceneDeleter
    {
        void operator()(Engine::Scene* scene) const
        {
            if (scene != nullptr)
            {
                scene->Release();
            }
        }
    };

    int Fail(const char* message)
    {
        std::cerr << message << '\n';
        return 1;
    }

    bool SameOrientation(const glm::quat& left, const glm::quat& right)
    {
        const glm::vec3 axes[] =
        {
            glm::vec3(1.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f),
            glm::vec3(0.0f, 0.0f, 1.0f)
        };

        for (const auto& axis : axes)
        {
            if (glm::length(left * axis - right * axis) > 1.0e-4f)
            {
                return false;
            }
        }

        return true;
    }

    int CheckTransformRotation()
    {
        Engine::TransformComponent transform;
        const glm::vec3 entered(25.0f, 450.0f, -35.0f);

        if (!transform.SetEulerRotation(entered) || transform.eulerRotation != entered ||
            !SameOrientation(transform.rotation, glm::quat(glm::radians(entered))))
        {
            return Fail("Transform Euler input preservation failed.");
        }

        const glm::quat opposite = -transform.rotation;

        if (!transform.SetLocalRotation(opposite) || transform.eulerRotation != entered)
        {
            return Fail("Transform quaternion sign preservation failed.");
        }

        const glm::vec3 initialEuler(30.0f, 20.0f, 10.0f);
        const glm::vec3 axis(0.0f, 1.0f, 0.0f);
        const glm::quat delta = glm::angleAxis(glm::radians(40.0f), axis);

        if (!transform.SetEulerRotation(initialEuler))
        {
            return Fail("Transform rotation setup failed.");
        }

        const glm::quat initialRotation = transform.rotation;

        if (!transform.RotateLocal(axis, 40.0) ||
            !SameOrientation(transform.rotation, glm::normalize(initialRotation * delta)))
        {
            return Fail("Transform local rotation composition failed.");
        }

        if (!transform.SetEulerRotation(initialEuler) || !transform.RotateParentSpace(axis, 40.0) ||
            !SameOrientation(transform.rotation, glm::normalize(delta * initialRotation)))
        {
            return Fail("Transform parent-space rotation composition failed.");
        }

        if (!transform.SetEulerRotation(glm::vec3(0.0f)) || !transform.RotateParentSpace(axis, 720.0) ||
            glm::length(transform.eulerRotation - glm::vec3(0.0f, 720.0f, 0.0f)) > 0.01f ||
            !SameOrientation(transform.rotation, glm::quat::wxyz(1.0f, 0.0f, 0.0f, 0.0f)))
        {
            return Fail("Transform accumulated rotation failed.");
        }

        const glm::vec3 singularHint(30.0f, 90.0f, 20.0f);
        const glm::quat singularRotation = glm::quat(glm::radians(glm::vec3(31.0f, 90.0f, 20.0f)));

        if (!transform.SetEulerRotation(singularHint) || !transform.SetLocalRotation(singularRotation) ||
            glm::length(transform.eulerRotation - singularHint) > 2.0f ||
            !SameOrientation(transform.rotation, glm::quat(glm::radians(transform.eulerRotation))))
        {
            return Fail("Transform singular Euler hint continuity failed.");
        }

        if (!transform.SetEulerRotation(glm::vec3(0.0f, 45.0f, 0.0f)))
        {
            return Fail("Transform rejection setup failed.");
        }

        const glm::quat savedRotation = transform.rotation;
        const glm::vec3 savedEuler = transform.eulerRotation;

        if (transform.SetLocalRotation(glm::quat::wxyz(0.0f, 0.0f, 0.0f, 0.0f)) ||
            transform.RotateLocal(glm::vec3(0.0f), 10.0) ||
            transform.eulerRotation != savedEuler || !SameOrientation(transform.rotation, savedRotation))
        {
            return Fail("Transform invalid rotation rejection failed.");
        }

        transform.eulerRotation = glm::vec3(0.0f, 10.0f, 0.0f);
        transform.rotation = glm::quat(glm::radians(glm::vec3(0.0f, 90.0f, 0.0f)));

        if (transform.SynchronizeRotation() || transform.eulerRotation != savedEuler ||
            !SameOrientation(transform.rotation, savedRotation))
        {
            return Fail("Transform conflicting rotation recovery failed.");
        }

        std::cout << "Transform rotation API checks passed.\n";

        Engine::TransformComponent archiveSource;
        Engine::TransformComponent archiveRestored;
        const glm::vec3 archiveEuler(25.0f, 450.0f, -35.0f);

        if (!archiveSource.SetEulerRotation(archiveEuler) ||
            !archiveRestored.SetEulerRotation(glm::vec3(0.0f, 720.0f, 0.0f)) ||
            !archiveRestored.RestoreLocalRotation(archiveSource.rotation, archiveSource.eulerRotation) ||
            archiveRestored.eulerRotation != archiveEuler ||
            !SameOrientation(archiveRestored.rotation, archiveSource.rotation))
        {
            return Fail("Transform rotation restoration failed.");
        }

        const glm::vec3 editedEuler(10.0f, 30.0f, 20.0f);
        archiveRestored.eulerRotation = editedEuler;

        if (!archiveRestored.SynchronizeRotation() || archiveRestored.eulerRotation != editedEuler ||
            !SameOrientation(archiveRestored.rotation, glm::quat(glm::radians(editedEuler))))
        {
            return Fail("Transform editing after restoration failed.");
        }

        const auto beforeRestoreFailure = archiveRestored;

        if (archiveRestored.RestoreLocalRotation(glm::quat::wxyz(0.0f, 0.0f, 0.0f, 0.0f), archiveEuler) ||
            archiveRestored.rotation != beforeRestoreFailure.rotation ||
            archiveRestored.eulerRotation != beforeRestoreFailure.eulerRotation)
        {
            return Fail("Invalid restored rotation changed the transform.");
        }

        if (!archiveRestored.RestoreLocalRotation(archiveSource.rotation, glm::vec3(0.0f)) ||
            !SameOrientation(archiveRestored.rotation, archiveSource.rotation) ||
            !SameOrientation(archiveRestored.rotation, glm::quat(glm::radians(archiveRestored.eulerRotation))))
        {
            return Fail("Restored Euler hint correction failed.");
        }

        std::cout << "Transform rotation restore checks passed.\n";

        return 0;
    }
}

namespace
{
    int CheckUnifiedRegistry(const Engine::ReflectionRegistry& registry)
    {
        const auto* camera = registry.FindComponent(std::type_index(typeid(Engine::CameraComponent)));
        const auto* rigidBody = registry.FindComponent(std::type_index(typeid(Engine::RigidBodyComponent)));
        const auto* identity = registry.FindComponent(std::type_index(typeid(Engine::IDComponent)));
        const auto* worldTransform = registry.FindComponent(std::type_index(typeid(Engine::WorldTransformComponent)));
        if (camera == nullptr || camera->ReflectionType == nullptr || rigidBody == nullptr ||
            identity == nullptr || identity->UserAddable || worldTransform == nullptr || worldTransform->UserAddable)
            return Fail("Unified component registration lost a built-in operation or policy.");

        for (const auto& entry : registry.GetComponentEntries())
        {
            if (entry.ReflectionType != registry.GetRegistry().FindType(entry.Type))
                return Fail("Component reflection points outside the owning registry.");
        }

        entt::registry first;
        entt::registry second;
        const auto entity = first.create();
        const auto other = second.create();
        if (!camera->TryAdd(first, entity) || camera->TryAdd(first, entity) || camera->TryAdd(first, entt::null) ||
            !first.all_of<Engine::CameraComponent, Engine::TransformComponent,
                Engine::WorldTransformComponent, Engine::FlagComponent>(entity) || camera->Has(second, other))
            return Fail("Unified component addition lost dependencies, guards or world isolation.");

        const auto view = registry.GetComponentObject(std::as_const(first), camera->StorageId, entity);
        if (!view.Is<Engine::CameraComponent>() || !view.IsReadOnly() ||
            !registry.GetComponentObject(first, camera->StorageId, entity).Is<Engine::CameraComponent>())
            return Fail("Unified component object access failed.");

        const auto bodyEntity = first.create();
        if (!rigidBody->TryAdd(first, bodyEntity))
            return Fail("Rigid body addition failed.");
        struct UnregisteredStorage { int Value = 0; };
        first.emplace<UnregisteredStorage>(bodyEntity);
        archive::JsonArchive bodyWriter;
        std::string before;
        std::string after;
        if (!bodyWriter.Write("Keep", 17) || !bodyWriter.ToJson(before))
            return Fail("Unregistered storage test setup failed.");
        const auto bodyResult = registry.WriteComponents(bodyWriter, "Components", first, bodyEntity);
        if (bodyResult.Code != archive::ArchiveErrorCode::UnsupportedType ||
            !bodyWriter.ToJson(after) || after != before)
            return Fail("Unsupported component serialization silently succeeded or changed the document.");

        const auto* transform = registry.FindComponent(std::type_index(typeid(Engine::TransformComponent)));
        if (transform == nullptr)
            return Fail("Transform operation binding is missing.");
        entt::registry aliased;
        const auto aliasEntity = aliased.create();
        aliased.storage<Engine::NameComponent>(transform->StorageId).emplace(aliasEntity);
        if (transform->TryAdd(aliased, aliasEntity) ||
            registry.GetComponentObject(aliased, transform->StorageId, aliasEntity).IsValid())
            return Fail("A mismatched EnTT storage was treated as Transform.");

        archive::JsonArchive missing;
        const auto aliasWrite = registry.WriteComponent<Engine::TransformComponent>(
            missing, "Transform", aliased, aliasEntity);
        if (aliasWrite.Code != archive::ArchiveErrorCode::InvalidOperation)
            return Fail("Mismatched storage writing was not rejected before typed access.");
        const auto aliasResult = registry.ReadComponent<Engine::TransformComponent>(
            missing, "Transform", aliased, aliasEntity);
        if (aliasResult.Code != archive::ArchiveErrorCode::InvalidOperation)
            return Fail("Mismatched storage was not rejected before typed access.");

        const auto stopped = registry.ForEachComponent(first, entity,
            [](const reflection::TypeInfo&, const reflection::ObjectView&) { return false; });
        if (stopped != Engine::ComponentVisitResult::Stopped)
            return Fail("Component visitor stop was ignored.");
        return 0;
    }

    template<typename Archive>
    int CheckTransformArchive(const Engine::ReflectionRegistry& registry)
    {
        entt::registry sourceWorld;
        const auto sourceEntity = sourceWorld.create();
        auto& source = sourceWorld.emplace<Engine::TransformComponent>(sourceEntity);
        if (!source.SetLocalPosition(glm::vec3(3.0f, -2.0f, 7.0f)) ||
            !source.SetLocalScale(glm::vec3(-1.0f, 0.0f, 2.0f)) ||
            !source.SetEulerRotation(glm::vec3(25.0f, 450.0f, -35.0f)))
            return Fail("Transform archive setup failed.");

        Archive writer;
        Archive reader;
        std::string document;
        if (!registry.WriteComponent<Engine::TransformComponent>(writer, "Transform", sourceWorld, sourceEntity) ||
            !writer.Encode(document) || !reader.Decode(document))
            return Fail("Transform writing with engine-owned GLM codecs failed.");

        entt::registry restoredWorld;
        const auto restoredEntity = restoredWorld.create();
        if (!registry.ReadComponent<Engine::TransformComponent>(reader, "Transform", restoredWorld, restoredEntity) ||
            !restoredWorld.all_of<Engine::TransformComponent, Engine::WorldTransformComponent,
                Engine::FlagComponent>(restoredEntity))
            return Fail("Transform restoration did not create its required components.");

        auto& restored = restoredWorld.get<Engine::TransformComponent>(restoredEntity);
        if (restored.position != source.position || restored.scale != source.scale ||
            restored.eulerRotation != source.eulerRotation || !SameOrientation(restored.rotation, source.rotation) ||
            !restored.SynchronizeRotation() || restored.eulerRotation != source.eulerRotation)
            return Fail("Transform restoration lost values or rotation cache consistency.");

        archive::JsonArchive invalid;
        if (!invalid.Parse(R"({"Transform":{"position":[1,2,3],"rotation":[0,0,0,0],)"
            R"("eulerRotation":[0,90,0],"scale":[1,1,1]}})"))
            return Fail("Invalid quaternion test setup failed.");
        const auto previous = restored;
        const auto previousMatrix = restoredWorld.get<Engine::WorldTransformComponent>(restoredEntity).worldMatrix;
        const auto rejected = registry.ReadComponent<Engine::TransformComponent>(
            invalid, "Transform", restoredWorld, restoredEntity);
        if (rejected.Code != archive::ArchiveErrorCode::InvalidValue ||
            restored.position != previous.position || restored.scale != previous.scale ||
            restored.rotation != previous.rotation || restored.eulerRotation != previous.eulerRotation ||
            restoredWorld.get<Engine::WorldTransformComponent>(restoredEntity).worldMatrix != previousMatrix)
            return Fail("Invalid Transform changed the existing component or world matrix.");

        const auto emptyEntity = restoredWorld.create();
        const auto emptyRejected = registry.ReadComponent<Engine::TransformComponent>(
            invalid, "Transform", restoredWorld, emptyEntity);
        if (emptyRejected.Code != archive::ArchiveErrorCode::InvalidValue ||
            restoredWorld.any_of<Engine::TransformComponent, Engine::WorldTransformComponent,
                Engine::FlagComponent>(emptyEntity))
            return Fail("Invalid Transform created component dependencies.");

        return 0;
    }
}

int main()
{
    if (CheckTransformRotation() != 0)
    {
        return 1;
    }

    using VisitResult = Engine::ComponentVisitResult;
    using AccessError = reflection::PropertyAccessError;

    struct ReflectionLifetime
    {
        ~ReflectionLifetime() { Engine::ReflectionRegistry::Destroy(); }
    } lifetime;
    const auto* engineRegistry = Engine::ReflectionRegistry::Create();
    const auto* registry = engineRegistry != nullptr ? &engineRegistry->GetRegistry() : nullptr;

    if (registry == nullptr || registry->FindType<Engine::NameComponent>() == nullptr ||
        registry->FindType<Engine::TransformComponent>() == nullptr)
    {
        return Fail("Engine core reflection lookup failed.");
    }

    if (CheckUnifiedRegistry(*engineRegistry) != 0 ||
        CheckTransformArchive<archive::JsonArchive>(*engineRegistry) != 0 ||
        CheckTransformArchive<archive::BeveArchive>(*engineRegistry) != 0)
        return 1;

    std::unique_ptr<Engine::Scene, SceneDeleter> scene{ Engine::Scene::Create() };
    std::unique_ptr<Engine::Scene, SceneDeleter> otherScene{ Engine::Scene::Create() };

    if (!scene || !otherScene)
    {
        return Fail("Scene creation failed.");
    }

    auto& entity = scene->CreateEntity();
    auto& foreignEntity = otherScene->CreateEntity();
    scene->AddComponent<Engine::NameComponent>(entity);
    scene->AddComponent<Engine::TransformComponent>(entity);

    const std::wstring expectedName = L"Reflected entity";
    std::size_t visited = 0;
    bool valid = true;

    const auto edited = scene->ForEachReflectedComponent(entity,
        [&](const reflection::TypeInfo& type, const reflection::ObjectView& object)
        {
            ++visited;
            valid = valid && !object.IsReadOnly();

            if (object.Is<Engine::NameComponent>())
            {
                const auto* property = type.FindProperty("name");
                valid = valid && property != nullptr &&
                    property->TryWriteValue(object, reflection::ValueView::From(expectedName)) == AccessError::None;
            }
            else if (object.Is<Engine::TransformComponent>())
            {
                const auto* property = type.FindProperty("eulerRotation");
                const glm::vec3 expectedEuler(0.0f, 450.0f, 0.0f);

                valid = valid && property != nullptr &&
                    property->TryWriteValue(object, reflection::ValueView::From(expectedEuler)) == AccessError::None;

                auto& transform = scene->GetComponent<Engine::TransformComponent>(entity);

                valid = valid && transform.SynchronizeRotation() &&
                    transform.eulerRotation == expectedEuler &&
                    SameOrientation(transform.rotation, glm::quat(glm::radians(expectedEuler)));
            }

            return valid;
        });

    if (edited != VisitResult::Completed || !valid || visited != 3 ||
        !scene->HasComponent<Engine::IDComponent>(entity) ||
        scene->GetComponent<Engine::IDComponent>(entity).id == 0 ||
        scene->GetComponent<Engine::NameComponent>(entity).name != expectedName)
    {
        return Fail("Scene reflected editing failed.");
    }

    const Engine::Scene& constScene = *scene;
    visited = 0;
    valid = true;

    const auto readOnly = constScene.ForEachReflectedComponent(entity,
        [&](const reflection::TypeInfo& type, const reflection::ObjectView& object)
        {
            ++visited;
            valid = valid && object.IsReadOnly();

            if (object.Is<Engine::NameComponent>())
            {
                const auto* property = type.FindProperty("name");
                valid = valid && property != nullptr &&
                    property->TryWriteValue(object, reflection::ValueView::From(expectedName)) ==
                    AccessError::ReadOnlyObject;
            }

            return valid;
        });

    if (readOnly != VisitResult::Completed || !valid || visited != 3)
    {
        return Fail("Scene const reflection access failed.");
    }

    visited = 0;

    const auto foreign = scene->ForEachReflectedComponent(foreignEntity,
        [&](const reflection::TypeInfo&, const reflection::ObjectView&)
        {
            ++visited;
            return true;
        });

    if (foreign != VisitResult::InvalidEntity || visited != 0)
    {
        return Fail("Scene foreign entity guard failed.");
    }

    otherScene->AddComponent<Engine::NameComponent>(foreignEntity);

    archive::JsonArchive writer;
    std::string document;

    const auto writeResult = engineRegistry->WriteComponent<Engine::NameComponent>(
        writer, "Name", constScene.GetRegistry(), entity.GetEntityHandle());

    if (!writeResult || !writer.ToJson(document))
    {
        return Fail("Scene NameComponent writing failed.");
    }

    archive::JsonArchive reader;

    if (!reader.Parse(document))
    {
        return Fail("Scene NameComponent document parsing failed.");
    }

    auto readResult = engineRegistry->ReadComponent<Engine::NameComponent>(
        reader, "Name", otherScene->GetRegistry(), foreignEntity.GetEntityHandle());

    auto& restoredName = otherScene->GetComponent<Engine::NameComponent>(foreignEntity);

    if (!readResult || restoredName.name != expectedName)
    {
        return Fail("Scene NameComponent round-trip failed.");
    }

    const std::wstring beforeName = restoredName.name;
    archive::JsonArchive invalidReader;

    if (!invalidReader.Parse(R"({"Name":{"name":42}})"))
    {
        return Fail("Scene invalid-name document parsing failed.");
    }

    readResult = engineRegistry->ReadComponent<Engine::NameComponent>(
        invalidReader, "Name", otherScene->GetRegistry(), foreignEntity.GetEntityHandle());

    if (readResult.Code != archive::ArchiveErrorCode::InvalidValue ||
        readResult.Field != "name" || restoredName.name != beforeName)
    {
        return Fail("Scene NameComponent invalid-field rollback failed.");
    }

    archive::JsonArchive missingReader;

    if (!missingReader.Parse(R"({"Name":{}})"))
    {
        return Fail("Scene missing-name document parsing failed.");
    }

    readResult = engineRegistry->ReadComponent<Engine::NameComponent>(
        missingReader, "Name", otherScene->GetRegistry(), foreignEntity.GetEntityHandle());

    if (readResult.Code != archive::ArchiveErrorCode::MissingField ||
        readResult.Field != "name" || restoredName.name != beforeName)
    {
        return Fail("Scene NameComponent missing-field rollback failed.");
    }

    auto& emptyEntity = otherScene->CreateEntity();

    readResult = engineRegistry->ReadComponent<Engine::NameComponent>(
        invalidReader, "Name", otherScene->GetRegistry(), emptyEntity.GetEntityHandle());

    if (readResult.Code != archive::ArchiveErrorCode::InvalidValue ||
        otherScene->HasComponent<Engine::NameComponent>(emptyEntity))
    {
        return Fail("Invalid data created a component.");
    }

    readResult = engineRegistry->ReadComponent<Engine::NameComponent>(
        missingReader, "Name", otherScene->GetRegistry(), emptyEntity.GetEntityHandle());

    if (readResult.Code != archive::ArchiveErrorCode::MissingField ||
        otherScene->HasComponent<Engine::NameComponent>(emptyEntity))
    {
        return Fail("Missing data created a component.");
    }

    archive_reflection::PropertyCodecs rejectingCodecs;
    if (!rejectingCodecs.Register<std::wstring>(
        [](archive::IArchive& ar, std::string_view key, const std::wstring& value) { return ar.Write(key, value); },
        [](const archive::IArchive&, std::string_view key, std::wstring& value) -> archive::ArchiveResult
        {
            value = L"Rejected";
            return { archive::ArchiveErrorCode::InvalidValue, std::string(key) };
        }))
        return Fail("Rejecting codec registration failed.");

    readResult = engineRegistry->ReadComponent<Engine::NameComponent>(
        reader, "Name", otherScene->GetRegistry(), emptyEntity.GetEntityHandle(), &rejectingCodecs);

    if (readResult.Code != archive::ArchiveErrorCode::InvalidValue ||
        otherScene->HasComponent<Engine::NameComponent>(emptyEntity))
    {
        return Fail("A failed codec created a component.");
    }

    readResult = engineRegistry->ReadComponent<Engine::NameComponent>(
        reader, "Name", otherScene->GetRegistry(), emptyEntity.GetEntityHandle());

    if (!readResult || !otherScene->HasComponent<Engine::NameComponent>(emptyEntity))
    {
        return Fail("Component creation during restoration failed.");
    }

    if (otherScene->GetComponent<Engine::NameComponent>(emptyEntity).name != expectedName)
    {
        return Fail("Created component contains incorrect data.");
    }

    std::cout << "Scene NameComponent creation checks passed.\n";

    readResult = engineRegistry->ReadComponent<Engine::NameComponent>(
        reader, "Name", otherScene->GetRegistry(), entt::null);

    if (readResult.Code != archive::ArchiveErrorCode::InvalidOperation)
    {
        return Fail("Scene invalid-entity archive guard failed.");
    }

    std::cout << "Scene NameComponent archive checks passed.\n";

    std::cout << "Engine core reflection integration checks passed.\n";
    std::cout << "Scene component reflection checks passed.\n";
    std::cout << "Scene foreign entity guard checks passed.\n";
    std::cout << "Scene reflected rotation editing checks passed.\n";

    scene->AddComponent<Engine::WorldTransformComponent>(entity);

    archive_reflection::PropertyCodecs codecs;

    if (!codecs.Register<glm::vec3>(archive_glm::WriteVec3, archive_glm::ReadVec3) ||
        !codecs.Register<glm::quat>(archive_glm::WriteQuat, archive_glm::ReadQuat))
    {
        return Fail("GLM archive codec registration failed.");
    }

    archive::JsonArchive entityWriter;
    std::string entityDocument;

    if (!engineRegistry->WriteComponents(
        entityWriter, "Components", constScene.GetRegistry(), entity.GetEntityHandle(), &codecs) ||
        !entityWriter.ToJson(entityDocument))
    {
        return Fail("Entity component writing failed.");
    }

    archive::JsonArchive entityReader;

    std::cout << entityDocument << '\n';

    if (!entityReader.Parse(entityDocument) || !entityReader.BeginReadObject("Components"))
    {
        return Fail("Entity component document parsing failed.");
    }

    const auto* nameType = registry->FindType<Engine::NameComponent>();
    const auto* transformType = registry->FindType<Engine::TransformComponent>();
    const auto* worldType = registry->FindType<Engine::WorldTransformComponent>();

    if (nameType == nullptr || transformType == nullptr || worldType == nullptr)
    {
        return Fail("Entity component type lookup failed.");
    }

    bool hasName = false;
    bool hasTransform = false;
    bool hasWorld = false;

    if (!entityReader.HasField(nameType->QualifiedName, hasName) ||
        !entityReader.HasField(transformType->QualifiedName, hasTransform) ||
        !entityReader.HasField(worldType->QualifiedName, hasWorld) ||
        !entityReader.EndObject() || !hasName || !hasTransform || hasWorld)
    {
        return Fail("Entity component serialization policy failed.");
    }

    const archive_reflection::PropertyCodecs emptyCodecs;
    const auto failedWrite = engineRegistry->WriteComponents(
        entityWriter, "Components", constScene.GetRegistry(), entity.GetEntityHandle(), &emptyCodecs);
    std::string afterFailure;

    if (failedWrite.Code != archive::ArchiveErrorCode::UnsupportedType ||
        !entityWriter.ToJson(afterFailure) || afterFailure != entityDocument)
    {
        return Fail("Entity component failed-write rollback failed.");
    }

    const auto invalidWrite = engineRegistry->WriteComponents(
        entityWriter, "Components", constScene.GetRegistry(), entt::null, &codecs);

    if (invalidWrite.Code != archive::ArchiveErrorCode::InvalidOperation ||
        !entityWriter.ToJson(afterFailure) || afterFailure != entityDocument)
    {
        return Fail("Entity component invalid-entity guard failed.");
    }

    std::cout << "Entity component archive checks passed.\n";

    return 0;
}
