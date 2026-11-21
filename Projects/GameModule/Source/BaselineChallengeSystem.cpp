#include "BaselineChallengeSystem.h"

#include "ActionTimelineComponent.h"
#include "ActionTimelineEvents.h"
#include "CanvasComponent.h"
#include "ComponentEnable.h"
#include "FlagComponent.h"
#include "HierarchyComponent.h"
#include "InputManager.h"
#include "Logger.h"
#include "RectTransformComponent.h"
#include "SceneManager.h"
#include "UIImageComponent.h"
#include "WorldTransformComponent.h"
#include <array>
#include <cmath>
#include <optional>

namespace
{
	// Persistent IDs from BaselineVerification.bamscene.json.
	constexpr uint64 playerId = 5;
	constexpr uint64 targetId = 56;
	constexpr std::size_t requiredHits = 3;
	constexpr f32 hitRadius = 2.5f;
	constexpr f32 hitHeight = 2.0f;
	constexpr auto timelinePath = "Project://Data/Animation/Timeline/BaselineVerification_Hit.bamtimeline.json";

	struct ChallengeState
	{
		entt::entity canvas = entt::null;
		std::array<entt::entity, requiredHits> indicators{};
		std::size_t hits = 0;
		std::optional<uint32> attackSerial;
		bool hitConsumed = false;
	};

	bool IsPlayingEntity(const entt::registry& world, entt::entity id)
	{
		const auto* flag = world.try_get<Engine::FlagComponent>(id);
		return !flag || (flag->IsActiveInHierarchy() &&
			!HasFlag(flag->effectiveFlags, Engine::EEntityFlag::Paused) &&
			!HasFlag(flag->effectiveFlags, Engine::EEntityFlag::Dead));
	}

	Engine::ActionTimelineComponent* GetPlayerTimeline(Engine::Scene& scene, entt::entity& player)
	{
		const auto* entity = scene.FindEntityById(playerId);
		if (!entity) return nullptr;
		player = entity->GetEntityHandle();
		auto& world = scene.GetRegistry();
		auto* timeline = Engine::TryGetEnabledComponent<Engine::ActionTimelineComponent>(world, player);
		return timeline && timeline->timelinePath == timelinePath && IsPlayingEntity(world, player) ? timeline : nullptr;
	}

	void UpdateIndicators(entt::registry& world, const ChallengeState& state)
	{
		for (std::size_t index = 0; index < state.indicators.size(); ++index)
		{
			auto* image = world.try_get<Engine::UIImageComponent>(state.indicators[index]);
			if (!image) continue;
			if (state.hits == requiredHits)
				image->color = glm::vec4(0.20f, 0.90f, 0.44f, 1.0f);
			else if (index < state.hits)
				image->color = glm::vec4(1.0f, 0.62f, 0.15f, 1.0f);
			else
				image->color = glm::vec4(0.25f, 0.28f, 0.33f, 0.9f);
		}
	}

	ChallengeState& CreateChallengeState(Engine::Scene& scene)
	{
		auto& world = scene.GetRegistry();
		// The challenge is reset when PIE restores the scene archive into a new registry.
		auto& state = world.ctx().emplace<ChallengeState>();
		state.canvas = scene.CreateEntity().GetEntityHandle();
		world.emplace<Engine::FlagComponent>(state.canvas);
		world.emplace<Engine::CanvasComponent>(state.canvas);
		world.emplace<Engine::HierarchyComponent>(state.canvas);

		for (std::size_t index = 0; index < state.indicators.size(); ++index)
		{
			const auto id = scene.CreateEntity().GetEntityHandle();
			state.indicators[index] = id;
			world.emplace<Engine::FlagComponent>(id);
			world.emplace<Engine::HierarchyComponent>(id).parent = state.canvas;
			world.get<Engine::HierarchyComponent>(state.canvas).children.push_back(id);
			auto& rect = world.emplace<Engine::RectTransformComponent>(id);
			rect.anchorMin = rect.anchorMax = glm::vec2(0.5f, 1.0f);
			rect.anchoredPosition = glm::vec2((static_cast<f32>(index) - 1.0f) * 58.0f, -52.0f);
			rect.sizeDelta = glm::vec2(44.0f, 16.0f);
			world.emplace<Engine::UIImageComponent>(id).raycastTarget = false;
		}
		UpdateIndicators(world, state);
		return state;
	}

	bool IsTargetInRange(Engine::Scene& scene, entt::entity player)
	{
		const auto* target = scene.FindEntityById(targetId);
		if (!target) return false;
		const auto& world = scene.GetRegistry();
		const auto targetHandle = target->GetEntityHandle();
		const auto* playerTransform = Engine::TryGetEnabledComponent<Engine::WorldTransformComponent>(world, player);
		const auto* targetTransform = Engine::TryGetEnabledComponent<Engine::WorldTransformComponent>(world, targetHandle);
		if (!playerTransform || !targetTransform || !IsPlayingEntity(world, targetHandle)) return false;

		const glm::vec3 offset(targetTransform->worldMatrix[3] - playerTransform->worldMatrix[3]);
		return offset.x * offset.x + offset.z * offset.z <= hitRadius * hitRadius && std::abs(offset.y) <= hitHeight;
	}
}

void BaselineChallengeSystem::OnUpdate(entt::registry&, const std::vector<Engine::Scene*>& activeScenes, f32)
{
	const auto& input = Engine::InputManager::Get();
	for (auto* scene : activeScenes)
	{
		if (!scene || scene->GetName() != L"BaselineVerification" || !scene->IsActive() || scene->IsPaused()) continue;

		entt::entity player = entt::null;
		auto* timeline = GetPlayerTimeline(*scene, player);
		if (!timeline) continue;

		auto& world = scene->GetRegistry();
		auto* state = world.ctx().find<ChallengeState>();
		if (!state || !world.valid(state->canvas))
		{
			world.ctx().erase<ChallengeState>();
			state = &CreateChallengeState(*scene);
		}
		if (input.WasActionPressed("Gameplay", "Restart"))
		{
			if (state->attackSerial == timeline->playSerial)
				timeline->Stop();
			state->hits = 0;
			state->attackSerial.reset();
			UpdateIndicators(world, *state);
		}
		else if (state->hits < requiredHits && !timeline->isPlaying && input.WasActionPressed("Gameplay", "Attack"))
		{
			timeline->Play();
			state->attackSerial = timeline->playSerial;
			state->hitConsumed = false;
		}
	}
}

void BaselineChallengeSystem::OnActionTimelineEvent(const Engine::ActionTimelineEvent& event)
{
	if (!event.scene || event.eventTag != "OnAttackHit") return;
	auto& world = event.scene->GetRegistry();
	auto* state = world.ctx().find<ChallengeState>();
	if (!state || state->hitConsumed || state->hits >= requiredHits) return;

	entt::entity player = entt::null;
	const auto* timeline = GetPlayerTimeline(*event.scene, player);
	if (!timeline || event.entity != player || state->attackSerial != timeline->playSerial) return;
	state->hitConsumed = true;
	if (!IsTargetInRange(*event.scene, player)) return;

	++state->hits;
	UpdateIndicators(world, *state);
	BAM_LOG(Info, "GameModule", "BaselineChallenge", "Hits: {}/{}", state->hits, requiredHits);
}

void BaselineChallengeSystem::ClearSceneState()
{
	if (!Engine::SceneManager::IsCreated()) return;
	for (auto* scene : Engine::SceneManager::Get().GetAllScenes())
	{
		auto& world = scene->GetRegistry();
		auto* state = world.ctx().find<ChallengeState>();
		if (!state) continue;
		entt::entity player = entt::null;
		if (auto* timeline = GetPlayerTimeline(*scene, player); timeline && state->attackSerial == timeline->playSerial)
			timeline->Stop();
		if (auto* canvas = scene->FindEntity(state->canvas); canvas && !scene->DestroyEntity(*canvas))
			BAM_LOG(Warn, "GameModule", "BaselineChallenge", "Could not remove the challenge HUD.");
		world.ctx().erase<ChallengeState>();
	}
}
