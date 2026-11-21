#include "BaselineChallengeSystem.h"

#include "ActionTimelineComponent.h"
#include "ActionTimelineEvents.h"
#include "AttackChallengeComponent.h"
#include "CanvasComponent.h"
#include "ComponentEnable.h"
#include "FlagComponent.h"
#include "HierarchyComponent.h"
#include "IDComponent.h"
#include "InputManager.h"
#include "Logger.h"
#include "RectTransformComponent.h"
#include "RuntimeUIInput.h"
#include "SceneManager.h"
#include "UIButtonComponent.h"
#include "UIImageComponent.h"
#include "UITextComponent.h"
#include "WorldTransformComponent.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <optional>
#include <utility>

namespace
{
	using Engine::AttackChallengeComponent;

	struct ChallengeBinding
	{
		entt::entity player = entt::null;
		Engine::ActionTimelineComponent* timeline = nullptr;
		AttackChallengeComponent settings;
	};

	struct ChallengeState
	{
		entt::entity player = entt::null;
		entt::entity canvas = entt::null;
		entt::entity restartButton = entt::null;
		std::array<entt::entity, AttackChallengeComponent::MaxRequiredHits> indicators{};
		int32 requiredHits = 0;
		int32 hits = 0;
		std::optional<uint32> attackSerial;
		std::optional<uint32> consumedAttackSerial;
		bool usesGraphAttack = false;
	};

	bool IsPlayingEntity(const entt::registry& world, entt::entity id)
	{
		const auto* flag = world.try_get<Engine::FlagComponent>(id);
		return !flag || (flag->IsActiveInHierarchy() &&
			!HasFlag(flag->effectiveFlags, Engine::EEntityFlag::Paused) &&
			!HasFlag(flag->effectiveFlags, Engine::EEntityFlag::Dead));
	}

	ChallengeBinding FindChallengeBinding(Engine::Scene& scene)
	{
		ChallengeBinding binding;
		auto& world = scene.GetRegistry();
		const auto challenges = std::as_const(world).view<const AttackChallengeComponent>();
		if (challenges.empty())
		{
			// The existing sample can be opened without adding a component to its local scene file.
			if (scene.GetName() != L"BaselineVerification") return {};
			const auto* player = scene.FindEntityById(5);
			if (!player) return {};
			binding.player = player->GetEntityHandle();
			binding.settings.TargetEntity.PersistentId = 56;
			binding.timeline = Engine::TryGetEnabledComponent<Engine::ActionTimelineComponent>(world, binding.player);
			if (!binding.timeline || binding.timeline->timelinePath !=
				"Project://Data/Animation/Timeline/BaselineVerification_Hit.bamtimeline.json") return {};
			return IsPlayingEntity(world, binding.player) ? binding : ChallengeBinding{};
		}

		for (const auto player : challenges)
		{
			if (!Engine::IsComponentEnabled<AttackChallengeComponent>(world, player) || !IsPlayingEntity(world, player)) continue;
			auto* timeline = Engine::TryGetEnabledComponent<Engine::ActionTimelineComponent>(world, player);
			const auto* playerId = world.try_get<Engine::IDComponent>(player);
			if (!timeline || !playerId) continue;
			const auto& settings = challenges.get<AttackChallengeComponent>(player);
			if (settings.TargetEntity.PersistentId == 0 || settings.TargetEntity.PersistentId == playerId->id || settings.RequiredHits < 1 ||
				settings.RequiredHits > AttackChallengeComponent::MaxRequiredHits ||
				!std::isfinite(settings.HitRadius) || settings.HitRadius < 0.0f ||
				!std::isfinite(settings.HitHeight) || settings.HitHeight < 0.0f) continue;
			// One player owns the global Gameplay input and the HUD for this scene.
			if (binding.timeline) return {};
			binding = { player, timeline, settings };
		}
		return binding;
	}

	void UpdateIndicators(entt::registry& world, const ChallengeState& state)
	{
		for (int32 index = 0; index < state.requiredHits; ++index)
		{
			auto* image = world.try_get<Engine::UIImageComponent>(state.indicators[index]);
			if (!image) continue;
			if (state.hits == state.requiredHits)
				image->color = glm::vec4(0.20f, 0.90f, 0.44f, 1.0f);
			else if (index < state.hits)
				image->color = glm::vec4(1.0f, 0.62f, 0.15f, 1.0f);
			else
				image->color = glm::vec4(0.25f, 0.28f, 0.33f, 0.9f);
		}
	}

	Engine::RectTransformComponent& CreateHUDImage(Engine::Scene& scene, entt::entity parent, entt::entity& id)
	{
		auto& world = scene.GetRegistry();
		id = scene.CreateEntity().GetEntityHandle();
		world.emplace<Engine::FlagComponent>(id);
		world.emplace<Engine::HierarchyComponent>(id).parent = parent;
		world.get<Engine::HierarchyComponent>(parent).children.push_back(id);
		world.emplace<Engine::UIImageComponent>(id).raycastTarget = false;
		return world.emplace<Engine::RectTransformComponent>(id);
	}

	ChallengeState& CreateChallengeState(Engine::Scene& scene, const ChallengeBinding& binding)
	{
		auto& world = scene.GetRegistry();
		// The challenge is reset when PIE restores the scene archive into a new registry.
		auto& state = world.ctx().emplace<ChallengeState>();
		state.player = binding.player;
		state.requiredHits = binding.settings.RequiredHits;
		state.usesGraphAttack = binding.settings.UseGraphAttackInput;
		state.canvas = scene.CreateEntity().GetEntityHandle();
		world.emplace<Engine::FlagComponent>(state.canvas);
		world.emplace<Engine::CanvasComponent>(state.canvas);
		world.emplace<Engine::HierarchyComponent>(state.canvas);

		const f32 stride = glm::min(58.0f, 300.0f / static_cast<f32>(state.requiredHits));
		const f32 center = static_cast<f32>(state.requiredHits - 1) * 0.5f;
		for (int32 index = 0; index < state.requiredHits; ++index)
		{
			auto& rect = CreateHUDImage(scene, state.canvas, state.indicators[index]);
			rect.anchorMin = rect.anchorMax = glm::vec2(0.5f, 1.0f);
			rect.anchoredPosition = glm::vec2((static_cast<f32>(index) - center) * stride, -52.0f);
			rect.sizeDelta = glm::vec2(44.0f / 58.0f * stride, 16.0f);
		}
		auto& restartRect = CreateHUDImage(scene, state.canvas, state.restartButton);
		restartRect.anchorMin = restartRect.anchorMax = glm::vec2(0.5f, 1.0f);
		restartRect.anchoredPosition = glm::vec2(center * stride + 22.0f / 58.0f * stride + 72.0f, -52.0f);
		restartRect.sizeDelta = glm::vec2(108.0f, 44.0f);
		auto& restartImage = world.get<Engine::UIImageComponent>(state.restartButton);
		restartImage.raycastTarget = true;
		restartImage.color = glm::vec4(0.26f, 0.39f, 0.53f, 1.0f);
		world.emplace<Engine::UIButtonComponent>(state.restartButton);

		auto& label = world.emplace<Engine::UITextComponent>(state.restartButton);
		label.localizationKey = "Game.BaselineChallenge.Restart";

		UpdateIndicators(world, state);
		return state;
	}

	void RemoveChallengeState(Engine::Scene& scene, const ChallengeState& state)
	{
		auto& world = scene.GetRegistry();
		auto* timeline = world.try_get<Engine::ActionTimelineComponent>(state.player);
		if (timeline && state.attackSerial == timeline->playSerial)
			timeline->Stop();
		if (auto* canvas = scene.FindEntity(state.canvas); canvas && !scene.DestroyEntity(*canvas))
			BAM_LOG(Warn, "GameModule", "BaselineChallenge", "Could not remove the challenge HUD.");
		world.ctx().erase<ChallengeState>();
	}

	bool IsTargetInRange(const Engine::Scene& scene, const ChallengeBinding& binding)
	{
		const auto* target = scene.FindEntityById(binding.settings.TargetEntity.PersistentId);
		if (!target) return false;
		const auto targetHandle = target->GetEntityHandle();
		const auto& world = scene.GetRegistry();
		const auto* playerTransform = Engine::TryGetEnabledComponent<Engine::WorldTransformComponent>(world, binding.player);
		const auto* targetTransform = Engine::TryGetEnabledComponent<Engine::WorldTransformComponent>(world, targetHandle);
		if (!playerTransform || !targetTransform || !IsPlayingEntity(world, targetHandle)) return false;

		const glm::vec3 offset(targetTransform->worldMatrix[3] - playerTransform->worldMatrix[3]);
		return std::hypot(offset.x, offset.z) <= binding.settings.HitRadius && std::abs(offset.y) <= binding.settings.HitHeight;
	}
}

void BaselineChallengeSystem::OnUpdate(entt::registry&, const std::vector<Engine::Scene*>& activeScenes, f32)
{
	const auto& input = Engine::InputManager::Get();
	for (auto* scene : activeScenes)
	{
		if (!scene || !scene->IsActive() || scene->IsPaused()) continue;

		const auto binding = FindChallengeBinding(*scene);
		auto& world = scene->GetRegistry();
		auto* state = world.ctx().find<ChallengeState>();
		if (state && (!binding.timeline || state->player != binding.player ||
			state->requiredHits != binding.settings.RequiredHits || state->usesGraphAttack != binding.settings.UseGraphAttackInput ||
			(!world.valid(state->canvas) || !world.valid(state->restartButton))))
		{
			RemoveChallengeState(*scene, *state);
			state = nullptr;
		}
		if (!binding.timeline) continue;
		if (!state) state = &CreateChallengeState(*scene, binding);

		auto* timeline = binding.timeline;
		const auto events = Engine::GetRuntimeUIPointerEvents(*scene);
		const bool restartClicked = std::ranges::any_of(events, [&](const auto& event)
		{
			return event.type == Engine::EUIPointerEventType::Click && event.target == state->restartButton;
		});
		if (input.WasActionPressed("Gameplay", "Restart") || restartClicked)
		{
			if (state->attackSerial == timeline->playSerial)
				timeline->Stop();
			state->hits = 0;
			state->attackSerial.reset();
			state->consumedAttackSerial = timeline->playSerial;
			UpdateIndicators(world, *state);
		}
		else if (!state->usesGraphAttack && state->hits < state->requiredHits && !timeline->isPlaying &&
			timeline->timelinePath == timeline->loadedTimelinePath && timeline->timelineAsset.Get() &&
			input.WasActionPressed("Gameplay", "Attack"))
		{
			timeline->Play();
			state->attackSerial = timeline->playSerial;
		}
	}
}

void BaselineChallengeSystem::OnActionTimelineEvent(const Engine::ActionTimelineEvent& event)
{
	if (!event.scene || !event.scene->IsActive() || event.scene->IsPaused() || event.eventTag != "OnAttackHit") return;
	auto& world = event.scene->GetRegistry();
	auto* state = world.ctx().find<ChallengeState>();
	if (!state || state->hits >= state->requiredHits || event.entity != state->player) return;

	const auto binding = FindChallengeBinding(*event.scene);
	if (!binding.timeline || binding.player != state->player || state->requiredHits != binding.settings.RequiredHits ||
		state->usesGraphAttack != binding.settings.UseGraphAttackInput) return;
	const auto serial = event.playSerial;
	if (binding.timeline->playSerial != serial) return;
	if (state->consumedAttackSerial == serial || (!state->usesGraphAttack && state->attackSerial != serial)) return;
	state->consumedAttackSerial = serial;
	if (!IsTargetInRange(*event.scene, binding)) return;

	++state->hits;
	UpdateIndicators(world, *state);
	BAM_LOG(Info, "GameModule", "BaselineChallenge", "Hits: {}/{}", state->hits, state->requiredHits);
}

void BaselineChallengeSystem::ClearSceneState()
{
	if (!Engine::SceneManager::IsCreated()) return;
	for (auto* scene : Engine::SceneManager::Get().GetAllScenes())
	{
		if (auto* state = scene->GetRegistry().ctx().find<ChallengeState>())
			RemoveChallengeState(*scene, *state);
	}
}
