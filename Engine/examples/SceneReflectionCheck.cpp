#include "Scene.h"
#include "Components/NameComponent.h"
#include "Components/TransformComponent.h"
#include "Reflection/CoreComponentReflection.h"
#include "ComponentArchive.h"
#include <archive/JsonArchive.h>

#include <iostream>
#include <memory>
#include <string>
#include <cmath>

#include "EntityArchive.h"
#include "Components/WorldTransformComponent.h"
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

int main()
{
    if (CheckTransformRotation() != 0)
    {
        return 1;
    }

    using VisitResult = Engine::ComponentVisitResult;
    using AccessError = reflection::PropertyAccessError;

    const auto* registry = Engine::GetCoreComponentReflectionRegistry();

    if (registry == nullptr || registry->FindType<Engine::NameComponent>() == nullptr ||
        registry->FindType<Engine::TransformComponent>() == nullptr)
    {
        return Fail("Engine core reflection lookup failed.");
    }

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

    if (edited != VisitResult::Completed || !valid || visited != 2 ||
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

    if (readOnly != VisitResult::Completed || !valid || visited != 2)
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

    const auto writeResult = Engine::WriteComponent<Engine::NameComponent>(
        writer, "Name", constScene.GetRegistry(), entity.GetEntityHandle(), *registry);

    if (!writeResult || !writer.ToJson(document))
    {
        return Fail("Scene NameComponent writing failed.");
    }

    archive::JsonArchive reader;

    if (!reader.Parse(document))
    {
        return Fail("Scene NameComponent document parsing failed.");
    }

    auto readResult = Engine::ReadComponent<Engine::NameComponent>(
        reader, "Name", otherScene->GetRegistry(), foreignEntity.GetEntityHandle(), *registry);

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

    readResult = Engine::ReadComponent<Engine::NameComponent>(
        invalidReader, "Name", otherScene->GetRegistry(), foreignEntity.GetEntityHandle(), *registry);

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

    readResult = Engine::ReadComponent<Engine::NameComponent>(
        missingReader, "Name", otherScene->GetRegistry(), foreignEntity.GetEntityHandle(), *registry);

    if (readResult.Code != archive::ArchiveErrorCode::MissingField ||
        readResult.Field != "name" || restoredName.name != beforeName)
    {
        return Fail("Scene NameComponent missing-field rollback failed.");
    }

    auto& emptyEntity = otherScene->CreateEntity();

    readResult = Engine::ReadComponent<Engine::NameComponent>(
        invalidReader, "Name", otherScene->GetRegistry(), emptyEntity.GetEntityHandle(), *registry);

    if (readResult.Code != archive::ArchiveErrorCode::InvalidValue ||
        otherScene->HasComponent<Engine::NameComponent>(emptyEntity))
    {
        return Fail("Invalid data created a component.");
    }

    readResult = Engine::ReadComponent<Engine::NameComponent>(
        missingReader, "Name", otherScene->GetRegistry(), emptyEntity.GetEntityHandle(), *registry);

    if (readResult.Code != archive::ArchiveErrorCode::MissingField ||
        otherScene->HasComponent<Engine::NameComponent>(emptyEntity))
    {
        return Fail("Missing data created a component.");
    }

    const auto rejectPreparedName = +[](Engine::NameComponent& candidate) -> archive::ArchiveResult
        {
            candidate.name = L"Rejected";
            return { archive::ArchiveErrorCode::InvalidValue, "name" };
        };

    readResult = Engine::ReadComponent<Engine::NameComponent>(
        reader, "Name", otherScene->GetRegistry(), emptyEntity.GetEntityHandle(), *registry,
        nullptr, rejectPreparedName);

    if (readResult.Code != archive::ArchiveErrorCode::InvalidValue ||
        otherScene->HasComponent<Engine::NameComponent>(emptyEntity))
    {
        return Fail("Failed preparation created a component.");
    }

    readResult = Engine::ReadComponent<Engine::NameComponent>(
        reader, "Name", otherScene->GetRegistry(), emptyEntity.GetEntityHandle(), *registry);

    if (!readResult || !otherScene->HasComponent<Engine::NameComponent>(emptyEntity))
    {
        return Fail("Component creation during restoration failed.");
    }

    if (otherScene->GetComponent<Engine::NameComponent>(emptyEntity).name != expectedName)
    {
        return Fail("Created component contains incorrect data.");
    }

    std::cout << "Scene NameComponent creation checks passed.\n";

    readResult = Engine::ReadComponent<Engine::NameComponent>(
        reader, "Name", otherScene->GetRegistry(), entt::null, *registry);

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

    if (!Engine::WriteEntityComponents(entityWriter, "Components", constScene, entity, &codecs) ||
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

    const auto failedWrite = Engine::WriteEntityComponents(entityWriter, "Components", constScene, entity);
    std::string afterFailure;

    if (failedWrite.Code != archive::ArchiveErrorCode::UnsupportedType ||
        !entityWriter.ToJson(afterFailure) || afterFailure != entityDocument)
    {
        return Fail("Entity component failed-write rollback failed.");
    }

    const auto foreignWrite = Engine::WriteEntityComponents(
        entityWriter, "Components", constScene, foreignEntity, &codecs);

    if (foreignWrite.Code != archive::ArchiveErrorCode::InvalidOperation ||
        !entityWriter.ToJson(afterFailure) || afterFailure != entityDocument)
    {
        return Fail("Entity component foreign-scene guard failed.");
    }

    std::cout << "Entity component archive checks passed.\n";

    return 0;
}