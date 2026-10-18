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

#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>

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
        auto meshHandle = rm.LoadResource<Engine::Mesh>(&meshDesc);
        if (!meshHandle.IsValid() || Engine::IsFailure(rm.SaveToJsonFile(meshHandle.Get(), L"test-mesh.bammesh.json")))
            return Fail("Mesh creation or JSON archive save failed.");

        ShaderDesc vsDesc{};
        vsDesc.Key = L"test-vs.bamshader.json";
        vsDesc.Path = L"test-vs.bamshader.json";
        vsDesc.shaderType = EShaderType::Vertex;
        vsDesc.entryPoint = "VSMain";
        vsDesc.spirvPath = L"Shaders/TestVS.spv";
        vsDesc.numUniformBuffers = 2;
        auto vsHandle = rm.LoadResource<Engine::Shader>(&vsDesc);
        if (!vsHandle.IsValid() || Engine::IsFailure(rm.SaveToJsonFile(vsHandle.Get(), L"test-vs.bamshader.json")))
            return Fail("Shader creation or JSON archive save failed.");

        Engine::MaterialDesc matDesc{};
        matDesc.Key = L"test-mat.bammaterial.json";
        matDesc.Path = L"test-mat.bammaterial.json";
        matDesc.VertexShaderHandle = vsHandle;
        matDesc.CullMode = Engine::ECullMode::None;
        auto matHandle = rm.LoadResource<Engine::Material>(&matDesc);
        if (!matHandle.IsValid()) return Fail("Material creation failed.");
        matHandle->SetFloat("Roughness", 0.75f);
        if (Engine::IsFailure(rm.SaveToJsonFile(matHandle.Get(), L"test-mat.bammaterial.json")))
            return Fail("Material JSON archive save failed.");

        MaterialInstanceDesc miDesc{};
        miDesc.Key = L"test-mi.bammatinst.json";
        miDesc.Path = L"test-mi.bammatinst.json";
        miDesc.BaseMaterialHandle = matHandle;
        auto miHandle = rm.LoadResource<Engine::MaterialInstance>(&miDesc);
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
        auto skelHandle = rm.LoadResource<Engine::Skeleton>(&skelDesc);
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
        auto animHandle = rm.LoadResource<Engine::Animation>(&animDesc);
        if (!animHandle.IsValid() || Engine::IsFailure(rm.SaveToJsonFile(animHandle.Get(), L"test-anim.bamanim.json")))
            return Fail("Animation creation or JSON archive save failed.");

        ModelCreateDesc modelDesc{};
        modelDesc.Key = L"test-model.bammodel.json";
        modelDesc.Path = L"test-model.bammodel.json";
        modelDesc.Meshes = { meshHandle };
        modelDesc.Materials = { matHandle };
        modelDesc.Skeleton = skelHandle;
        modelDesc.Animations = { animHandle };
        auto modelHandle = rm.LoadResource<Engine::Model>(&modelDesc);
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

        archive::JsonArchive sceneWriter;
        if (!scene->WriteArchive(sceneWriter) || !sceneWriter.SaveToFile(L"resource-scene.json"))
            return Fail("Scene with ResourceHandle components failed to save.");

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

        ModelCreateDesc reloadedModelDesc{};
        reloadedModelDesc.Key = L"test-model.bammodel.json";
        reloadedModelDesc.Path = L"test-model.bammodel.json";
        auto reloadedModel = rm.LoadResource<Engine::Model>(&reloadedModelDesc);
        const auto reloadedMeshes = reloadedModel.IsValid() ? reloadedModel->GetMeshes() : vector<ResourceHandle<Engine::Mesh>>{};
        const auto reloadedMaterials = reloadedModel.IsValid() ? reloadedModel->GetMaterials() : vector<ResourceHandle<Engine::Material>>{};
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
        return true;
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
        !CheckResourcesAndHandlesRoundTrip() || !CheckPrefabRoundTrip())
        return 1;
    std::cout << "Scene file, persistent ID, hierarchy, resource handle, resource archive, and prefab checks passed.\n";
    return 0;
}
