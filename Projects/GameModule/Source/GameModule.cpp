// Projects/GameModule/Source/GameModule.cpp
#include "ModuleInterface.h"
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
	}

	void OnPreHotReload() override
	{
		BAM_LOG(Info, "GameModule", "Lifecycle", "[GameModule] PreHotReload triggered.");
	}

	void OnPostHotReload() override
	{
		BAM_LOG(Info, "GameModule", "Lifecycle", "[GameModule] PostHotReload finished.");
	}
};

IMPLEMENT_MODULE(GameModule)