#include "Scene.h"
#include "SceneManager.h"
#include "CoreComponents.h"
#include "PhysicsComponent.h"
#include "RenderComponents.h"
#include "UIComponents.h"
#include "Resources.h"
#include "ResourceManager.h"
#include "TransformSystem.h"
#include <archive/JsonArchive.h>
#include <archive/BeveArchive.h>
#include "Adapters/ResourceHandleArchiveAdapter.h"
#include "ResourceReference.h"
#include <map>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include "Collision.h"
#include <cmath>
#include "SplineComponent.h"
#include "SplineFollowerComponent.h"
#include "SplineMeshComponent.h"
#include "Vertex.h"
#include "PlatformPath.h"

namespace
{
    constexpr uint64 RootId = 9007199254740993ULL;
    constexpr uint64 FirstChildId = 7;
    constexpr uint64 SecondChildId = 42;
    const std::string TransformJson = R"({"Type":"Engine::TransformComponent","Data":{)"
        R"("position":[0,0,0],"rotation":[0,0,0,1],"eulerRotation":[0,0,0],"scale":[1,1,1]}})";

    struct SceneDeleter
    {
        void operator()(Engine::Scene* scene) const { if (scene != nullptr) scene->Release(); }
    };
    using ScenePtr = std::unique_ptr<Engine::Scene, SceneDeleter>;

    struct RegistryScope
    {
        ~RegistryScope()
        {
            Engine::SceneManager::Destroy();
            Engine::ResourceManager::Destroy();
            Engine::ReflectionRegistry::Destroy();
        }
    };

    bool Fail(const char* message)
    {
        std::cerr << message << '\n';
        return false;
    }

    bool SameMatrix(const glm::mat4& left, const glm::mat4& right)
    {
        for (int i = 0; i < 4; ++i)
            if (glm::length(left[i] - right[i]) > 1.0e-4f) return false;
        return true;
    }

    bool TryGetFloatParameter(const Engine::MaterialInterface* material, const char* name, float& outValue)
    {
        if (material == nullptr)
            return false;

        const auto& parameters = material->GetParameters();
        const auto iter = parameters.find(name);
        if (iter == parameters.end() || iter->second.type != Engine::EMaterialParameterType::Float ||
            iter->second.data.size() < sizeof(float))
            return false;

        std::memcpy(&outValue, iter->second.data.data(), sizeof(float));
        return true;
    }

    entt::entity AddSpatialEntity(Engine::Scene& scene, uint64 id, const wchar_t* name, glm::vec3 position)
    {
        auto& entity = scene.CreateEntity();
        auto& world = scene.GetRegistry();
        const auto handle = entity.GetEntityHandle();
        world.get<Engine::IDComponent>(handle).id = id;
        world.emplace<Engine::NameComponent>(handle, name);
        world.emplace<Engine::TransformComponent>(handle).position = position;
        world.emplace<Engine::WorldTransformComponent>(handle);
        world.emplace<Engine::FlagComponent>(handle);
        world.emplace<Engine::HierarchyComponent>(handle);
        return handle;
    }

    ScenePtr MakeScene()
    {
        ScenePtr scene(Engine::Scene::Create());
        scene->SetName(L"저장 검사 씬");
        scene->SetActive(false);
        scene->SetPaused(true);
        const auto root = AddSpatialEntity(*scene, RootId, L"부모", { 5.0f, 2.0f, -3.0f });
        const auto first = AddSpatialEntity(*scene, FirstChildId, L"첫째", { 1.0f, 2.0f, 3.0f });
        const auto second = AddSpatialEntity(*scene, SecondChildId, L"둘째", { -2.0f, 0.0f, 1.0f });
        auto& world = scene->GetRegistry();
        world.get<Engine::HierarchyComponent>(root).children = { second, first };
        world.get<Engine::HierarchyComponent>(first).parent = root;
        world.get<Engine::HierarchyComponent>(second).parent = root;
        auto& transform = world.get<Engine::TransformComponent>(first);
        static_cast<void>(transform.SetEulerRotation({ 25.0f, 450.0f, -35.0f }));
        transform.scale = { -1.0f, 0.0f, 2.0f };
        struct EditorOnlyState { int Sentinel = 123; };
        world.ctx().emplace<EditorOnlyState>();
        Engine::TransformSystem::UpdateHierarchy(world);
        return scene;
    }

    bool CheckRestored(Engine::Scene& restored, const Engine::Scene& original)
    {
        auto* root = restored.FindEntityById(RootId);
        auto* first = restored.FindEntityById(FirstChildId);
        auto* second = restored.FindEntityById(SecondChildId);
        const auto* oldFirst = original.FindEntityById(FirstChildId);
        if (root == nullptr || first == nullptr || second == nullptr || oldFirst == nullptr ||
            restored.GetName() != original.GetName() || restored.IsActive() || restored.IsPaused() ||
            restored.FindEntityById(0) != nullptr || first->GetScene() != &restored)
            return Fail("Scene identity, ownership, or metadata restoration failed.");
        auto& world = restored.GetRegistry();
        const auto rootHandle = root->GetEntityHandle();
        const auto firstHandle = first->GetEntityHandle();
        const auto secondHandle = second->GetEntityHandle();
        if (rootHandle == original.FindEntityById(RootId)->GetEntityHandle())
            return Fail("Test did not exercise changed runtime handles.");
        if (world.get<Engine::NameComponent>(firstHandle).name != L"첫째" ||
            world.get<Engine::HierarchyComponent>(firstHandle).parent != rootHandle ||
            world.get<Engine::HierarchyComponent>(rootHandle).children !=
                std::vector<entt::entity>{ secondHandle, firstHandle })
            return Fail("Entity names, parent references, or sibling order changed.");
        const auto& transform = world.get<Engine::TransformComponent>(firstHandle);
        if (transform.eulerRotation != glm::vec3(25.0f, 450.0f, -35.0f) ||
            transform.scale != glm::vec3(-1.0f, 0.0f, 2.0f) ||
            !SameMatrix(world.get<Engine::WorldTransformComponent>(firstHandle).worldMatrix,
                original.GetRegistry().get<Engine::WorldTransformComponent>(oldFirst->GetEntityHandle()).worldMatrix))
            return Fail("Local transform or derived world matrix restoration failed.");
        const auto newHandle = restored.CreateEntity().GetEntityHandle();
        const auto newId = world.get<Engine::IDComponent>(newHandle).id;
        if (newId == 0 || newId == RootId || newId == FirstChildId || newId == SecondChildId ||
            restored.FindEntityById(newId)->GetEntityHandle() != newHandle)
            return Fail("Post-load persistent ID allocation collided.");
        return true;
    }

    template<typename Archive>
    bool CheckFileRoundTrip(const wchar_t* path)
    {
        auto original = MakeScene();
        auto& sourceWorld = original->GetRegistry();
        const auto sourceRoot = original->FindEntityById(RootId)->GetEntityHandle();
        const auto sourceChild = original->FindEntityById(FirstChildId)->GetEntityHandle();

        auto& spline = sourceWorld.emplace<Engine::SplineComponent>(sourceRoot);
        spline.Closed = true;
        spline.Points[1].Position = glm::vec3(3.0f, 1.0f, 4.0f);
        spline.Points[1].RollDegrees = 450.0f;

        sourceWorld.emplace<Engine::StaticMeshRendererComponent>(sourceRoot);
        auto& mesh = sourceWorld.emplace<Engine::SplineMeshComponent>(sourceRoot);
        mesh.DeformToPath = true;
        mesh.UVRepeatDistance = 2.5;
        mesh.GeneratedCount = 7;
        mesh.HasBuildError = true;

        auto& follower = sourceWorld.emplace<Engine::SplineFollowerComponent>(sourceChild);
        follower.Speed = -3.0f;
        follower.StartDistance = 1.25;
        follower.Loop = true;
        follower.AlignToMovement = true;
        follower.CurrentDistance = 4.0;
        Archive writer;
        if (!original->WriteArchive(writer) || !writer.SaveToFile(path))
            return Fail("Scene file write failed.");
        Archive reader;
        if (!reader.LoadFromFile(path)) return Fail("Scene file read failed.");
        Engine::Scene* output = nullptr;
        const auto result = Engine::Scene::CreateFromArchive(reader, output);
        ScenePtr restored(output);
        if (!result || restored == nullptr) return Fail("Scene candidate creation failed.");
        if (!CheckRestored(*restored, *original)) return false;
        auto& restoredWorld = restored->GetRegistry();
        const auto restoredRoot = restored->FindEntityById(RootId)->GetEntityHandle();
        const auto restoredChild = restored->FindEntityById(FirstChildId)->GetEntityHandle();

        const auto* restoredSpline = restoredWorld.try_get<Engine::SplineComponent>(restoredRoot);
        if (!restoredSpline || !restoredSpline->Closed || restoredSpline->Points != spline.Points)
            return Fail("Spline points, roll, or closed state changed after loading.");

        const auto* restoredMesh = restoredWorld.try_get<Engine::SplineMeshComponent>(restoredRoot);
        if (!restoredMesh || !restoredMesh->DeformToPath || restoredMesh->UVRepeatDistance != 2.5 ||
            restoredMesh->GeneratedCount != 0 || restoredMesh->HasBuildError)
            return Fail("Spline mesh settings or transient state changed after loading.");

        const auto* restoredFollower = restoredWorld.try_get<Engine::SplineFollowerComponent>(restoredChild);
        if (!restoredFollower || !restoredFollower->AlignToMovement || !restoredFollower->Loop ||
            restoredFollower->Speed != -3.0f || restoredFollower->StartDistance != 1.25 ||
            restoredFollower->CurrentDistance != 0.0)
            return Fail("Spline follower settings or transient state changed after loading.");
        Engine::Scene* occupied = original.get();
        if (Engine::Scene::CreateFromArchive(reader, occupied).Code != archive::ArchiveErrorCode::InvalidOperation ||
            occupied != original.get())
            return Fail("Occupied output pointer was overwritten.");
        return true;
    }

    std::string EntityJson(uint64 id, std::string components = "", std::string hierarchy = "")
    {
        return "{\"Id\":" + std::to_string(id) + ",\"Components\":[" + components + "]" + hierarchy + "}";
    }

    std::string SceneJson(std::string entities, uint32 version = 1)
    {
        return "{\"Scene\":{\"Version\":" + std::to_string(version) +
            ",\"Name\":\"Loaded\",\"Active\":true,\"Entities\":[" + entities + "]}}";
    }

    bool CheckRejectedDocuments()
    {
        const std::string name = R"({"Type":"Engine::NameComponent","Data":{"name":"Entity"}})";
        const auto& transform = TransformJson;
        const std::string child = R"(,"Hierarchy":{"Parent":2,"Children":[]})";
        const std::string parent = R"(,"Hierarchy":{"Parent":0,"Children":[1]})";
        const std::vector<std::pair<std::string, archive::ArchiveErrorCode>> cases =
        {
            { SceneJson("", 2), archive::ArchiveErrorCode::InvalidValue },
            { SceneJson(R"({"Components":[]})"), archive::ArchiveErrorCode::MissingField },
            { SceneJson(EntityJson(0)), archive::ArchiveErrorCode::InvalidValue },
            { SceneJson(EntityJson(1) + "," + EntityJson(1)), archive::ArchiveErrorCode::InvalidValue },
            { SceneJson(EntityJson(1, R"({"Type":"Unknown","Data":{}})")),
                archive::ArchiveErrorCode::UnsupportedType },
            { SceneJson(EntityJson(1, R"({"Type":"Engine::NameComponent","Data":{}})")),
                archive::ArchiveErrorCode::MissingField },
            { SceneJson(EntityJson(1, R"({"Type":"Engine::NameComponent","Data":{"name":3}})")),
                archive::ArchiveErrorCode::InvalidValue },
            { SceneJson(EntityJson(1, R"({"Type":"Engine::TransformComponent","Data":{)"
                R"("position":[0,0,0],"rotation":[0,0,0,0],"eulerRotation":[0,0,0],"scale":[1,1,1]}})")),
                archive::ArchiveErrorCode::InvalidValue },
            { SceneJson(EntityJson(1, name + "," + name)), archive::ArchiveErrorCode::InvalidValue },
            { SceneJson(EntityJson(1, R"({"Type":"Engine::IDComponent","Data":{"id":2}})")),
                archive::ArchiveErrorCode::InvalidValue },
            { SceneJson(EntityJson(1, R"({"Type":"Engine::WorldTransformComponent","Data":{}})")),
                archive::ArchiveErrorCode::InvalidValue },
            { SceneJson(EntityJson(1, R"({"Type":"Engine::HierarchyComponent","Data":{}})")),
                archive::ArchiveErrorCode::InvalidValue },
            { SceneJson(EntityJson(1, transform, child)), archive::ArchiveErrorCode::InvalidValue },
            { SceneJson(EntityJson(1, transform, child) + "," + EntityJson(2, transform)),
                archive::ArchiveErrorCode::InvalidValue },
            { SceneJson(EntityJson(1, transform, child) + "," + EntityJson(2, transform,
                R"(,"Hierarchy":{"Parent":0,"Children":[1,1]})")), archive::ArchiveErrorCode::InvalidValue },
            { SceneJson(EntityJson(1, transform, R"(,"Hierarchy":{"Parent":1,"Children":[1]})")),
                archive::ArchiveErrorCode::InvalidValue },
            { SceneJson(EntityJson(1, transform, R"(,"Hierarchy":{"Parent":2,"Children":[2]})") + "," +
                EntityJson(2, transform, R"(,"Hierarchy":{"Parent":1,"Children":[1]})")),
                archive::ArchiveErrorCode::InvalidValue },
            { SceneJson(EntityJson(1, name, child) + "," + EntityJson(2, transform, parent)),
                archive::ArchiveErrorCode::InvalidValue },
        };
        auto& manager = Engine::SceneManager::Get();
        Engine::Scene* current = manager.GetCurrentScene();
        const auto savedName = current->GetName();
        for (std::size_t i = 0; i < cases.size(); ++i)
        {
            archive::JsonArchive reader;
            if (!reader.Decode(cases[i].first)) return Fail("Malformed-input test JSON is invalid.");
            Engine::Scene* candidate = nullptr;
            const auto result = Engine::Scene::CreateFromArchive(reader, candidate);
            ScenePtr cleanup(candidate);
            if (result.Code != cases[i].second || candidate != nullptr || reader.GetObjectDepth() != 0)
            {
                std::cerr << "Unexpected rejection for case " << i << ": " << static_cast<int>(result.Code)
                    << ", " << result.Field << '\n';
                return false;
            }
            if (!reader.SaveToFile(L"invalid-scene.json") ||
                manager.LoadScene(reader, L"invalid-scene.json") == Engine::EResult::Success ||
                manager.GetCurrentScene() != current || current->GetName() != savedName)
                return Fail("Failed scene load changed the active scene.");
        }
        std::ofstream("truncated-scene.json", std::ios::binary) << "{\"Scene\":";
        archive::JsonArchive reader;
        if (manager.LoadScene(reader, L"truncated-scene.json") == Engine::EResult::Success ||
            manager.LoadScene(reader, L"missing-scene.json") == Engine::EResult::Success ||
            manager.GetCurrentScene() != current)
            return Fail("File parse/open failure replaced the current scene.");
        return true;
    }

    bool CheckManagerAndWriteFailure()
    {
        auto original = MakeScene();
        auto& manager = Engine::SceneManager::Get();
        if (manager.OpenScene(original.get()) != Engine::EResult::Success)
            return Fail("Initial scene activation failed.");
        archive::JsonArchive writer;
        if (manager.SaveScene(writer, L"managed-scene.json") != Engine::EResult::Success)
            return Fail("SceneManager save failed.");
        std::string before;
        if (!writer.Encode(before) || before.find("WorldTransformComponent") != std::string::npos)
            return Fail("Derived components were serialized.");
        auto& world = original->GetRegistry();
        const auto root = original->FindEntityById(RootId)->GetEntityHandle();
        struct UnregisteredStorage { int Value = 0; };
        world.emplace<UnregisteredStorage>(root);
        if (original->WriteArchive(writer).Code != archive::ArchiveErrorCode::UnsupportedType ||
            manager.SaveScene(writer, L"managed-scene.json") == Engine::EResult::Success)
            return Fail("Unregistered component storage was silently skipped.");
        archive::JsonArchive disk;
        std::string after, onDisk;
        if (!writer.Encode(after) || after != before || !disk.LoadFromFile(L"managed-scene.json") ||
            !disk.Encode(onDisk) || onDisk != before)
            return Fail("Rejected serialization changed the document or existing file.");
        world.remove<UnregisteredStorage>(root);
        const auto savedChildren = world.get<Engine::HierarchyComponent>(root).children;
        world.get<Engine::HierarchyComponent>(root).children = { entt::null };
        if (original->WriteArchive(writer).Code != archive::ArchiveErrorCode::InvalidValue ||
            !writer.Encode(after) || after != before)
            return Fail("Invalid hierarchy was written or changed the existing document.");
        world.get<Engine::HierarchyComponent>(root).children = savedChildren;
        archive::JsonArchive reader;
        if (manager.LoadScene(reader, L"managed-scene.json") != Engine::EResult::Success ||
            manager.GetCurrentScene() == original.get() || !CheckRestored(*manager.GetCurrentScene(), *original))
            return Fail("SceneManager did not replace the scene after successful restoration.");
        if (!CheckRejectedDocuments()) return false;
        // Check a payload failure inside the outer Scene object, after earlier fields have been written.
        const auto first = original->FindEntityById(FirstChildId)->GetEntityHandle();
        world.get<Engine::TransformComponent>(first).position.x = std::numeric_limits<float>::infinity();
        if (original->WriteArchive(writer).Code != archive::ArchiveErrorCode::InvalidValue ||
            !writer.Encode(after) || before != after)
            return Fail("Partial scene serialization was committed after an invalid transform.");
        return true;
    }

    bool CheckEmptyScene()
    {
        ScenePtr scene(Engine::Scene::Create());
        archive::BeveArchive archive;
        Engine::Scene* output = nullptr;
        if (!scene->WriteArchive(archive) || !Engine::Scene::CreateFromArchive(archive, output))
            return Fail("Empty scene round-trip failed.");
        ScenePtr restored(output);
        return restored != nullptr && restored->FindEntityById(1) == nullptr;
    }

    bool CheckHierarchyDepthAndIds()
    {
        for (uint64 depth : { 256ULL, 257ULL })
        {
            std::string entities;
            for (uint64 id = 1; id <= depth; ++id)
            {
                const auto parent = id == depth ? 0 : id + 1;
                const auto children = id == 1 ? "" : std::to_string(id - 1);
                const auto hierarchy = ",\"Hierarchy\":{\"Parent\":" + std::to_string(parent) +
                    ",\"Children\":[" + children + "]}";
                if (!entities.empty()) entities += ",";
                entities += EntityJson(id, TransformJson, hierarchy);
            }
            archive::JsonArchive reader;
            if (!reader.Decode(SceneJson(entities))) return Fail("Hierarchy depth test JSON is invalid.");
            Engine::Scene* output = nullptr;
            const auto result = Engine::Scene::CreateFromArchive(reader, output);
            ScenePtr restored(output);
            if ((depth == 256 && (!result || output == nullptr)) ||
                (depth == 257 && (result.Code != archive::ArchiveErrorCode::InvalidValue || output != nullptr)))
                return Fail("Hierarchy depth boundary validation failed.");
        }
        archive::JsonArchive reader;
        if (!reader.Decode(SceneJson(EntityJson(1) + "," + EntityJson(2) + "," + EntityJson(4)))) return false;
        Engine::Scene* output = nullptr;
        if (!Engine::Scene::CreateFromArchive(reader, output)) return false;
        ScenePtr restored(output);
        const auto handle = restored->CreateEntity().GetEntityHandle();
        if (restored->GetRegistry().get<Engine::IDComponent>(handle).id != 5)
            return Fail("Persistent ID allocation did not skip an occupied value.");
        archive::JsonArchive writer;
        if (!restored->WriteArchive(writer)) return false;
        std::string before, after;
        if (!writer.Encode(before)) return false;
        restored->GetRegistry().get<Engine::IDComponent>(handle).id = 1;
        if (restored->WriteArchive(writer).Code != archive::ArchiveErrorCode::InvalidValue ||
            !writer.Encode(after) || before != after)
            return Fail("Duplicate persistent IDs were saved.");
        restored->GetRegistry().get<Engine::IDComponent>(handle).id = 0;
        if (restored->WriteArchive(writer).Code != archive::ArchiveErrorCode::InvalidValue)
            return Fail("Zero persistent ID was saved.");
        ScenePtr collisionScene(Engine::Scene::Create());
        if (!collisionScene) return Fail("Failed to create ID collision test scene.");

        for (uint64 i = 0; i < 4; ++i)
        {
            auto& created = collisionScene->CreateEntity();
            collisionScene->GetRegistry().get<Engine::IDComponent>(created.GetEntityHandle()).id = i + 5;
        }

        const auto nextHandle = collisionScene->CreateEntity().GetEntityHandle();
        if (collisionScene->GetRegistry().get<Engine::IDComponent>(nextHandle).id != 9)
            return Fail("Persistent ID allocation did not skip consecutive occupied values.");

        struct AllocationCase { std::string Entities; uint64 NextId; };
        const AllocationCase allocationCases[] = {
            { EntityJson(1) + "," + EntityJson(2), 3 },
            { EntityJson(2) + "," + EntityJson(1), 4 },
            { EntityJson(5) + "," + EntityJson(6) + "," + EntityJson(7) + "," + EntityJson(8), 9 },
            { EntityJson((std::numeric_limits<uint64>::max)()), 2 }
        };

        for (const auto& allocation : allocationCases)
        {
            archive::JsonArchive allocationReader;
            if (!allocationReader.Decode(SceneJson(allocation.Entities)))
                return Fail("Invalid ID allocation test JSON.");

            Engine::Scene* loaded = nullptr;
            if (!Engine::Scene::CreateFromArchive(allocationReader, loaded))
                return Fail("ID allocation test scene load failed.");

            ScenePtr allocationScene(loaded);
            const auto createdHandle = allocationScene->CreateEntity().GetEntityHandle();
            if (allocationScene->GetRegistry().get<Engine::IDComponent>(createdHandle).id != allocation.NextId)
                return Fail("Scene loading changed the next persistent ID.");
        }
        return true;
    }

    bool CheckStoredResourceReferences(const archive::IArchive& ar, std::string_view root, const Engine::ResourceReferenceSet& expected)
    {
        Engine::ResourceReferenceSet actual;
        bool unused = false;

        const auto result = ar.ReadObject(root, unused,
            [&](archive::IArchive& object, bool&) -> archive::ArchiveResult
            {
                return object.ReadObjectArray("ResourceReferences",
                    [&](archive::IArchive& entry, std::size_t) -> archive::ArchiveResult
                    {
                        Engine::ResourceReference reference;
                        auto read = entry.Read("AssetKey", reference.AssetKey);
                        if (read) read = entry.Read("AssetPath", reference.AssetPath);
                        if (!read) return read;

                        if (!actual.insert(std::move(reference)).second)
                            return { archive::ArchiveErrorCode::InvalidValue, "ResourceReferences" };

                        return {};
                    });
            });

        if (!result || actual != expected)
            return Fail("Stored resource reference list did not match.");

        return true;
    }

    bool CheckResourceReferenceReading()
    {
        archive::JsonArchive reader;
        Engine::ResourceReferenceSet references;

        const char* valid =
            R"({"ResourceReferences":[{"AssetKey":"CubeMesh"},)"
            R"({"AssetPath":"Models/Test.bammesh.json"}]})";

        const Engine::ResourceReferenceSet expected = {
            { L"CubeMesh", L"" },
            { L"", L"Models/Test.bammesh.json" }
        };

        if (!reader.Decode(valid) ||
            !archive_resource::ReadResourceReferences(reader, "ResourceReferences", references) ||
            references != expected)
        {
            return Fail("Valid resource reference reading failed.");
        }

        for (const char* json : { "{}", R"({"ResourceReferences":[]})" })
        {
            references = expected;

            if (!reader.Decode(json) ||
                !archive_resource::ReadResourceReferences(reader, "ResourceReferences", references) ||
                !references.empty())
            {
                return Fail("Missing or empty resource references did not use an empty default.");
            }
        }

        const std::vector<std::string> invalidDocuments = {
            R"({"ResourceReferences":{}})",
            R"({"ResourceReferences":[{"AssetKey":7}]})",
            R"({"ResourceReferences":[{}]})",
            R"({"ResourceReferences":[{"AssetKey":"CubeMesh"},)"
            R"({"AssetKey":"CubeMesh","AssetPath":""}]})"
        };

        const Engine::ResourceReferenceSet preserved = { { L"Keep", L"keep.json" } };

        for (const auto& json : invalidDocuments)
        {
            if (!reader.Decode(json))
                return Fail("Invalid-reference fixture was not valid JSON.");

            references = preserved;
            const auto result = archive_resource::ReadResourceReferences(reader, "ResourceReferences", references);

            if (result || result.Code != archive::ArchiveErrorCode::InvalidValue || references != preserved)
                return Fail("Invalid resource references were accepted or changed the previous output.");
        }

        return true;
    }

    bool CheckRejectedResourceReferenceDocuments()
    {
        for (const std::string root : { "Scene", "Prefab" })
        {
            const std::string json = "{\"" + root + "\":" +
                R"({"Version":1,"Name":"References","Active":true,"RootId":1,)"
                R"("Entities":[{"Id":1,"Components":[]}],)"
                R"("ResourceReferences":[{"AssetKey":"","AssetPath":""}]}})";

            archive::JsonArchive reader;
            if (!reader.Decode(json))
                return Fail("Invalid-reference document fixture could not be decoded.");

            archive::ArchiveResult result;

            if (root == "Scene")
            {
                Engine::Scene* loaded = nullptr;
                result = Engine::Scene::CreateFromArchive(reader, loaded);
                ScenePtr owned(loaded);

                if (loaded)
                    return Fail("Invalid reference metadata unexpectedly created a scene.");
            }
            else
            {
                ScenePtr target(Engine::Scene::Create());
                if (!target)
                    return Fail("Could not create the prefab validation target.");

                Engine::Entity* loaded = nullptr;
                result = target->InstantiatePrefabArchive(reader, loaded);

                if (loaded)
                    return Fail("Invalid reference metadata unexpectedly instantiated a prefab.");
            }

            if (result || result.Code != archive::ArchiveErrorCode::InvalidValue ||
                !result.Field.starts_with("ResourceReferences"))
            {
                return Fail("Document loading did not reject invalid resource reference metadata.");
            }
        }

        return true;
    }

    bool CheckResourcesAndHandlesRoundTrip()
    {
        auto& rm = Engine::ResourceManager::Get();

        Engine::VertexPosition positions[3] = {
            { { 0.0f, 0.0f, 0.0f } }, { { 1.0f, 0.0f, 0.0f } }, { { 0.0f, 1.0f, 0.0f } }
        };
        uint32 indices[3] = { 0, 1, 2 };
        MeshCreateDesc meshDesc{};
        meshDesc.Key = L"test-mesh.bammesh.json";
        meshDesc.Path = L"test-mesh.bammesh.json";
        meshDesc.Streams[(uint32)Engine::EMeshStream::Position] = { positions, 3, sizeof(Engine::VertexPosition) };
        meshDesc.IndexData = indices;
        meshDesc.IndexCount = 3;
        meshDesc.IndexStride = sizeof(uint32);
        meshDesc.BoundingBoxMin = { -1.0f, -2.0f, -3.0f };
        meshDesc.BoundingBoxMax = { 4.0f, 5.0f, 6.0f };
        auto meshHandle = rm.AddResource<Engine::Mesh>(meshDesc.Key, Engine::Mesh::Create(&meshDesc));
        if (!meshHandle.IsValid() || Engine::IsFailure(rm.SaveToJsonFile(meshHandle.Get(), L"test-mesh.bammesh.json")))
            return Fail("Mesh creation or JSON archive save failed.");

        ShaderDesc vsDesc{};
        vsDesc.Key = L"test-vs.bamshader.json";
        vsDesc.Path = L"test-vs.bamshader.json";
        vsDesc.shaderType = EShaderType::Vertex;
        vsDesc.entryPoint = "VSMain";
        vsDesc.spirvPath = L"Shaders/TestVS.spv";
        vsDesc.numUniformBuffers = 2;
        auto vsHandle = rm.AddResource<Engine::Shader>(vsDesc.Key, Engine::Shader::Create(&vsDesc));
        if (!vsHandle.IsValid() || Engine::IsFailure(rm.SaveToJsonFile(vsHandle.Get(), L"test-vs.bamshader.json")))
            return Fail("Shader creation or JSON archive save failed.");

        Engine::MaterialDesc matDesc{};
        matDesc.Key = L"test-mat.bammat.json";
        matDesc.Path = L"test-mat.bammat.json";
        matDesc.VertexShaderHandle = vsHandle;
        matDesc.CullMode = Engine::ECullMode::None;
        auto matHandle = rm.AddResource<Engine::Material>(matDesc.Key, Engine::Material::Create(&matDesc));
        if (!matHandle.IsValid()) return Fail("Material creation failed.");
        matHandle->SetFloat("Roughness", 0.75f);
        if (rm.SaveToJsonFile(matHandle.Get(), L"test-mat.bammat.json") != Engine::EResult::Success)
            return Fail("Material JSON archive save failed.");

        MaterialInstanceDesc miDesc{};
        miDesc.Key = L"test-mi.bammatinst.json";
        miDesc.Path = L"test-mi.bammatinst.json";
        miDesc.BaseMaterialHandle = matHandle;
        auto miHandle = rm.AddResource<Engine::MaterialInstance>(miDesc.Key, Engine::MaterialInstance::Create(&miDesc));
        if (!miHandle.IsValid()) return Fail("MaterialInstance creation failed.");
        miHandle->SetFloat("Roughness", 0.25f);
        if (Engine::IsFailure(rm.SaveToJsonFile(miHandle.Get(), L"test-mi.bammatinst.json")))
            return Fail("MaterialInstance JSON archive save failed.");

        SkeletonCreateDesc skelDesc{};
        skelDesc.Key = L"test-skel.bamskel.json";
        skelDesc.Path = L"test-skel.bamskel.json";
        Bone rootBone{};
        rootBone.Name = L"RootBone";
        rootBone.ParentIndex = -1;
        skelDesc.Bones.push_back(rootBone);
        auto skelHandle = rm.AddResource<Engine::Skeleton>(skelDesc.Key, Engine::Skeleton::Create(&skelDesc));
        if (!skelHandle.IsValid() || Engine::IsFailure(rm.SaveToJsonFile(skelHandle.Get(), L"test-skel.bamskel.json")))
            return Fail("Skeleton creation or JSON archive save failed.");

        AnimationCreateDesc animDesc{};
        animDesc.Key = L"test-anim.bamanim.json";
        animDesc.Path = L"test-anim.bamanim.json";
        animDesc.Duration = 2.5f;
        animDesc.TicksPerSecond = 30.0f;
        AnimationTrack track{};
        track.TargetBoneName = L"RootBone";
        track.PositionKeyframes.push_back({ 0.5, { 1.0f, 2.0f, 3.0f } });
        animDesc.Tracks.push_back(track);
        auto animHandle = rm.AddResource<Engine::Animation>(animDesc.Key, Engine::Animation::Create(&animDesc));
        if (!animHandle.IsValid() || Engine::IsFailure(rm.SaveToJsonFile(animHandle.Get(), L"test-anim.bamanim.json")))
            return Fail("Animation creation or JSON archive save failed.");

        ModelCreateDesc modelDesc{};
        modelDesc.Key = L"test-model.bammodel.json";
        modelDesc.Path = L"test-model.bammodel.json";
        modelDesc.Meshes = { meshHandle };
        modelDesc.Materials = { matHandle };
        modelDesc.Skeleton = skelHandle;
        modelDesc.Animations = { animHandle };
        Engine::ModelNode node;
        node.Name = L"Body";
        node.SourceNodeKey = L"4:Body";
        node.MeshIndices = { 0 };
        node.MeshAttachmentKeys = { L"BodyMesh" };
        modelDesc.Nodes.push_back(std::move(node));
        modelDesc.MeshMaterialIndices = { 0 };
        auto modelHandle = rm.AddResource<Engine::Model>(modelDesc.Key, Engine::Model::Create(&modelDesc));
        if (!modelHandle.IsValid() || Engine::IsFailure(rm.SaveToJsonFile(modelHandle.Get(), L"test-model.bammodel.json")))
            return Fail("Model creation or JSON archive save failed.");

        ScenePtr scene(Engine::Scene::Create());
        const auto entity = AddSpatialEntity(*scene, 100, L"MeshActor", { 3.0f, 4.0f, 5.0f });
        auto& world = scene->GetRegistry();
        auto& staticRenderer = world.emplace<Engine::StaticMeshRendererComponent>(entity);
        staticRenderer.meshHandle = meshHandle;
        staticRenderer.materialHandle = Engine::ResourceHandle<Engine::MaterialInterface>(miHandle);

        auto& skinnedRenderer = world.emplace<Engine::SkinnedMeshRendererComponent>(entity);
        skinnedRenderer.meshHandle = meshHandle;
        skinnedRenderer.materialHandle = Engine::ResourceHandle<Engine::MaterialInterface>(matHandle);
        skinnedRenderer.skeletonHandle = skelHandle;

        auto& collider = world.emplace<Engine::ColliderComponent>(entity);
        Engine::MeshShape meshShape{};
        meshShape.MeshHandle = meshHandle;
        if (!collider.SetShape(meshShape)) return Fail("Failed to set ColliderComponent MeshShape.");

        Engine::ResourceReferenceSet sceneReferences;
        archive::JsonArchive sceneWriter;
        sceneWriter.SetWriteContext(&sceneReferences);

        if (!scene->WriteArchive(sceneWriter) || !sceneWriter.SaveToFile(L"resource-scene.json"))
            return Fail("Scene with ResourceHandle components failed to save.");

        const Engine::ResourceReferenceSet expectedReferences = {
            { meshHandle->GetKey(), meshHandle->GetPath() },
            { miHandle->GetKey(), miHandle->GetPath() },
            { matHandle->GetKey(), matHandle->GetPath() },
            { skelHandle->GetKey(), skelHandle->GetPath() }
        };

        if (sceneReferences != expectedReferences)
            return Fail("Scene resource references were missing or duplicated.");

        Engine::ResourceReferenceSet prefabReferences;
        archive::JsonArchive prefabWriter;
        prefabWriter.SetWriteContext(&prefabReferences);

        auto* prefabRoot = scene->FindEntityById(100);

        if (!prefabRoot || !scene->WritePrefabArchive(prefabWriter, *prefabRoot) ||
            prefabReferences != expectedReferences)
        {
            return Fail("Prefab resource references did not match the serialized subtree.");
        }

        if (!CheckStoredResourceReferences(sceneWriter, "Scene", expectedReferences) ||
            !CheckStoredResourceReferences(prefabWriter, "Prefab", expectedReferences))
        {
            return false;
        }

        archive::BeveArchive binaryWriter;

        if (!scene->WriteArchive(binaryWriter) || !scene->WritePrefabArchive(binaryWriter, *prefabRoot))
            return Fail("BEVE resource reference writing failed.");

        std::string binary;
        archive::BeveArchive binaryReader;

        if (!binaryWriter.Encode(binary) || !binaryReader.Decode(binary) ||
            !CheckStoredResourceReferences(binaryReader, "Scene", expectedReferences) ||
            !CheckStoredResourceReferences(binaryReader, "Prefab", expectedReferences))
        {
            return Fail("BEVE resource reference round-trip failed.");
        }

        {
            Engine::ResourceReferenceSet accumulated = expectedReferences;
            accumulated.insert({ L"OutsideThisScene", L"" });

            archive::JsonArchive writer;
            writer.SetWriteContext(&accumulated);

            if (!scene->WriteArchive(writer) ||
                !CheckStoredResourceReferences(writer, "Scene", expectedReferences) ||
                writer.GetWriteContext<Engine::ResourceReferenceSet>() != &accumulated ||
                accumulated.size() != expectedReferences.size() + 1)
            {
                return Fail("Document writing replaced or leaked the caller's resource reference context.");
            }
        }

        {
            Engine::ResourceReferenceSet captured;
            const Engine::ResourceReferenceSet meshOnly = {
                { meshHandle->GetKey(), meshHandle->GetPath() }
            };

            archive::BeveArchive writer;
            writer.SetWriteContext(&captured);

            const std::map<int, Engine::ResourceHandle<Engine::Mesh>> meshes{ { 0, meshHandle } };

            const auto mapResult = writer.WriteObjectMap("Map", meshes,
                [](archive::IArchive& object, const Engine::ResourceHandle<Engine::Mesh>& mesh)
                {
                    return archive_resource::WriteHandle(object, "Ref", mesh);
                });

            if (!mapResult || captured != meshOnly)
                return Fail("Object-map writing did not propagate the write context.");

            captured.clear();

            const auto objectArrayResult = writer.WriteObjectArray("Objects", 1,
                [&](archive::IArchive& object, std::size_t)
                {
                    return archive_resource::WriteHandle(object, "Ref", meshHandle);
                });

            if (!objectArrayResult || captured != meshOnly)
                return Fail("Object-array writing did not propagate the write context.");

            captured.clear();

            const auto arrayResult = writer.WriteArrayElements("Array", 1,
                [&](archive::IArchive& object, std::string_view key, std::size_t)
                {
                    return archive_resource::WriteHandle(object, key, meshHandle);
                });

            if (!arrayResult || captured != meshOnly)
                return Fail("Array-element writing did not propagate the write context.");
        }

        // Clear all resources from ResourceManager and reload the scene from scratch so ReadHandle resolves from disk.
        meshHandle.Reset();
        vsHandle.Reset();
        matHandle.Reset();
        miHandle.Reset();
        skelHandle.Reset();
        animHandle.Reset();
        modelHandle.Reset();
        staticRenderer = {};
        skinnedRenderer = {};
        collider = {};
        rm.Free();

        {
            const std::wstring meshKey = L"test-mesh.bammesh.json";
            const std::wstring missingKey = L"000-preload-missing.bammesh.json";

            const Engine::ResourceReferenceSet references = {
                { missingKey, missingKey },
                { meshKey, meshKey }
            };
            const Engine::ResourceReferenceSet expectedUnresolved = { { missingKey, missingKey } };

            if (rm.IsValid(rm.FindHandleByKey(meshKey)))
                return Fail("Resource preload fixture was not cold.");

            const auto unresolved = archive_resource::PreloadResourceReferences(references);
            auto prepared = rm.FindHandleByKey(meshKey);
            const auto* preparedMesh = dynamic_cast<const Engine::Mesh*>(rm.GetResource(prepared));

            if (unresolved != expectedUnresolved || !preparedMesh || preparedMesh->GetVertexCount() != 3)
                return Fail("Preloading did not continue after an unresolved reference.");

            if (!archive_resource::PreloadResourceReferences({ { meshKey, meshKey } }).empty() ||
                rm.FindHandleByKey(meshKey) != prepared)
            {
                return Fail("Preloading did not reuse the registered resource.");
            }

            if (Engine::IsFailure(rm.DestroyResource(prepared)))
                return Fail("Failed to clean up the direct preload check.");

            const std::string referenceJson =
                R"("ResourceReferences":[{"AssetKey":"000-preload-missing.bammesh.json"},)"
                R"({"AssetKey":"test-mesh.bammesh.json"}])";

            for (const std::string root : { "Scene", "Prefab" })
            {
                const std::string json = "{\"" + root + "\":" +
                    R"({"Version":1,"Name":"Preload","Active":true,"RootId":1,)" + referenceJson +
                    R"(,"Entities":[{"Id":1,"Components":[)" + TransformJson + R"(]}]}})";

                archive::JsonArchive reader;
                if (!reader.Decode(json))
                    return Fail("Resource preload document fixture could not be decoded.");

                ScenePtr document;

                if (root == "Scene")
                {
                    Engine::Scene* loaded = nullptr;
                    const auto result = Engine::Scene::CreateFromArchive(reader, loaded);
                    document.reset(loaded);

                    if (!result || !document)
                        return Fail("An unused unresolved preload reference rejected the scene.");
                }
                else
                {
                    document.reset(Engine::Scene::Create());
                    if (!document)
                        return Fail("Could not create the prefab preload target.");

                    Engine::Entity* loaded = nullptr;
                    if (!document->InstantiatePrefabArchive(reader, loaded) || !loaded)
                        return Fail("An unused unresolved preload reference rejected the prefab.");
                }

                prepared = rm.FindHandleByKey(meshKey);
                if (!rm.IsValid(prepared))
                    return Fail("Document loading did not preload its resource reference list.");

                if (Engine::IsFailure(rm.DestroyResource(prepared)))
                    return Fail("Failed to clean up the document preload check.");
            }

            const std::string requiredJson =
                R"({"Scene":{"Version":1,"Name":"Required","Active":true,)"
                R"("ResourceReferences":[{"AssetKey":"000-preload-missing.bammesh.json"}],)"
                R"("Entities":[{"Id":1,"Components":[)"
                R"({"Type":"Engine::StaticMeshRendererComponent","Data":{)"
                R"("meshHandle":{"Null":false,"AssetKey":"000-preload-missing.bammesh.json","AssetPath":""},)"
                R"("materialHandle":{"Null":true},"castShadow":true}}]}]}})";

            archive::JsonArchive requiredReader;
            if (!requiredReader.Decode(requiredJson))
                return Fail("Required-resource fixture could not be decoded.");

            Engine::Scene* rejected = nullptr;
            const auto result = Engine::Scene::CreateFromArchive(requiredReader, rejected);
            ScenePtr unexpected(rejected);

            if (result || rejected || !result.Field.starts_with("Entities"))
                return Fail("An unresolved component reference did not reject the scene.");
        }

        Engine::Handle loadedKeyOnlyMesh;

        {
            archive::JsonArchive reader;
            const char* json = R"({"Ref":{"Null":false,"AssetKey":"test-mesh.bammesh.json","AssetPath":""}})";

            if (!reader.Decode(json))
                return Fail("Failed to decode the key-only resource reference.");

            Engine::ResourceHandle<Engine::Mesh> restoredMesh;

            if (!archive_resource::ReadHandle(reader, "Ref", restoredMesh) || !restoredMesh.IsValid() ||
                restoredMesh->GetVertexCount() != 3 || restoredMesh->GetIndexCount() != 3)
            {
                return Fail("Failed to load an uncached resource using its exact AssetKey path.");
            }

            loadedKeyOnlyMesh = restoredMesh.GetRawHandle();
        }

        if (Engine::IsFailure(rm.DestroyResource(loadedKeyOnlyMesh)))
            return Fail("Failed to clean up the key-only resource check.");

        Engine::Handle basenameMeshHandle;
        Engine::Handle pathMeshHandle;

        {
            MeshCreateDesc basenameDesc = meshDesc;
            basenameDesc.Key = L"test-mesh";
            basenameDesc.Path = L"test-mesh";

            auto basenameMesh = rm.AddResource<Engine::Mesh>(basenameDesc.Key, Engine::Mesh::Create(&basenameDesc));
            if (!basenameMesh.IsValid())
                return Fail("Failed to create the basename collision fixture.");

            archive::JsonArchive reader;
            const char* json =
                R"({"Ref":{"Null":false,"AssetKey":"Models/test-mesh.bammesh","AssetPath":"test-mesh.bammesh.json"}})";

            if (!reader.Decode(json))
                return Fail("Failed to decode the path-qualified resource reference.");

            Engine::ResourceHandle<Engine::Mesh> restoredMesh;

            if (!archive_resource::ReadHandle(reader, "Ref", restoredMesh) || !restoredMesh.IsValid() ||
                restoredMesh.GetRawHandle() == basenameMesh.GetRawHandle() ||
                restoredMesh->GetVertexCount() != 3 || restoredMesh->GetIndexCount() != 3)
            {
                return Fail("A path-qualified resource reference resolved to an unrelated basename resource.");
            }

            basenameMeshHandle = basenameMesh.GetRawHandle();
            pathMeshHandle = restoredMesh.GetRawHandle();
        }

        if (Engine::IsFailure(rm.DestroyResource(pathMeshHandle)) ||
            Engine::IsFailure(rm.DestroyResource(basenameMeshHandle)))
        {
            return Fail("Failed to clean up the basename collision check.");
        }

        ModelCreateDesc reloadedModelDesc{};
        reloadedModelDesc.Key = L"test-model.bammodel.json";
        reloadedModelDesc.Path = L"test-model.bammodel.json";
        auto reloadedModel = rm.LoadResource<Engine::Model>(&reloadedModelDesc);
        const auto reloadedMeshes = reloadedModel.IsValid() ? reloadedModel->GetMeshes() : vector<ResourceHandle<Engine::Mesh>>{};
        const auto reloadedMaterials = reloadedModel.IsValid() ? reloadedModel->GetMaterials() : vector<ResourceHandle<Engine::MaterialInterface>>{};
        const auto reloadedAnimations = reloadedModel.IsValid() ? reloadedModel->GetAnimations() : vector<ResourceHandle<Engine::Animation>>{};
        const auto reloadedSkeleton = reloadedModel.IsValid() ? reloadedModel->GetSkeleton() : ResourceHandle<Engine::Skeleton>{};
        float reloadedRoughness = 0.0f;
        if (!reloadedModel.IsValid() || reloadedMeshes.size() != 1 ||
            reloadedMaterials.size() != 1 || reloadedAnimations.size() != 1 ||
            !reloadedSkeleton.IsValid() || reloadedSkeleton->GetBoneIndex(L"RootBone") != 0 ||
            !reloadedMeshes[0].IsValid() || reloadedMeshes[0]->GetVertexCount() != 3 ||
            reloadedMeshes[0]->GetIndexCount() != 3 ||
            !reloadedMaterials[0].IsValid() ||
            !TryGetFloatParameter(reloadedMaterials[0].Get(), "Roughness", reloadedRoughness) ||
            reloadedRoughness != 0.75f)
            return Fail("Reloaded Model or its dependent Mesh/Material/Skeleton/Animation resources did not match.");

        archive::JsonArchive sceneReader;
        if (!sceneReader.LoadFromFile(L"resource-scene.json"))
            return Fail("Failed to read resource-scene.json.");
        Engine::Scene* restoredRaw = nullptr;
        if (!Engine::Scene::CreateFromArchive(sceneReader, restoredRaw) || restoredRaw == nullptr)
            return Fail("Failed to restore scene with ResourceHandle components.");
        ScenePtr restored(restoredRaw);
        auto* restoredActor = restored->FindEntityById(100);
        if (restoredActor == nullptr) return Fail("Restored actor not found.");
        const auto restoredHandle = restoredActor->GetEntityHandle();
        auto& restoredWorld = restored->GetRegistry();
        const auto& restoredStatic = restoredWorld.get<Engine::StaticMeshRendererComponent>(restoredHandle);
        const auto& restoredSkinned = restoredWorld.get<Engine::SkinnedMeshRendererComponent>(restoredHandle);
        const auto& restoredCollider = restoredWorld.get<Engine::ColliderComponent>(restoredHandle);
        const auto* restoredShape = std::get_if<Engine::MeshShape>(&restoredCollider.GetShape());
        float restoredRoughness = 0.0f;
        if (!restoredStatic.meshHandle.IsValid() || !restoredStatic.materialHandle.IsValid() ||
            restoredStatic.meshHandle->GetVertexCount() != 3 ||
            !TryGetFloatParameter(restoredStatic.materialHandle.Get(), "Roughness", restoredRoughness) ||
            restoredRoughness != 0.25f || !restoredSkinned.skeletonHandle.IsValid() ||
            restoredSkinned.skeletonHandle->GetBoneIndex(L"RootBone") != 0 ||
            restoredShape == nullptr || !restoredShape->MeshHandle.IsValid())
            return Fail("Restored scene ResourceHandle components did not preserve referenced assets.");

        ScenePtr linked(Engine::Scene::Create());
        const auto originalMaterial = reloadedMaterials[0];
        const auto newMaterial = restoredStatic.materialHandle;

        Engine::Entity* rootA = nullptr;
        Engine::Entity* rootB = nullptr;
        if (!linked || linked->InstantiateLinkedModel(reloadedModel, originalMaterial, rootA) != Engine::EResult::Success ||
            linked->InstantiateLinkedModel(reloadedModel, originalMaterial, rootB) != Engine::EResult::Success ||
            !rootA || !rootB)
            return Fail("Linked model placement failed.");

        const auto findBodyMesh = [](const Engine::Entity& root) -> entt::entity
            {
                const auto& world = root.GetScene()->GetRegistry();
                const auto* rootLink = world.try_get<Engine::HierarchyComponent>(root.GetEntityHandle());
                if (!rootLink || rootLink->children.size() != 1 || !world.valid(rootLink->children[0]))
                    return entt::null;

                const auto* nodeLink = world.try_get<Engine::HierarchyComponent>(rootLink->children[0]);
                if (!nodeLink || nodeLink->children.size() != 1 || !world.valid(nodeLink->children[0]))
                    return entt::null;

                const auto mesh = nodeLink->children[0];
                const auto* source = world.try_get<Engine::ModelSourceComponent>(mesh);
                return source && source->isMeshAttachment && source->sourceNodeKey == L"4:Body" &&
                    source->meshAttachmentKey == L"BodyMesh" && world.all_of<Engine::SkinnedMeshRendererComponent>(mesh)
                    ? mesh : entt::null;
            };

        const auto partA = findBodyMesh(*rootA);
        const auto partB = findBodyMesh(*rootB);
        if (partA == entt::null || partB == entt::null || partA == partB)
            return Fail("Linked model part identities were not created.");

        auto& linkedWorld = linked->GetRegistry();
        linkedWorld.get<Engine::ModelSourceComponent>(partB).useModelMaterial = false;
        if (newMaterial == originalMaterial || reloadedModel->SetMaterial(0, newMaterial) != Engine::EResult::Success)
            return Fail("Could not change the source model material for the test.");

        bool changedA = false;
        bool changedB = false;
        if (linked->RefreshLinkedModelParts(*rootA, changedA) != Engine::EResult::Success || !changedA ||
            linked->RefreshLinkedModelParts(*rootB, changedB) != Engine::EResult::Success || changedB ||
            linkedWorld.get<Engine::SkinnedMeshRendererComponent>(partA).materialHandle != newMaterial ||
            linkedWorld.get<Engine::SkinnedMeshRendererComponent>(partB).materialHandle != originalMaterial)
            return Fail("Model refresh did not preserve the per-instance material override.");

        const auto rootBId = linkedWorld.get<Engine::IDComponent>(rootB->GetEntityHandle()).id;
        archive::JsonArchive linkedArchive;
        Engine::Scene* reopenedRaw = nullptr;
        if (!linked->WriteArchive(linkedArchive) || !Engine::Scene::CreateFromArchive(linkedArchive, reopenedRaw) || !reopenedRaw)
            return Fail("Linked model scene round trip failed.");

        ScenePtr reopened(reopenedRaw);
        auto* reopenedRoot = reopened->FindEntityById(rootBId);
        const auto reopenedPart = reopenedRoot ? findBodyMesh(*reopenedRoot) : entt::null;
        if (reopenedPart == entt::null)
            return Fail("Linked model part identity did not survive the scene round trip.");

        auto& reopenedWorld = reopened->GetRegistry();
        auto* partEntity = reopened->FindEntity(reopenedPart);
        bool restoredMaterial = false;
        if (!partEntity || reopenedWorld.get<Engine::ModelSourceComponent>(reopenedPart).useModelMaterial ||
            reopenedWorld.get<Engine::SkinnedMeshRendererComponent>(reopenedPart).materialHandle != originalMaterial ||
            reopened->RestoreLinkedModelPartMaterial(*partEntity, restoredMaterial) != Engine::EResult::Success ||
            !restoredMaterial || !reopenedWorld.get<Engine::ModelSourceComponent>(reopenedPart).useModelMaterial ||
            reopenedWorld.get<Engine::SkinnedMeshRendererComponent>(reopenedPart).materialHandle != newMaterial)
            return Fail("The restored part could not return to the source model material.");

        if (reloadedModel->SetMaterial(0, originalMaterial) != Engine::EResult::Success)
            return Fail("Could not restore the test model material.");

        return true;
    }

    bool CheckShaderBinaryPaths()
    {
        auto& manager = Engine::ResourceManager::Get();
        const std::wstring assetPath = L"shader-path-round-trip.bamshader.json";

        ShaderDesc desc{};
        desc.Key = L"shader-path-round-trip";
        desc.Path = L"Shaders/GraphMaterialPS.cso";
        desc.spirvPath = L"Shaders/GraphMaterialPS.spv";
        desc.shaderType = EShaderType::Fragment;

        const auto releaseResource = [](Engine::Resource* resource)
            {
                if (resource)
                {
                    resource->Free();
                    delete resource;
                }
            };

        auto* shader = Engine::Shader::Create(&desc);
        std::unique_ptr<Engine::Resource, decltype(releaseResource)> source(shader, releaseResource);
        if (!shader || shader->GetDxilPath() != desc.Path || shader->GetSpirvPath() != desc.spirvPath)
            return Fail("Shader binary path initialization failed.");

        if (Engine::IsFailure(manager.SaveToJsonFile(source.get(), assetPath)))
            return Fail("Initial shader archive save failed.");

        ShaderDesc loadDesc{};
        loadDesc.Key = assetPath;
        loadDesc.Path = assetPath;

        for (int attempt = 0; attempt < 3; ++attempt)
        {
            auto loaded = manager.LoadResource<Engine::Shader>(&loadDesc);
            if (!loaded.IsValid())
                return Fail("Shader archive load failed.");

            if (loaded->GetPath() != assetPath || loaded->GetDxilPath() != desc.Path ||
                loaded->GetSpirvPath() != desc.spirvPath)
                return Fail("Shader asset and binary paths were not preserved.");

            if (Engine::IsFailure(manager.SaveToJsonFile(loaded.Get(), assetPath)))
                return Fail("Shader archive resave failed.");

            const auto handle = loaded.GetRawHandle();
            loaded.Reset();

            if (Engine::IsFailure(manager.DestroyResource(handle)))
                return Fail("Shader test resource cleanup failed.");
        }

        return true;
    }

    bool CheckMeshArchiveValidation()
    {
        Engine::VertexPosition positions[3] = {
            { { 0.0f, 0.0f, 0.0f } }, { { 1.0f, 0.0f, 0.0f } }, { { 0.0f, 1.0f, 0.0f } }
        };
        Engine::uint32 indices[3] = { 0, 1, 2 };
        constexpr auto positionSlot = static_cast<Engine::uint32>(Engine::EMeshStream::Position);
        constexpr auto streamCount = static_cast<std::size_t>(Engine::EMeshStream::Max);

        Engine::MeshCreateDesc desc{};
        desc.Streams[positionSlot] = { positions, 3, sizeof(Engine::VertexPosition) };
        desc.IndexData = indices;
        desc.IndexCount = 3;
        desc.IndexStride = sizeof(Engine::uint32);

        auto* source = Engine::Mesh::Create(&desc);
        if (!source) return Fail("Mesh archive validation setup failed.");

        const auto rejects = [&](bool invalidIndex) -> bool
            {
                archive::JsonArchive ar;
                std::string before, after;
                const auto geometryVersion = source->GetGeometryVersion();
                if (!source->WriteArchive(ar) || !ar.Encode(before)) return false;

                if (invalidIndex)
                {
                    if (!ar.Write("m_IndexCount", Engine::uint32{ 4 }) ||
                        !ar.Write("m_IndexStride", Engine::uint32{ 3 })) return false;
                }
                else
                {
                    std::array<Engine::uint32, streamCount> counts{};
                    std::array<Engine::uint32, streamCount> strides{};
                    counts[positionSlot] = static_cast<Engine::uint32>(sizeof(positions) / sizeof(float));
                    strides[positionSlot] = sizeof(float);
                    if (!ar.WriteArray("m_StreamCounts", counts) ||
                        !ar.WriteArray("m_StreamStrides", strides)) return false;
                }

                const auto result = source->ReadArchive(ar);
                archive::JsonArchive preserved;
                return result.Code == archive::ArchiveErrorCode::InvalidValue &&
                    result.Field == "Resource.OnPostLoad" &&
                    source->GetGeometryVersion() == geometryVersion &&
                    source->WriteArchive(preserved) && preserved.Encode(after) && before == after;
            };

        bool ok = rejects(true) && rejects(false);

        const glm::mat4 identity(1.0f);
        const Engine::Ray oldRay({ 0.25f, 0.25f, -5.0f }, { 0.0f, 0.0f, 1.0f });
        const Engine::Ray newRay({ 10.25f, 0.25f, -5.0f }, { 0.0f, 0.0f, 1.0f });
        Engine::HitResult hit{};

        // 재로드 전에 기존 위치의 BVH를 만든다.
        ok = ok && Engine::Collision::Raycast(oldRay, *source, identity, hit, 20.0f) &&
            std::abs(hit.distance - 5.0f) < 0.001f;

        for (auto& vertex : positions) vertex.position += glm::vec3(10.0f, 0.0f, 2.0f);

        auto* replacement = Engine::Mesh::Create(&desc);
        if (!replacement)
        {
            source->Free();
            delete source;
            return Fail("Replacement mesh creation failed.");
        }

        archive::JsonArchive incoming;
        const auto written = replacement->WriteArchive(incoming);
        replacement->Free();
        delete replacement;

        const auto previousVersion = source->GetGeometryVersion();
        ok = ok && written && source->ReadArchive(incoming) &&
            source->GetGeometryVersion() == previousVersion + 1 &&
            glm::length(source->GetMin() - glm::vec3(10.0f, 0.0f, 2.0f)) < 0.001f &&
            glm::length(source->GetMax() - glm::vec3(11.0f, 1.0f, 2.0f)) < 0.001f;

        ok = ok && !Engine::Collision::Raycast(oldRay, *source, identity, hit, 20.0f) &&
            Engine::Collision::Raycast(newRay, *source, identity, hit, 20.0f) &&
            std::abs(hit.distance - 7.0f) < 0.001f;

        // 성공한 재로드 이후에도 잘못된 입력으로 현재 상태가 손상되지 않아야 한다.
        ok = ok && rejects(true) && rejects(false);

        source->Free();
        delete source;
        return ok || Fail("Mesh archive rollback, commit, or BVH refresh failed.");
    }

    bool CheckResourceRuntimeVersion()
    {
        Engine::MeshCreateDesc desc{};
        auto* mesh = Engine::Mesh::Create(&desc);
        if (!mesh) return Fail("Runtime version test setup failed.");

        mesh->IncreaseVersion();
        mesh->IncreaseVersion();

        archive::JsonArchive ar;
        bool hasVersion = true;
        bool ok = mesh->WriteArchive(ar) &&
            ar.HasField("m_Version", hasVersion) && !hasVersion;

        const wchar_t* path = L"runtime-version.bammesh.json";

        // 이전 형식의 파일에 남아 있는 실행 버전을 재현한다.
        ok = ok && ar.Write("m_Version", Engine::uint32{ 0 }) && ar.SaveToFile(path);

        Engine::uint32 expectedVersion = mesh->GetVersion();
        for (int i = 0; ok && i < 2; ++i)
        {
            ++expectedVersion;
            ok = mesh->Reload(path) == Engine::EResult::Success &&
                mesh->GetVersion() == expectedVersion;
        }

        // 실패한 리로드에서는 두 버전 모두 유지되어야 한다.
        ok = ok && ar.Write("m_IndexCount", Engine::uint32{ 1 }) &&
            ar.Write("m_IndexStride", Engine::uint32{ 3 }) && ar.SaveToFile(path);

        const auto previousVersion = mesh->GetVersion();
        const auto previousGeometryVersion = mesh->GetGeometryVersion();
        ok = ok && mesh->Reload(path) != Engine::EResult::Success &&
            mesh->GetVersion() == previousVersion &&
            mesh->GetGeometryVersion() == previousGeometryVersion;

        mesh->Free();
        delete mesh;
        return ok || Fail("Resource runtime version serialization or reload handling failed.");
    }

    bool CheckPrefabRoundTrip()
    {
        auto scene = MakeScene();
        auto& manager = Engine::SceneManager::Get();
        if (manager.OpenScene(scene.get()) != Engine::EResult::Success)
            return Fail("Failed to open scene for prefab check.");

        Engine::Entity* rootEntity = scene->FindEntityById(RootId);
        if (rootEntity == nullptr)
            return Fail("Root entity not found for prefab check.");
        const auto rootHandle = rootEntity->GetEntityHandle();

        archive::JsonArchive prefabWriter;
        if (manager.SavePrefab(prefabWriter, L"actor.bamprefab.json", *rootEntity) != Engine::EResult::Success)
            return Fail("SavePrefab failed.");

        archive::JsonArchive prefabReader;
        Engine::Entity* instantiatedRootEntity = nullptr;
        if (manager.InstantiatePrefab(
                prefabReader, L"actor.bamprefab.json", instantiatedRootEntity, rootEntity) != Engine::EResult::Success ||
            instantiatedRootEntity == nullptr || instantiatedRootEntity->GetEntityHandle() == rootHandle)
            return Fail("InstantiatePrefab failed.");

        const auto instantiatedRoot = instantiatedRootEntity->GetEntityHandle();
        auto& world = scene->GetRegistry();
        const auto newRootId = world.get<Engine::IDComponent>(instantiatedRoot).id;
        if (newRootId == 0 || newRootId == RootId || newRootId == FirstChildId || newRootId == SecondChildId)
            return Fail("Instantiated prefab root ID collided with existing scene IDs.");
        const auto& rootHierarchy = world.get<Engine::HierarchyComponent>(instantiatedRoot);
        if (rootHierarchy.parent != rootHandle || rootHierarchy.children.size() != 2)
            return Fail("Instantiated prefab hierarchy parent or children count mismatch.");
        const auto child0 = rootHierarchy.children[0];
        const auto child1 = rootHierarchy.children[1];
        if (world.get<Engine::NameComponent>(child0).name != L"둘째" ||
            world.get<Engine::NameComponent>(child1).name != L"첫째" ||
            world.get<Engine::HierarchyComponent>(child0).parent != instantiatedRoot ||
            world.get<Engine::HierarchyComponent>(child1).parent != instantiatedRoot)
            return Fail("Instantiated prefab children names or parent links mismatch.");

        // Verify rollback when prefab contains an invalid component
        const std::size_t countBefore = world.storage<Engine::IDComponent>().size();
        archive::JsonArchive badPrefab;
        if (!badPrefab.Decode(R"({"Prefab":{"Version":1,"RootId":1,"Entities":[)"
            R"({"Id":1,"Components":[{"Type":"UnknownComponent","Data":{}}]}]}})"))
            return Fail("Failed to decode malformed prefab test JSON.");
        Engine::Entity* failedRoot = nullptr;
        if (scene->InstantiatePrefabArchive(badPrefab, failedRoot).Code != archive::ArchiveErrorCode::UnsupportedType ||
            failedRoot != nullptr || world.storage<Engine::IDComponent>().size() != countBefore)
            return Fail("Failed prefab instantiation did not roll back created entities.");

        auto& rollbackParent = scene->CreateEntity();
        const auto rollbackParentHandle = rollbackParent.GetEntityHandle();
        const auto rollbackCount = world.storage<Engine::IDComponent>().size();

        const std::string reorderedJson =
            R"({"Prefab":{"Version":1,"RootId":1,"Entities":[)"
            R"({"Id":2,"Hierarchy":{"Parent":1,"Children":[]},"Components":[)" + TransformJson +
            R"(]},{"Id":1,"Hierarchy":{"Parent":0,"Children":[2]},"Components":[)" + TransformJson + R"(]}]}})";

        archive::JsonArchive rollbackReader;
        if (!rollbackReader.Decode(reorderedJson))
            return Fail("Failed to decode reordered prefab.");

        Engine::Entity* rollbackRoot = nullptr;
        const auto rollbackResult = scene->InstantiatePrefabArchive(rollbackReader, rollbackRoot, &rollbackParent);

        if (rollbackResult.Code != archive::ArchiveErrorCode::InvalidValue ||
            rollbackResult.Field != "Prefab.Parent" || rollbackRoot != nullptr ||
            world.storage<Engine::IDComponent>().size() != rollbackCount ||
            scene->FindEntity(rollbackParentHandle) == nullptr)
            return Fail("Reordered prefab rollback failed.");

        return true;
    }

    bool CheckLinkedModelPartMeshOverrideAndReimportRoundTrip()
    {
        auto& rm = Engine::ResourceManager::Get();

        const std::wstring cleanupFiles[] = {
            L"part-test-skel.bamskel.json", L"part-head.bammesh.json", L"part-body.bammesh.json",
            L"part-armor.bammesh.json", L"part-incompatible.bammesh.json", L"part-head.bammat.json",
            L"part-body.bammat.json", L"part-armor.bammat.json", L"part-character.bammodel.json"
        };
        const auto cleanup = [&cleanupFiles]()
        {
            for (const auto& path : cleanupFiles)
            {
                std::error_code ec;
                std::filesystem::remove(path, ec);
                std::filesystem::remove(Engine::PlatformPath::ResolveProjectRelative(path), ec);
            }
        };
        cleanup();

        // 1. 스켈레톤 준비 및 리그 시그니처 계산
        SkeletonCreateDesc skelDesc{};
        skelDesc.Key = L"part-test-skel.bamskel.json";
        skelDesc.Path = L"part-test-skel.bamskel.json";
        Bone rootBone{};
        rootBone.Name = L"RootBone";
        rootBone.ParentIndex = -1;
        skelDesc.Bones.push_back(rootBone);
        const uint64 skinSignature = Engine::Skeleton::ComputeSkinBindingSignature(skelDesc.Bones);
        auto skelHandle = rm.AddResource<Engine::Skeleton>(skelDesc.Key, Engine::Skeleton::Create(&skelDesc));
        if (!skelHandle.IsValid() || Engine::IsFailure(rm.SaveToJsonFile(skelHandle.Get(), skelDesc.Path)))
        {
            cleanup();
            return Fail("Failed to create skeleton for linked part test.");
        }

        // 2. 메시 생성 헬퍼
        Engine::VertexPosition positions[3] = {
            { { 0.0f, 0.0f, 0.0f } }, { { 1.0f, 0.0f, 0.0f } }, { { 0.0f, 1.0f, 0.0f } }
        };
        Engine::VertexSkinData skinData[3] = {
            { { 0, 0, 0, 0 }, { 1.0f, 0.0f, 0.0f, 0.0f } },
            { { 0, 0, 0, 0 }, { 1.0f, 0.0f, 0.0f, 0.0f } },
            { { 0, 0, 0, 0 }, { 1.0f, 0.0f, 0.0f, 0.0f } }
        };
        uint32 indices[3] = { 0, 1, 2 };

        const auto createSkinMesh = [&](const std::wstring& key, uint64 signature, bool withSkinData)
            -> Engine::ResourceHandle<Engine::Mesh>
        {
            MeshCreateDesc desc{};
            desc.Key = key;
            desc.Path = key;
            desc.Streams[(uint32)Engine::EMeshStream::Position] = { positions, 3, sizeof(Engine::VertexPosition) };
            if (withSkinData)
                desc.Streams[(uint32)Engine::EMeshStream::SkinData] = { skinData, 3, sizeof(Engine::VertexSkinData) };
            desc.IndexData = indices;
            desc.IndexCount = 3;
            desc.IndexStride = sizeof(uint32);
            desc.SkinBindingSignature = signature;
            auto handle = rm.AddResource<Engine::Mesh>(desc.Key, Engine::Mesh::Create(&desc));
            if (handle.IsValid())
                rm.SaveToJsonFile(handle.Get(), key);
            return handle;
        };

        const auto headMesh = createSkinMesh(L"part-head.bammesh.json", skinSignature, true);
        const auto bodyMesh = createSkinMesh(L"part-body.bammesh.json", skinSignature, true);
        const auto armorMesh = createSkinMesh(L"part-armor.bammesh.json", skinSignature, true);
        const auto incompatibleMesh = createSkinMesh(L"part-incompatible.bammesh.json", skinSignature + 999, true);

        if (!headMesh.IsValid() || !bodyMesh.IsValid() || !armorMesh.IsValid() || !incompatibleMesh.IsValid())
        {
            cleanup();
            return Fail("Failed to create meshes for linked part test.");
        }

        // 3. 머티리얼 준비
        Engine::MaterialDesc matDesc{};
        matDesc.CullMode = Engine::ECullMode::None;

        const auto createMat = [&](const std::wstring& key, float roughness)
            -> Engine::ResourceHandle<Engine::Material>
        {
            matDesc.Key = key;
            matDesc.Path = key;
            auto handle = rm.AddResource<Engine::Material>(matDesc.Key, Engine::Material::Create(&matDesc));
            if (handle.IsValid())
            {
                handle->SetFloat("Roughness", roughness);
                rm.SaveToJsonFile(handle.Get(), key);
            }
            return handle;
        };

        const auto headMat = createMat(L"part-head.bammat.json", 0.5f);
        const auto bodyMat = createMat(L"part-body.bammat.json", 0.8f);
        const auto armorMat = createMat(L"part-armor.bammat.json", 0.1f);
        if (!headMat.IsValid() || !bodyMat.IsValid() || !armorMat.IsValid())
        {
            cleanup();
            return Fail("Failed to create materials for linked part test.");
        }

        // 4. 멀티 파트 모델 생성 (Node 0: Head, Node 1: Body)
        ModelCreateDesc modelDesc{};
        modelDesc.Key = L"part-character.bammodel.json";
        modelDesc.Path = L"part-character.bammodel.json";
        modelDesc.Skeleton = skelHandle;
        modelDesc.Meshes = { headMesh, bodyMesh };
        modelDesc.Materials = { headMat, bodyMat };
        modelDesc.MeshMaterialIndices = { 0, 1 };

        Engine::ModelNode headNode;
        headNode.Name = L"Head";
        headNode.SourceNodeKey = L"1:Head";
        headNode.MeshIndices = { 0 };
        headNode.MeshAttachmentKeys = { L"HeadMesh" };

        Engine::ModelNode bodyNode;
        bodyNode.Name = L"Body";
        bodyNode.SourceNodeKey = L"2:Body";
        bodyNode.MeshIndices = { 1 };
        bodyNode.MeshAttachmentKeys = { L"BodyMesh" };

        modelDesc.Nodes = { headNode, bodyNode };

        auto charModel = rm.AddResource<Engine::Model>(modelDesc.Key, Engine::Model::Create(&modelDesc));
        if (!charModel.IsValid() || Engine::IsFailure(rm.SaveToJsonFile(charModel.Get(), modelDesc.Path)))
        {
            cleanup();
            return Fail("Failed to create multi-part character model.");
        }

        // 5. 씬에 인스턴스화
        ScenePtr scene(Engine::Scene::Create());
        Engine::Entity* charRoot = nullptr;
        if (!scene || scene->InstantiateLinkedModel(charModel, headMat, charRoot) != Engine::EResult::Success || !charRoot)
        {
            cleanup();
            return Fail("Failed to instantiate linked multi-part model.");
        }

        const auto findPart = [](const Engine::Entity& root, const std::wstring& nodeKey, const std::wstring& partKey)
            -> entt::entity
        {
            const auto& reg = root.GetScene()->GetRegistry();
            const auto rootEntity = root.GetEntityHandle();
            const auto* link = reg.try_get<Engine::HierarchyComponent>(rootEntity);
            if (!link) return entt::null;

            for (auto nodeEntity : link->children)
            {
                if (!reg.valid(nodeEntity)) continue;
                const auto* nodeLink = reg.try_get<Engine::HierarchyComponent>(nodeEntity);
                if (!nodeLink) continue;

                for (auto meshEntity : nodeLink->children)
                {
                    if (!reg.valid(meshEntity)) continue;
                    const auto* src = reg.try_get<Engine::ModelSourceComponent>(meshEntity);
                    if (src && src->isMeshAttachment && src->sourceNodeKey == nodeKey && src->meshAttachmentKey == partKey)
                        return meshEntity;
                }
            }
            return entt::null;
        };

        const auto headPart = findPart(*charRoot, L"1:Head", L"HeadMesh");
        const auto bodyPart = findPart(*charRoot, L"2:Body", L"BodyMesh");
        if (headPart == entt::null || bodyPart == entt::null)
        {
            cleanup();
            return Fail("Failed to find head or body parts in instantiated model.");
        }

        auto& world = scene->GetRegistry();
        auto* bodyEntity = scene->FindEntity(bodyPart);
        if (!bodyEntity)
        {
            cleanup();
            return Fail("Body entity not found in scene.");
        }

        // 6. 스켈레톤 비호환 메시 교체 시도 -> 반드시 거부(EResult::InvalidArgument)되어야 함!
        if (scene->ReplaceLinkedSkinnedPartMesh(*bodyEntity, incompatibleMesh) != Engine::EResult::InvalidArgument)
        {
            cleanup();
            return Fail("ReplaceLinkedSkinnedPartMesh did not reject incompatible skeleton mesh.");
        }

        if (world.get<Engine::SkinnedMeshRendererComponent>(bodyPart).meshHandle != bodyMesh)
        {
            cleanup();
            return Fail("Incompatible mesh replacement modified the existing mesh.");
        }

        // 7. 호환되는 갑옷 메시 및 머티리얼로 교체
        if (scene->ReplaceLinkedSkinnedPartMesh(*bodyEntity, armorMesh) != Engine::EResult::Success)
        {
            cleanup();
            return Fail("Failed to replace linked skinned part with compatible armor mesh.");
        }

        auto& bodyRenderer = world.get<Engine::SkinnedMeshRendererComponent>(bodyPart);
        auto& bodySource = world.get<Engine::ModelSourceComponent>(bodyPart);
        if (bodyRenderer.meshHandle != armorMesh || bodySource.useModelMesh)
        {
            cleanup();
            return Fail("Body part mesh or useModelMesh flag was not properly updated.");
        }

        bodyRenderer.materialHandle = armorMat;
        bodySource.useModelMaterial = false;

        const auto rootId = world.get<Engine::IDComponent>(charRoot->GetEntityHandle()).id;

        // 8. 씬 직렬화 및 역직렬화 (라운드트립 검증)
        archive::JsonArchive sceneArchive;
        if (!scene->WriteArchive(sceneArchive))
        {
            cleanup();
            return Fail("Failed to write scene archive with mesh override.");
        }

        Engine::Scene* loadedRaw = nullptr;
        if (!Engine::Scene::CreateFromArchive(sceneArchive, loadedRaw) || !loadedRaw)
        {
            cleanup();
            return Fail("Failed to load scene from archive with mesh override.");
        }

        ScenePtr loaded(loadedRaw);
        auto* loadedRoot = loaded->FindEntityById(rootId);
        if (!loadedRoot)
        {
            cleanup();
            return Fail("Loaded root entity not found.");
        }

        const auto loadedHead = findPart(*loadedRoot, L"1:Head", L"HeadMesh");
        const auto loadedBody = findPart(*loadedRoot, L"2:Body", L"BodyMesh");
        if (loadedHead == entt::null || loadedBody == entt::null)
        {
            cleanup();
            return Fail("Loaded scene lost part identities.");
        }

        auto& loadedWorld = loaded->GetRegistry();
        const auto& loadedHeadRenderer = loadedWorld.get<Engine::SkinnedMeshRendererComponent>(loadedHead);
        const auto& loadedHeadSource = loadedWorld.get<Engine::ModelSourceComponent>(loadedHead);
        const auto& loadedBodyRenderer = loadedWorld.get<Engine::SkinnedMeshRendererComponent>(loadedBody);
        const auto& loadedBodySource = loadedWorld.get<Engine::ModelSourceComponent>(loadedBody);

        if (loadedHeadRenderer.meshHandle != headMesh || !loadedHeadSource.useModelMesh ||
            loadedHeadRenderer.materialHandle != headMat || !loadedHeadSource.useModelMaterial)
        {
            cleanup();
            return Fail("Unchanged head part was corrupted across round trip.");
        }

        if (loadedBodyRenderer.meshHandle != armorMesh || loadedBodySource.useModelMesh ||
            loadedBodyRenderer.materialHandle != armorMat || loadedBodySource.useModelMaterial)
        {
            cleanup();
            return Fail("Body part mesh/material overrides were not preserved across round trip.");
        }

        // 9. 원본 모델의 노드/메시 순서를 바꿔 재임포트 시뮬레이션
        ModelCreateDesc reorderedDesc = modelDesc;
        reorderedDesc.Meshes = { bodyMesh, headMesh };
        reorderedDesc.Materials = { bodyMat, headMat };
        reorderedDesc.MeshMaterialIndices = { 0, 1 };

        Engine::ModelNode reorderedHeadNode = headNode;
        reorderedHeadNode.MeshIndices = { 1 };

        Engine::ModelNode reorderedBodyNode = bodyNode;
        reorderedBodyNode.MeshIndices = { 0 };

        reorderedDesc.Key = L"part-character-reordered-temp.bammodel.json";
        reorderedDesc.Path = modelDesc.Path;
        reorderedDesc.Nodes = { reorderedBodyNode, reorderedHeadNode };

        auto tempHandle = rm.AddResource<Engine::Model>(reorderedDesc.Key, Engine::Model::Create(&reorderedDesc));
        if (!tempHandle.IsValid() || Engine::IsFailure(rm.SaveToJsonFile(tempHandle.Get(), modelDesc.Path)))
        {
            if (tempHandle.IsValid()) static_cast<void>(rm.DestroyResource(tempHandle.GetRawHandle()));
            cleanup();
            return Fail("Failed to write reordered model JSON.");
        }
        static_cast<void>(rm.DestroyResource(tempHandle.GetRawHandle()));

        const auto modelResolvedPath = Engine::PlatformPath::ResolveProjectRelative(modelDesc.Path);
        if (Engine::IsFailure(charModel->Reload(modelResolvedPath)))
        {
            cleanup();
            return Fail("Failed to reload reordered character model.");
        }

        bool refreshedChanged = false;
        if (loaded->RefreshLinkedModelParts(*loadedRoot, refreshedChanged) != Engine::EResult::Success)
        {
            cleanup();
            return Fail("RefreshLinkedModelParts failed after model reorder.");
        }

        if (loadedWorld.get<Engine::SkinnedMeshRendererComponent>(loadedBody).meshHandle != armorMesh ||
            loadedWorld.get<Engine::ModelSourceComponent>(loadedBody).useModelMesh ||
            loadedWorld.get<Engine::SkinnedMeshRendererComponent>(loadedBody).materialHandle != armorMat ||
            loadedWorld.get<Engine::ModelSourceComponent>(loadedBody).useModelMaterial)
        {
            cleanup();
            return Fail("Body overrides did not survive model node/mesh reordering.");
        }

        if (loadedWorld.get<Engine::SkinnedMeshRendererComponent>(loadedHead).meshHandle != headMesh ||
            !loadedWorld.get<Engine::ModelSourceComponent>(loadedHead).useModelMesh ||
            loadedWorld.get<Engine::SkinnedMeshRendererComponent>(loadedHead).materialHandle != headMat ||
            !loadedWorld.get<Engine::ModelSourceComponent>(loadedHead).useModelMaterial)
        {
            cleanup();
            return Fail("Head identity was corrupted after model node/mesh reordering.");
        }

        // 10. 모델 원본 메시 및 머티리얼로 복원
        auto* loadedBodyEntity = loaded->FindEntity(loadedBody);
        if (!loadedBodyEntity)
        {
            cleanup();
            return Fail("Loaded body entity not found for restore.");
        }

        bool restoredMesh = false;
        if (loaded->RestoreLinkedModelPartMesh(*loadedBodyEntity, restoredMesh) != Engine::EResult::Success ||
            !restoredMesh || !loadedWorld.get<Engine::ModelSourceComponent>(loadedBody).useModelMesh ||
            loadedWorld.get<Engine::SkinnedMeshRendererComponent>(loadedBody).meshHandle != bodyMesh)
        {
            cleanup();
            return Fail("Failed to restore body part to reordered source model mesh.");
        }

        bool restoredMat = false;
        if (loaded->RestoreLinkedModelPartMaterial(*loadedBodyEntity, restoredMat) != Engine::EResult::Success ||
            !restoredMat || !loadedWorld.get<Engine::ModelSourceComponent>(loadedBody).useModelMaterial ||
            loadedWorld.get<Engine::SkinnedMeshRendererComponent>(loadedBody).materialHandle != bodyMat)
        {
            cleanup();
            return Fail("Failed to restore body part to reordered source model material.");
        }

        cleanup();
        return true;
    }

    bool CheckSkinnedMeshRuntimeEquipmentAndTemporaryEffects()
    {
        auto& rm = Engine::ResourceManager::Get();

        const std::wstring cleanupFiles[] = {
            L"runtime-effect-skel.bamskel.json", L"runtime-iron.bammesh.json",
            L"runtime-leather.bammesh.json", L"runtime-incompatible.bammesh.json",
            L"runtime-shared.bammat.json", L"runtime-special.bammat.json"
        };
        const auto cleanup = [&cleanupFiles]()
        {
            for (const auto& path : cleanupFiles)
            {
                std::error_code ec;
                std::filesystem::remove(path, ec);
                std::filesystem::remove(Engine::PlatformPath::ResolveProjectRelative(path), ec);
            }
        };
        cleanup();

        // 1. 스켈레톤 준비
        SkeletonCreateDesc skelDesc{};
        skelDesc.Key = L"runtime-effect-skel.bamskel.json";
        skelDesc.Path = L"runtime-effect-skel.bamskel.json";
        Bone rootBone{};
        rootBone.Name = L"RootBone";
        rootBone.ParentIndex = -1;
        skelDesc.Bones.push_back(rootBone);
        const uint64 skinSignature = Engine::Skeleton::ComputeSkinBindingSignature(skelDesc.Bones);
        auto skelHandle = rm.AddResource<Engine::Skeleton>(skelDesc.Key, Engine::Skeleton::Create(&skelDesc));
        if (!skelHandle.IsValid())
        {
            cleanup();
            return Fail("Failed to create skeleton for runtime effect test.");
        }

        // 2. 메시 생성
        Engine::VertexPosition positions[3] = {
            { { 0.0f, 0.0f, 0.0f } }, { { 1.0f, 0.0f, 0.0f } }, { { 0.0f, 1.0f, 0.0f } }
        };
        Engine::VertexSkinData skinData[3] = {
            { { 0, 0, 0, 0 }, { 1.0f, 0.0f, 0.0f, 0.0f } },
            { { 0, 0, 0, 0 }, { 1.0f, 0.0f, 0.0f, 0.0f } },
            { { 0, 0, 0, 0 }, { 1.0f, 0.0f, 0.0f, 0.0f } }
        };
        uint32 indices[3] = { 0, 1, 2 };

        const auto createMesh = [&](const std::wstring& key, uint64 signature, bool withSkin)
            -> Engine::ResourceHandle<Engine::Mesh>
        {
            MeshCreateDesc desc{};
            desc.Key = key;
            desc.Path = key;
            desc.Streams[(uint32)Engine::EMeshStream::Position] = { positions, 3, sizeof(Engine::VertexPosition) };
            if (withSkin)
                desc.Streams[(uint32)Engine::EMeshStream::SkinData] = { skinData, 3, sizeof(Engine::VertexSkinData) };
            desc.IndexData = indices;
            desc.IndexCount = 3;
            desc.IndexStride = sizeof(uint32);
            desc.SkinBindingSignature = signature;
            auto handle = rm.AddResource<Engine::Mesh>(desc.Key, Engine::Mesh::Create(&desc));
            if (handle.IsValid())
                rm.SaveToJsonFile(handle.Get(), key);
            return handle;
        };

        const auto ironMesh = createMesh(L"runtime-iron.bammesh.json", skinSignature, true);
        const auto leatherMesh = createMesh(L"runtime-leather.bammesh.json", skinSignature, true);
        const auto incompatibleMesh = createMesh(L"runtime-incompatible.bammesh.json", skinSignature + 888, true);
        if (!ironMesh.IsValid() || !leatherMesh.IsValid() || !incompatibleMesh.IsValid())
        {
            cleanup();
            return Fail("Failed to create meshes for runtime effect test.");
        }

        // 3. 머티리얼 생성 (공유 머티리얼 및 전용 머티리얼)
        Engine::MaterialDesc matDesc{};
        matDesc.CullMode = Engine::ECullMode::None;
        const auto createMat = [&](const std::wstring& key, float roughness)
            -> Engine::ResourceHandle<Engine::Material>
        {
            matDesc.Key = key;
            matDesc.Path = key;
            auto handle = rm.AddResource<Engine::Material>(matDesc.Key, Engine::Material::Create(&matDesc));
            if (handle.IsValid())
            {
                handle->SetFloat("Roughness", roughness);
                rm.SaveToJsonFile(handle.Get(), key);
            }
            return handle;
        };

        const auto sharedMat = createMat(L"runtime-shared.bammat.json", 0.5f);
        const auto specialMat = createMat(L"runtime-special.bammat.json", 0.8f);
        if (!sharedMat.IsValid() || !specialMat.IsValid())
        {
            cleanup();
            return Fail("Failed to create materials for runtime effect test.");
        }

        // 4. 씬에 같은 머티리얼을 공유하는 두 캐릭터 엔티티 생성
        ScenePtr scene(Engine::Scene::Create());
        auto& world = scene->GetRegistry();

        const auto entityA = AddSpatialEntity(*scene, 201, L"CharacterA", { 0.0f, 0.0f, 0.0f });
        auto& compA = world.emplace<Engine::SkinnedMeshRendererComponent>(entityA);
        compA.meshHandle = ironMesh;
        compA.materialHandle = sharedMat;
        compA.skeletonHandle = skelHandle;

        const auto entityB = AddSpatialEntity(*scene, 202, L"CharacterB", { 2.0f, 0.0f, 0.0f });
        auto& compB = world.emplace<Engine::SkinnedMeshRendererComponent>(entityB);
        compB.meshHandle = ironMesh;
        compB.materialHandle = sharedMat;
        compB.skeletonHandle = skelHandle;

        // 5. 초기 상태 검증: 둘 다 기본 철갑옷과 공유 머티리얼, 발광값 0
        Engine::Mesh* meshA = nullptr;
        Engine::MaterialInterface* matA = nullptr;
        Engine::MaterialConstants constA{};
        compA.ResolveRenderState(meshA, matA, constA);

        Engine::Mesh* meshB = nullptr;
        Engine::MaterialInterface* matB = nullptr;
        Engine::MaterialConstants constB{};
        compB.ResolveRenderState(meshB, matB, constB);

        if (meshA != ironMesh.Get() || matA != sharedMat.Get() || constA.emissiveColor != glm::vec3(0.0f) ||
            meshB != ironMesh.Get() || matB != sharedMat.Get() || constB.emissiveColor != glm::vec3(0.0f))
        {
            cleanup();
            return Fail("Initial state of shared material characters was incorrect.");
        }

        // 6. 캐릭터 A에만 피격 발광 적용 (Red 발광, duration 1.0초, requestId 101)
        Engine::SkinnedMaterialParameterOverride hitEffect{};
        hitEffect.emissiveColor = glm::vec3(2.0f, 0.0f, 0.0f);
        compA.ApplyParameterOverride(hitEffect, 1.0f, 101);

        compA.ResolveRenderState(meshA, matA, constA);
        compB.ResolveRenderState(meshB, matB, constB);

        // A만 발광하고 B는 영향을 받지 않아야 함! (독립성 검증)
        if (constA.emissiveColor != glm::vec3(2.0f, 0.0f, 0.0f) || constB.emissiveColor != glm::vec3(0.0f))
        {
            cleanup();
            return Fail("Hit effect contaminated shared material instance across characters.");
        }

        // 7. 발광 도중 캐릭터 A의 런타임 갑옷 교체
        // 7-a. 스켈레톤 비호환 메시 장착 시도 -> 거부 확인
        if (compA.EquipRuntimeMesh(incompatibleMesh) != Engine::EResult::InvalidArgument ||
            compA.runtimeEquipment.isEquipped)
        {
            cleanup();
            return Fail("EquipRuntimeMesh accepted incompatible skeleton mesh.");
        }

        // 7-b. 호환 가죽갑옷으로 장착 교체
        if (compA.EquipRuntimeMesh(leatherMesh, specialMat) != Engine::EResult::Success ||
            !compA.runtimeEquipment.isEquipped)
        {
            cleanup();
            return Fail("EquipRuntimeMesh failed to equip compatible armor mesh.");
        }

        compA.ResolveRenderState(meshA, matA, constA);
        // 가죽갑옷과 전용 머티리얼로 교체되었으나, 피격 발광 효과는 그대로 유지되어야 함!
        if (meshA != leatherMesh.Get() || matA != specialMat.Get() ||
            constA.emissiveColor != glm::vec3(2.0f, 0.0f, 0.0f))
        {
            cleanup();
            return Fail("Armor replacement during hit effect did not preserve ongoing effect.");
        }

        // 8. 연속 피격 및 만료 타이머/요청 ID 갱신 검증
        compA.UpdateTemporaryEffects(0.5f);
        if (!compA.temporaryEffect.isActive || compA.temporaryEffect.remainingTime > 0.6f)
        {
            cleanup();
            return Fail("UpdateTemporaryEffects did not progress remaining time.");
        }

        // 새 피격 도착으로 만료 시각 1.0초 리셋 및 ID 102 갱신
        compA.ApplyParameterOverride(hitEffect, 1.0f, 102);
        if (compA.temporaryEffect.requestId != 102 || compA.temporaryEffect.remainingTime != 1.0f)
        {
            cleanup();
            return Fail("Consecutive hit effect did not renew expiration timer or requestId.");
        }

        // 이전 요청 ID(101)로 해제 시도 -> ID 불일치로 해제되지 않아야 함!
        compA.ClearTemporaryEffect(101);
        if (!compA.temporaryEffect.isActive)
        {
            cleanup();
            return Fail("Obsolete requestId prematurely cleared the renewed temporary effect.");
        }

        // 9. 피격 효과 만료 -> 발광은 꺼지고 새 갑옷(가죽갑옷)은 그대로 유지!
        compA.UpdateTemporaryEffects(1.5f);
        if (compA.temporaryEffect.isActive)
        {
            cleanup();
            return Fail("Temporary effect did not expire after duration elapsed.");
        }

        compA.ResolveRenderState(meshA, matA, constA);
        compB.ResolveRenderState(meshB, matB, constB);

        if (meshA != leatherMesh.Get() || matA != specialMat.Get() ||
            constA.emissiveColor != glm::vec3(0.0f))
        {
            cleanup();
            return Fail("New armor was not preserved after hit effect ended.");
        }

        if (meshB != ironMesh.Get() || matB != sharedMat.Get() || constB.emissiveColor != glm::vec3(0.0f))
        {
            cleanup();
            return Fail("Character B state was corrupted by Character A lifecycle.");
        }

        // 10. 씬 직렬화/역직렬화 후 수명 분리 검증 (런타임 장비는 씬에 저장되지 않아야 함)
        archive::JsonArchive runtimeArchive;
        if (!scene->WriteArchive(runtimeArchive))
        {
            cleanup();
            return Fail("Failed to write scene archive with runtime equipment.");
        }

        Engine::Scene* reloadedRaw = nullptr;
        if (!Engine::Scene::CreateFromArchive(runtimeArchive, reloadedRaw) || !reloadedRaw)
        {
            cleanup();
            return Fail("Failed to reload scene with runtime equipment.");
        }

        ScenePtr reloaded(reloadedRaw);
        auto* reloadedActorA = reloaded->FindEntityById(201);
        if (!reloadedActorA)
        {
            cleanup();
            return Fail("Reloaded actor A not found.");
        }

        const auto& reloadedCompA = reloaded->GetRegistry().get<Engine::SkinnedMeshRendererComponent>(
            reloadedActorA->GetEntityHandle());
        if (reloadedCompA.meshHandle != ironMesh || reloadedCompA.materialHandle != sharedMat ||
            reloadedCompA.runtimeEquipment.isEquipped || reloadedCompA.temporaryEffect.isActive)
        {
            cleanup();
            return Fail("Runtime equipment or temporary effect leaked into saved scene.");
        }

        cleanup();
        return true;
    }
}

int main()
{
    RegistryScope scope;
    if (Engine::ReflectionRegistry::Create() == nullptr ||
        Engine::ResourceManager::Create() == nullptr ||
        Engine::SceneManager::Create() == nullptr)
    {
        Fail("Registry, resource manager, or scene manager initialization failed.");
        return 1;
    }
    if (!CheckFileRoundTrip<archive::JsonArchive>(L"round-trip.json") ||
        !CheckFileRoundTrip<archive::BeveArchive>(L"round-trip.beve") ||
        !CheckManagerAndWriteFailure() || !CheckEmptyScene() || !CheckHierarchyDepthAndIds() ||
        !CheckResourceRuntimeVersion() || !CheckShaderBinaryPaths() || !CheckMeshArchiveValidation() ||
        !CheckResourceReferenceReading() || !CheckRejectedResourceReferenceDocuments() ||
        !CheckResourcesAndHandlesRoundTrip() || !CheckLinkedModelPartMeshOverrideAndReimportRoundTrip() ||
        !CheckSkinnedMeshRuntimeEquipmentAndTemporaryEffects() || !CheckPrefabRoundTrip())
        return 1;
    std::cout << "Scene file, persistent ID, hierarchy, resource handle, resource archive, and prefab checks passed.\n";
    return 0;
}
