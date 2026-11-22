#include "Scene.h"
#include "SceneManager.h"
#include "ScenePersistenceComponent.h"
#include "AsyncResourceLoader.h"
#include "ResourceManager.h"
#include "ReflectionRegistry.h"
#include "WorldComponents.h"
#include <archive/JsonArchive.h>

#include <iostream>
#include <fstream>
#include <chrono>
#include <thread>
#include <filesystem>
#include <string>
#include <vector>
#include <algorithm>

namespace
{
	using namespace Engine;

	struct ScopeCleanup
	{
		~ScopeCleanup()
		{
			SceneManager::Destroy();
			AsyncResourceLoader::Destroy();
			if (ResourceManager::IsCreated())
				ResourceManager::Destroy();
			if (ReflectionRegistry::IsCreated())
				ReflectionRegistry::Destroy();
		}
	};

	bool Check(bool condition, const char* message)
	{
		if (!condition)
		{
			std::cerr << "[CHECK FAILED] " << message << std::endl;
			return false;
		}
		std::cout << "[CHECK PASSED] " << message << std::endl;
		return true;
	}

	bool Test_MarkUnmark()
	{
		std::cout << "\n=== [Test 1] DontDestroyOnLoad Mark & Unmark ===" << std::endl;
		Scene* scene = Scene::Create();
		if (!Check(scene != nullptr, "Scene::Create() succeeds"))
			return false;

		auto& entity = scene->CreateEntity();
		const auto handle = entity.GetEntityHandle();

		const EResult markRes = SceneManager::Get().MarkDontDestroyOnLoad(*scene, handle, "TestGroup");
		if (!Check(markRes == EResult::Success, "MarkDontDestroyOnLoad returns Success"))
		{
			Safe_Release(scene);
			return false;
		}

		const auto* comp = scene->GetRegistry().try_get<ScenePersistenceComponent>(handle);
		if (!Check(comp != nullptr, "ScenePersistenceComponent is added"))
		{
			Safe_Release(scene);
			return false;
		}
		if (!Check(comp->persistAcrossSceneLoads == true, "persistAcrossSceneLoads is true"))
		{
			Safe_Release(scene);
			return false;
		}
		if (!Check(comp->persistentGroup == "TestGroup", "persistentGroup is 'TestGroup'"))
		{
			Safe_Release(scene);
			return false;
		}

		const EResult unmarkRes = SceneManager::Get().UnmarkDontDestroyOnLoad(*scene, handle);
		if (!Check(unmarkRes == EResult::Success, "UnmarkDontDestroyOnLoad returns Success"))
		{
			Safe_Release(scene);
			return false;
		}
		if (!Check(!scene->GetRegistry().all_of<ScenePersistenceComponent>(handle), "ScenePersistenceComponent is removed"))
		{
			Safe_Release(scene);
			return false;
		}

		Safe_Release(scene);
		return true;
	}

	bool Test_PersistentTransfer()
	{
		std::cout << "\n=== [Test 2] Persistent Entity Transfer Across Scenes ===" << std::endl;
		Scene* sceneA = Scene::Create();
		Scene* sceneB = Scene::Create();
		if (!Check(sceneA != nullptr && sceneB != nullptr, "Scene creation succeeds"))
			return false;

		sceneA->SetName(L"SceneA");
		sceneB->SetName(L"SceneB");

		// Persistent Entity in Scene A
		auto& ePersistent = sceneA->CreateEntity();
		const auto hPersistent = ePersistent.GetEntityHandle();
		sceneA->GetRegistry().emplace<NameComponent>(hPersistent, L"HeroPlayer");
		auto& transP = sceneA->GetRegistry().emplace<TransformComponent>(hPersistent);
		transP.position = glm::vec3(10.5f, -20.0f, 33.25f);
		auto& tagP = sceneA->GetRegistry().emplace<TagComponent>(hPersistent);
		tagP.tags.insert(L"PlayerTag");
		tagP.tags.insert(L"PersistentTag");
		static_cast<void>(SceneManager::Get().MarkDontDestroyOnLoad(*sceneA, hPersistent, "Heroes"));

		// Ephemeral (Non-persistent) Entity in Scene A
		auto& eEphemeral = sceneA->CreateEntity();
		const auto hEphemeral = eEphemeral.GetEntityHandle();
		sceneA->GetRegistry().emplace<NameComponent>(hEphemeral, L"TransientMob");
		auto& transE = sceneA->GetRegistry().emplace<TransformComponent>(hEphemeral);
		transE.position = glm::vec3(1.0f, 2.0f, 3.0f);
		auto& tagE = sceneA->GetRegistry().emplace<TagComponent>(hEphemeral);
		tagE.tags.insert(L"EnemyTag");

		// Open Scene A as current scene
		const EResult openRes = SceneManager::Get().OpenScene(sceneA);
		if (!Check(openRes == EResult::Success, "OpenScene(sceneA) succeeds"))
			return false;
		if (!Check(SceneManager::Get().GetCurrentScene() == sceneA, "Current scene is Scene A"))
			return false;

		// Change scene to Scene B (which triggers TransferPersistentEntities internally)
		const EResult changeRes = SceneManager::Get().ChangeScene(sceneB);
		if (!Check(changeRes == EResult::Success, "ChangeScene(sceneB) succeeds"))
			return false;
		if (!Check(SceneManager::Get().GetCurrentScene() == sceneB, "Current scene is now Scene B"))
			return false;

		// Inspect Scene B
		auto& regB = sceneB->GetRegistry();
		bool foundPersistent = false;
		bool foundEphemeral = false;

		for (auto entity : regB.view<NameComponent>())
		{
			const auto& nameComp = regB.get<NameComponent>(entity);
			if (nameComp.name == L"HeroPlayer")
			{
				foundPersistent = true;
				// Verify TransformComponent::position
				if (const auto* tc = regB.try_get<TransformComponent>(entity))
				{
					const bool posMatches = (glm::distance(tc->position, glm::vec3(10.5f, -20.0f, 33.25f)) < 1e-4f);
					static_cast<void>(Check(posMatches, "Persistent entity position preserved in Scene B"));
				}
				else
				{
					static_cast<void>(Check(false, "Persistent entity has TransformComponent in Scene B"));
				}

				// Verify TagComponent
				if (const auto* tag = regB.try_get<TagComponent>(entity))
				{
					const bool hasPlayerTag = (tag->tags.find(L"PlayerTag") != tag->tags.end());
					const bool hasPersistTag = (tag->tags.find(L"PersistentTag") != tag->tags.end());
					static_cast<void>(Check(hasPlayerTag && hasPersistTag, "Persistent entity tags preserved in Scene B"));
				}
				else
				{
					static_cast<void>(Check(false, "Persistent entity has TagComponent in Scene B"));
				}

				// Verify ScenePersistenceComponent
				if (const auto* persist = regB.try_get<ScenePersistenceComponent>(entity))
				{
					static_cast<void>(Check(persist->persistAcrossSceneLoads && persist->persistentGroup == "Heroes",
						"Persistent entity ScenePersistenceComponent preserved in Scene B"));
				}
				else
				{
					static_cast<void>(Check(false, "Persistent entity has ScenePersistenceComponent in Scene B"));
				}
			}
			else if (nameComp.name == L"TransientMob")
			{
				foundEphemeral = true;
			}
		}

		if (!Check(foundPersistent, "HeroPlayer successfully transferred to Scene B"))
			return false;
		if (!Check(!foundEphemeral, "TransientMob was NOT transferred to Scene B"))
			return false;

		Safe_Release(sceneA);
		Safe_Release(sceneB);
		return true;
	}

	bool Test_AdditiveSubscenes()
	{
		std::cout << "\n=== [Test 3] Additive Subscene Streaming ===" << std::endl;
		Scene* mainScene = Scene::Create();
		if (!Check(mainScene != nullptr, "Main scene creation succeeds"))
			return false;

		mainScene->SetName(L"MainWorld");
		static_cast<void>(SceneManager::Get().OpenScene(mainScene));

		// 1. Register in-memory additive subscene
		Scene* sub1 = Scene::Create();
		if (!Check(sub1 != nullptr, "SubScene creation succeeds"))
		{
			Safe_Release(mainScene);
			return false;
		}

		sub1->SetName(L"SubScene_Chunk1");
		const EResult regRes = SceneManager::Get().RegisterAdditiveScene("Chunk1", sub1);
		if (!Check(regRes == EResult::Success, "RegisterAdditiveScene('Chunk1') succeeds"))
		{
			Safe_Release(sub1);
			Safe_Release(mainScene);
			return false;
		}

		if (!Check(SceneManager::Get().GetAdditiveScene("Chunk1") == sub1, "GetAdditiveScene('Chunk1') returns sub1"))
			return false;

		const auto names = SceneManager::Get().GetAdditiveSceneNames();
		const bool hasChunk1 = std::find(names.begin(), names.end(), "Chunk1") != names.end();
		if (!Check(hasChunk1, "GetAdditiveSceneNames() includes 'Chunk1'"))
			return false;

		auto activeScenes = SceneManager::Get().GetActiveScenes();
		const bool mainInActive = std::find(activeScenes.begin(), activeScenes.end(), mainScene) != activeScenes.end();
		bool sub1InActive = std::find(activeScenes.begin(), activeScenes.end(), sub1) != activeScenes.end();
		if (!Check(mainInActive && sub1InActive, "GetActiveScenes() contains both mainScene and Chunk1"))
			return false;

		// Toggle active state
		sub1->SetActive(false);
		activeScenes = SceneManager::Get().GetActiveScenes();
		sub1InActive = std::find(activeScenes.begin(), activeScenes.end(), sub1) != activeScenes.end();
		if (!Check(!sub1InActive, "Deactivated Chunk1 excluded from GetActiveScenes()"))
			return false;

		sub1->SetActive(true);
		activeScenes = SceneManager::Get().GetActiveScenes();
		sub1InActive = std::find(activeScenes.begin(), activeScenes.end(), sub1) != activeScenes.end();
		if (!Check(sub1InActive, "Re-activated Chunk1 included in GetActiveScenes()"))
			return false;

		// 2. Unload additive scene
		const EResult unloadRes = SceneManager::Get().UnloadAdditiveScene("Chunk1");
		if (!Check(unloadRes == EResult::Success, "UnloadAdditiveScene('Chunk1') succeeds"))
			return false;
		if (!Check(SceneManager::Get().GetAdditiveScene("Chunk1") == nullptr, "GetAdditiveScene('Chunk1') is null after unload"))
			return false;

		activeScenes = SceneManager::Get().GetActiveScenes();
		sub1InActive = std::find(activeScenes.begin(), activeScenes.end(), sub1) != activeScenes.end();
		if (!Check(!sub1InActive, "Unloaded Chunk1 excluded from GetActiveScenes()"))
			return false;

		Safe_Release(sub1);
		Safe_Release(mainScene);
		return true;
	}

	bool Test_AsyncResourceLoader()
	{
		std::cout << "\n=== [Test 4] AsyncResourceLoader State Transitions ===" << std::endl;
		AsyncResourceLoader loader;

		const std::filesystem::path dummyDir = std::filesystem::temp_directory_path() / "BamEngine_AsyncCheck";
		std::error_code ec;
		std::filesystem::create_directories(dummyDir, ec);

		const std::filesystem::path validFile = dummyDir / "valid_test.dat";
		const std::filesystem::path cancelFile = dummyDir / "cancel_test.dat";

		{
			std::ofstream out(validFile, std::ios::binary);
			const std::string data = "BAM_ENGINE_ASYNC_DATA_1234567890";
			out.write(data.data(), static_cast<std::streamsize>(data.size()));
		}
		{
			std::ofstream out(cancelFile, std::ios::binary);
			const std::string data = "BAM_ENGINE_CANCEL_DATA";
			out.write(data.data(), static_cast<std::streamsize>(data.size()));
		}

		// 1. Successful load transition: Pending/ReadingDisk -> ReadyToFinalize -> StepMainThread -> Completed
		const auto reqSuccess = loader.RequestLoad(validFile);
		static_cast<void>(Check(reqSuccess > 0, "RequestLoad returned valid RequestId"));

		for (int i = 0; i < 100; ++i)
		{
			const auto state = loader.GetState(reqSuccess);
			if (state == EAsyncLoadState::ReadyToFinalize || state == EAsyncLoadState::Completed)
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		const auto stateBeforeFinalize = loader.GetState(reqSuccess);
		static_cast<void>(Check(stateBeforeFinalize == EAsyncLoadState::ReadyToFinalize || stateBeforeFinalize == EAsyncLoadState::Completed,
			"Worker thread transitioned request to ReadyToFinalize"));

		loader.StepMainThread(10.0);
		const auto stateAfterFinalize = loader.GetState(reqSuccess);
		if (!Check(stateAfterFinalize == EAsyncLoadState::Completed, "Request transitioned to Completed after StepMainThread"))
			return false;
		const Handle h = loader.GetHandle(reqSuccess);
		if (!Check(h.IsValid(), "GetHandle returned valid handle on Completed request"))
			return false;

		// 2. Failed load transition: Non-existent file -> Failed
		const auto reqFail = loader.RequestLoad(dummyDir / "non_existent_file_98765.missing");
		for (int i = 0; i < 100; ++i)
		{
			const auto state = loader.GetState(reqFail);
			if (state == EAsyncLoadState::Failed)
				break;
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		if (!Check(loader.GetState(reqFail) == EAsyncLoadState::Failed, "Non-existent file request transitioned to Failed"))
			return false;
		if (!Check(!loader.GetHandle(reqFail).IsValid(), "Failed request has invalid handle"))
			return false;

		// 3. Cancel transition: Cancel in-flight request -> Cancelled
		const auto reqCancel = loader.RequestLoad(cancelFile);
		const bool cancelled = loader.Cancel(reqCancel);
		static_cast<void>(Check(cancelled, "Cancel() returned true for in-flight request"));
		static_cast<void>(Check(loader.GetState(reqCancel) == EAsyncLoadState::Cancelled, "State immediately reflects Cancelled"));

		loader.StepMainThread(10.0);
		static_cast<void>(Check(loader.GetState(reqCancel) == EAsyncLoadState::Cancelled,
			"Cancelled request remains Cancelled after StepMainThread"));

		// 4. Pending count check
		const size_t pending = loader.GetPendingCount();
		static_cast<void>(Check(pending == 0, "GetPendingCount() returns 0 when all requests reached terminal states"));

		// Clean up temporary files
		std::filesystem::remove_all(dummyDir, ec);
		loader.Shutdown();

		return true;
	}
}

int main()
{
	ScopeCleanup cleanup;

	std::cout << "========================================" << std::endl;
	std::cout << "Starting SceneStreamingCheck" << std::endl;
	std::cout << "========================================" << std::endl;

	if (!Test_MarkUnmark())
	{
		std::cerr << "Test_MarkUnmark failed!" << std::endl;
		return 1;
	}

	if (!Test_PersistentTransfer())
	{
		std::cerr << "Test_PersistentTransfer failed!" << std::endl;
		return 1;
	}

	if (!Test_AdditiveSubscenes())
	{
		std::cerr << "Test_AdditiveSubscenes failed!" << std::endl;
		return 1;
	}

	if (!Test_AsyncResourceLoader())
	{
		std::cerr << "Test_AsyncResourceLoader failed!" << std::endl;
		return 1;
	}

	std::cout << "\n========================================" << std::endl;
	std::cout << "All SceneStreamingCheck tests passed!" << std::endl;
	std::cout << "========================================" << std::endl;

	return 0;
}
