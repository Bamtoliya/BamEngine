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
#include "ActionTimelineEvents.h"
#include "ActionTimelineComponent.h"
#include "AnimatorComponent.h"
#include "CharacterControllerComponent.h"
#include "CompositeVFXComponent.h"
#include "FlagComponent.h"
#include "ResourceManager.h"
#include "TransformSystem.h"
#include "WorldTransformComponent.h"
#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include "GameModule.gen.h"

class BaselineJumpVFXSystem final : public Engine::ISystem
{
public:
	static BaselineJumpVFXSystem* Create() { return new BaselineJumpVFXSystem(); }

	void OnUpdate(entt::registry&, const std::vector<Engine::Scene*>& activeScenes, f32 dt) override
	{
		if (!std::isfinite(dt) || dt <= 0.0f) return;
		constexpr f32 fullTurn = 6.28318530718f;
		m_VariationX = std::fmod(m_VariationX + dt * 0.19f, fullTurn);
		m_VariationY = std::fmod(m_VariationY + dt * 0.13f, fullTurn);
		std::unordered_map<Engine::Scene*, std::unordered_set<entt::entity>> jumping;
		for (auto* scene : activeScenes)
		{
			if (!scene || !scene->IsActive()) continue;
			if (scene->IsPaused())
			{
				const auto previous = m_Jumping.find(scene);
				if (previous != m_Jumping.end()) jumping.emplace(scene, previous->second);
				continue;
			}

			auto& world = scene->GetRegistry();
			std::vector<vec3> effectPositions;
			auto view = world.view<Engine::AnimatorComponent, Engine::CharacterControllerComponent, Engine::WorldTransformComponent>();
			for (auto entity : view)
			{
				auto& animator = view.get<Engine::AnimatorComponent>(entity);
				if (animator.controllerPath != "Project://Data/Animation/BaselineVerification_Simple.animator" ||
					!animator.isPlaying || animator.timelineOverrideOwner != entt::null)
					continue;
				const auto* flag = world.try_get<Engine::FlagComponent>(entity);
				if (flag && (!flag->IsActiveInHierarchy() || HasFlag(flag->effectiveFlags, Engine::EEntityFlag::Paused) ||
					HasFlag(flag->effectiveFlags, Engine::EEntityFlag::Dead)))
					continue;

				animator.SetFloat("VariationX", 0.5f + 0.45f * std::sin(m_VariationX));
				animator.SetFloat("VariationY", 0.5f + 0.45f * std::sin(m_VariationY));
				const auto& runtime = animator.runtimeState;
				const bool inJump = runtime.GetCurrentStateName() == "Jump" || runtime.GetTransitionTargetStateName() == "Jump";
				if (!inJump) continue;

				jumping[scene].insert(entity);
				const auto previous = m_Jumping.find(scene);
				if (previous == m_Jumping.end() || !previous->second.contains(entity))
					effectPositions.emplace_back(view.get<Engine::WorldTransformComponent>(entity).worldMatrix[3]);
			}
			for (const auto& position : effectPositions)
				SpawnEffect(*scene, position);
		}
		m_Jumping = std::move(jumping);
	}

private:
	static void SpawnEffect(Engine::Scene& scene, const vec3& position)
	{
		const auto handle = Engine::ResourceManager::Get().LoadFile(L"Project://Effect/BaselineVerification_Hit.bamvfx.json");
		if (!handle.IsValid())
		{
			BAM_LOG(Warn, "GameModule", "BaselineJump", "Could not load the jump effect.");
			return;
		}

		auto& world = scene.GetRegistry();
		auto& entity = scene.CreateEntity();
		const auto id = entity.GetEntityHandle();
		auto& transform = world.emplace<Engine::TransformComponent>(id);
		auto& worldTransform = world.emplace<Engine::WorldTransformComponent>(id);
		if (!transform.SetLocalPosition(position) || !Engine::TransformSystem::UpdateRootWorld(transform, worldTransform))
		{
			BAM_LOG(Warn, "GameModule", "BaselineJump", "Could not position the jump effect.");
			if (!scene.DestroyEntity(entity))
				BAM_LOG(Error, "GameModule", "BaselineJump", "Could not remove the invalid jump effect entity.");
			return;
		}
		world.emplace<Engine::FlagComponent>(id);
		auto& vfx = world.emplace<Engine::CompositeVFXComponent>(id);
		vfx.effectHandle = Engine::ResourceHandle<Engine::CompositeEffect>(handle);
		vfx.playOnce = true;
		vfx.Restart();
		world.emplace<Engine::ActionVFXLifecycleComponent>(id);
	}

	std::unordered_map<Engine::Scene*, std::unordered_set<entt::entity>> m_Jumping;
	f32 m_VariationX = 0.0f;
	f32 m_VariationY = 0.0f;
};

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
		Engine::SystemManager::Get().AddSystem<BaselineJumpVFXSystem>();

		// 3. 액션 타임라인 이벤트 노티파이 구독 (OnUnload에서 해제하므로 핫리로드 후에도 구독은 하나다)
		m_ActionEventHandle = Engine::ActionTimelineEvents::Subscribe([](const Engine::ActionTimelineEvent& event) { OnActionTimelineEvent(event); });
		BAM_LOG(Info, "GameModule", "ActionEvent", "[GameModule] Subscribed to action timeline events (subscribers: {})", Engine::ActionTimelineEvents::GetSubscriberCount());
	}

	void OnUnload() override
	{
		// 언로드되는 코드를 가리키는 핸들러가 남지 않도록 가장 먼저 해제한다.
		if (m_ActionEventHandle != 0)
		{
			Engine::ActionTimelineEvents::Unsubscribe(m_ActionEventHandle);
			m_ActionEventHandle = 0;
		}

		BAM_LOG(Info, "GameModule", "Lifecycle", "[GameModule] Unregistering Systems...");
		if (Engine::SystemManager::IsCreated())
		{
			Engine::SystemManager::Get().RemoveSystem<BaselineJumpVFXSystem>();
			Engine::SystemManager::Get().RemoveSystem<SkyAtmosphereSystem>();
		}
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
	// 이벤트 노티파이 수신 예제: 타임라인이 지정 시점을 지날 때 한 번 호출된다. 게임 규칙(공격 판정 등)은 EventTag로 나눠 연결한다.
	static void OnActionTimelineEvent(const Engine::ActionTimelineEvent& event)
	{
		BAM_LOG(Info, "GameModule", "ActionEvent", "Action event '{}' (notify '{}', {:.2f}s) on entity {}",
			event.eventTag, event.notifyName, event.triggerTime, entt::to_integral(event.entity));
	}

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
	Engine::DelegateHandle m_ActionEventHandle = 0;
};

IMPLEMENT_MODULE(GameModule)
