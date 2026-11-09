// Projects/GameModule/Source/GameModule.cpp
#include "ModuleInterface.h"
#include "ModuleManager.h"
#include "PlatformPath.h"
#include "GameplayScoreApi.h"
#include "Logger.h"
#include "SystemManager.h"
#include "SkyAtmosphereSystem.h"
#include "GameGameplayLibrary.h"
#include "ReflectionRegistry.h"
#include "GameModule.gen.h"

class GameModule final : public Engine::IModule
{
public:
	// GameModule만의 고유 식별자 ('GAME' = 0x47414D45)
	static constexpr reflection::ModuleId GameModuleId = 0x47414D45;

	void OnLoad() override
	{
		RunGameplayScoreExample();

		// 1. 활성 모듈 태그를 지정하여 리플렉션 심볼 등록
		if (Engine::ReflectionRegistry::IsCreated())
		{
			auto& registry = Engine::ReflectionRegistry::Get().GetMutableRegistry();
			reflection::Registry::ScopedActiveModule scope(registry, GameModuleId);

			reflection::RegistrationResult regResult;
			const bool ok = reflection_generated::Register_GameModule(registry, regResult);
			BAM_LOG(Info, "GameModule", "Lifecycle", "[GameModule] Register_GameModule success: {}, error: {}", ok, (int)regResult.Error);

			// [매크로 적용] 모듈 내의 모든 STRUCT(Component) 자동 일괄 등록
			REGISTER_MODULE_COMPONENTS(GameModule);
		}

		// 2. 리소스 초기화 및 시스템 등록
		SkyAtmosphereSystem::InitializeResources();
		BAM_LOG(Info, "GameModule", "Lifecycle", "[GameModule] Registering Systems...");
		Engine::SystemManager::Get().AddSystem<SkyAtmosphereSystem>();
	}

	void OnUnload() override
	{
		BAM_LOG(Info, "GameModule", "Lifecycle", "[GameModule] Unregistering Systems...");
		Engine::SystemManager::Get().RemoveSystem<SkyAtmosphereSystem>();

		// 3. 모듈 언로드 시 컴포넌트 및 리플렉션 심볼 일괄 해제
		if (Engine::ReflectionRegistry::IsCreated())
		{
			// [매크로 적용] 모듈 컴포넌트 일괄 해제
			UNREGISTER_MODULE_COMPONENTS(GameModule);

			auto& registry = Engine::ReflectionRegistry::Get().GetMutableRegistry();
			const size_t removed = registry.UnregisterModule(GameModuleId);
			BAM_LOG(Info, "GameModule", "Lifecycle", "[GameModule] Unregistered {} reflection symbols.", removed);
		}

		if (m_OwnsScoreModule && Engine::ModuleManager::IsCreated())
		{
			auto& modules = Engine::ModuleManager::Get();
			if (modules.IsModuleLoaded(GameplayScore::ModuleName))
				modules.UnloadModule(GameplayScore::ModuleName);
			m_OwnsScoreModule = false;
		}
	}

	void OnPreHotReload() override
	{
		BAM_LOG(Info, "GameModule", "Lifecycle", "[GameModule] PreHotReload triggered.");
	}

	void OnPostHotReload() override
	{
		BAM_LOG(Info, "GameModule", "Lifecycle", "[GameModule] PostHotReload finished.");
	}

private:
	void RunGameplayScoreExample()
	{
		if (!Engine::ModuleManager::IsCreated())
			return;

		auto& modules = Engine::ModuleManager::Get();
		if (!modules.IsModuleLoaded(GameplayScore::ModuleName))
		{
			const auto path = Engine::PlatformPath::GetExecutableDirectory() / GameplayScore::DllName;
			const auto result = modules.LoadModule(GameplayScore::ModuleName, path);
			if (result != Engine::EResult::Success)
			{
				BAM_LOG(Warn, "GameModule", "GameplayScore", "Optional GameplayScore example is unavailable");
				return;
			}
			m_OwnsScoreModule = true;
		}

		const auto* module = modules.GetModuleInfo(GameplayScore::ModuleName);
		const auto getApi = module && module->Handle
			? reinterpret_cast<GameplayScore::GetApiV1Fn>(GetProcAddress(module->Handle, GameplayScore::EntryPoint))
			: nullptr;
		const auto* api = getApi ? getApi() : nullptr;
		if (!api || api->Size != sizeof(GameplayScore::ApiV1) || !api->CalculateComboScore)
		{
			BAM_LOG(Warn, "GameModule", "GameplayScore", "GameplayScore V1 API is missing or incompatible");
			return;
		}

		const auto score = api->CalculateComboScore(100, 3);
		BAM_LOG(Info, "GameModule", "GameplayScore", "Base score 100, combo 3: {}", score);
	}

	bool m_OwnsScoreModule = false;
};

IMPLEMENT_MODULE(GameModule)
