#include "SaveGameManager.h"
#include "SaveGameTypes.h"
#include "EntityPool.h"
#include "Scene.h"
#include "UIProjection.h"
#include "UITween.h"
#include "InputManager.h"
#include "InputActionSettings.h"
#include "InputMap.h"
#include "SceneTransitionManager.h"
#include "ActionCombatHelpers.h"
#include "Platformer2DHelpers.h"
#include "NavGrid.h"

#include <array>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

namespace
{
	using namespace Engine;

	struct ReleaseObject
	{
		void operator()(Base* object) const
		{
			if (object)
				object->Release();
		}
	};

	bool ApproxEqual(f32 a, f32 b, f32 eps = 0.001f)
	{
		return std::fabs(a - b) <= eps;
	}

	bool Expect(bool condition, const char* message)
	{
		if (!condition)
		{
			std::cerr << "[FAIL] " << message << '\n';
			return false;
		}
		return true;
	}

	bool CheckSaveGameSchemaMigration()
	{
		SaveGameManager manager;
		manager.ClearMigrations();

		// 1. 구버전(0) -> CurrentSchemaVersion(1) 자동 폴백
		SaveGameData legacyData{};
		legacyData.metadata.schemaVersion = 0;
		if (!Expect(manager.MigrateSaveData(legacyData, SaveGameData::CurrentSchemaVersion), "Legacy schema 0 -> 1 fallback failed"))
			return false;
		if (!Expect(legacyData.metadata.schemaVersion == SaveGameData::CurrentSchemaVersion, "Legacy schema version not updated"))
			return false;

		// 2. 단계적 BFS 마이그레이션 (1 -> 2 -> 3)
		manager.RegisterMigration(1, 2, [](SaveGameData& d) {
			d.intFlags["migrated_v2"] = 200;
			return true;
		});
		manager.RegisterMigration(2, 3, [](SaveGameData& d) {
			d.intFlags["migrated_v3"] = d.intFlags["migrated_v2"] + 100;
			return true;
		});

		SaveGameData multiStep{};
		multiStep.metadata.schemaVersion = 1;
		if (!Expect(manager.MigrateSaveData(multiStep, 3), "Multi-step migration 1 -> 3 failed"))
			return false;
		if (!Expect(multiStep.metadata.schemaVersion == 3 && multiStep.intFlags["migrated_v3"] == 300, "Multi-step migrated values mismatch"))
			return false;

		// 3. 마이그레이션 실패 시 원본 보존 롤백
		manager.RegisterMigration(3, 4, [](SaveGameData& d) {
			d.intFlags["corrupted"] = 999;
			return false;
		});
		if (!Expect(!manager.MigrateSaveData(multiStep, 4), "Failing migration should return false"))
			return false;
		if (!Expect(multiStep.metadata.schemaVersion == 3 && !multiStep.intFlags.contains("corrupted"), "Failed migration must rollback data"))
			return false;

		manager.ClearMigrations();
		return true;
	}

	bool CheckEntityPoolLifecycle()
	{
		std::unique_ptr<Scene, ReleaseObject> scene(Scene::Create());
		if (!Expect(scene != nullptr, "Scene::Create failed"))
			return false;

		int acquireCalls = 0;
		int releaseCalls = 0;
		EntityPool pool(scene.get(), nullptr,
			[&](Entity&) { ++acquireCalls; },
			[&](Entity&) { ++releaseCalls; });

		if (!Expect(pool.IsSceneValid(), "Pool scene should be valid"))
			return false;
		if (!Expect(pool.Prewarm(3) == 3, "Prewarm(3) should create 3 entities"))
			return false;
		if (!Expect(pool.GetAvailableCount() == 3 && pool.GetActiveCount() == 0, "Prewarm counts mismatch"))
			return false;

		Entity* e1 = pool.Acquire();
		Entity* e2 = pool.Acquire();
		if (!Expect(e1 != nullptr && e2 != nullptr && e1 != e2, "Acquire should return distinct entities"))
			return false;
		if (!Expect(pool.GetActiveCount() == 2 && pool.GetAvailableCount() == 1 && acquireCalls == 2, "Acquire counts mismatch"))
			return false;

		// 정상 반납 및 중복 반납 방어
		if (!Expect(pool.Release(e1), "First Release(e1) should succeed"))
			return false;
		if (!Expect(!pool.Release(e1), "Duplicate Release(e1) should fail"))
			return false;
		if (!Expect(pool.GetActiveCount() == 1 && pool.GetAvailableCount() == 2 && releaseCalls == 1, "Release counts mismatch"))
			return false;

		// 외부 파괴 후 PruneInvalid 방어
		scene->DestroyEntity(*e2);
		const std::size_t pruned = pool.PruneInvalid();
		if (!Expect(pruned == 1 && pool.GetActiveCount() == 0, "PruneInvalid should remove externally destroyed entity"))
			return false;

		// 씬 파괴 이후 dangling 접근 방어
		scene.reset();
		if (!Expect(!pool.IsSceneValid(), "Pool should detect destroyed scene"))
			return false;
		if (!Expect(pool.Acquire() == nullptr, "Acquire after scene destruction must return nullptr"))
			return false;

		return true;
	}

	bool CheckUIProjectionAndTween()
	{
		const glm::mat4 view = glm::lookAtLH(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		const glm::mat4 proj = glm::perspectiveLH_ZO(glm::radians(90.0f), 16.0f / 9.0f, 0.1f, 100.0f);
		const glm::vec2 viewport(1280.0f, 720.0f);

		const auto front = UIProjection::WorldToScreen(glm::vec3(0.0f, 0.0f, 5.0f), view, proj, viewport);
		if (!Expect(front.isInFront && front.isOnScreen, "Front center point should be in front and on screen"))
			return false;
		if (!Expect(ApproxEqual(front.screenPosition.x, 640.0f, 0.5f) && ApproxEqual(front.screenPosition.y, 360.0f, 0.5f),
			"Front center point should project to viewport center"))
			return false;

		const auto behind = UIProjection::WorldToScreen(glm::vec3(0.0f, 0.0f, -5.0f), view, proj, viewport);
		if (!Expect(!behind.isInFront, "Point behind camera must report isInFront == false"))
			return false;

		const glm::vec2 canvasPos = UIProjection::ScreenToCanvas(glm::vec2(640.0f, 360.0f), viewport, glm::vec2(1920.0f, 1080.0f));
		if (!Expect(ApproxEqual(canvasPos.x, 960.0f, 0.5f) && ApproxEqual(canvasPos.y, 540.0f, 0.5f), "ScreenToCanvas scaling mismatch"))
			return false;

		if (!Expect(ApproxEqual(UITweenMath::EvaluateEase(EUITweenEase::Linear, 0.5f), 0.5f), "Linear ease mismatch"))
			return false;
		if (!Expect(ApproxEqual(UITweenMath::EvaluateEase(EUITweenEase::EaseOutQuad, 1.0f), 1.0f), "EaseOutQuad(1) mismatch"))
			return false;
		if (!Expect(ApproxEqual(UITweenMath::EvaluatePunch(0.0f), 0.0f) &&
			ApproxEqual(UITweenMath::EvaluatePunch(0.5f), 1.0f) &&
			ApproxEqual(UITweenMath::EvaluatePunch(1.0f), 0.0f), "EvaluatePunch curve mismatch"))
			return false;

		return true;
	}

	bool CheckInputRebindAndDeadzone()
	{
		if (!Expect(StringToEngineGamepadButton(EngineGamepadButtonToString(EGamepadButton::South)) == EGamepadButton::South,
			"Gamepad button string round-trip failed"))
			return false;

		const bool createdHere = !InputManager::IsCreated();
		if (createdHere && !InputManager::Create())
			return Expect(false, "InputManager::Create failed");

		auto& input = InputManager::Get();
		input.SetGamepadDeadzone(1.5f, -0.5f);
		if (!Expect(input.GetGamepadStickDeadzone() <= 0.99f && input.GetGamepadTriggerDeadzone() >= 0.0f,
			"Gamepad deadzone clamping failed"))
		{
			if (createdHere) InputManager::Destroy();
			return false;
		}

		InputActionSettings defaults{};
		InputActionMap mapSettings{};
		mapSettings.Id = "TestGameplay";
		InputAction jumpAction{};
		jumpAction.Id = "Jump";
		mapSettings.Actions.push_back(jumpAction);
		InputBinding jumpBinding{};
		jumpBinding.ActionId = "Jump";
		jumpBinding.Key = "Space";
		mapSettings.Bindings.push_back(jumpBinding);
		defaults.Maps.push_back(mapSettings);
		input.SetInputActionSettings(defaults);

		if (!Expect(input.RebindAction("TestGameplay", "Jump", "Gamepad:South"), "RebindAction failed"))
		{
			if (createdHere) InputManager::Destroy();
			return false;
		}

		const auto tempFile = std::filesystem::temp_directory_path() / "BamEngine_GameplayCommonCheck_Bindings.json";
		std::error_code ec;
		std::filesystem::remove(tempFile, ec);

		bool ok = Expect(input.SaveBindingsToFile(tempFile), "SaveBindingsToFile failed");
		if (ok)
		{
			input.ResetAllBindingsToDefault();
			const auto resetKeys = input.GetActionBindings("TestGameplay", "Jump");
			ok = Expect(resetKeys.size() == 1 && resetKeys[0] == "Space", "ResetAllBindingsToDefault failed");
		}
		if (ok)
		{
			ok = Expect(input.LoadBindingsFromFile(tempFile), "LoadBindingsFromFile failed");
			const auto loadedKeys = input.GetActionBindings("TestGameplay", "Jump");
			ok = ok && Expect(loadedKeys.size() == 1 && loadedKeys[0] == "Gamepad:South", "Loaded binding mismatch");
		}

		std::filesystem::remove(tempFile, ec);
		if (createdHere)
			InputManager::Destroy();
		return ok;
	}

	bool CheckSceneTransitionStateMachine()
	{
		const bool createdHere = !SceneTransitionManager::IsCreated();
		if (createdHere && !SceneTransitionManager::Create())
			return Expect(false, "SceneTransitionManager::Create failed");

		auto& transition = SceneTransitionManager::Get();
		bool ok = Expect(transition.TransitionToScene({}, 0.5f, 0.5f) == EResult::InvalidArgument, "Empty path must return InvalidArgument");

		if (ok)
		{
			ok = Expect(transition.TransitionToScene("NonExistent_Check.bamscene.json", 0.5f, 0.5f) == EResult::Success,
				"TransitionToScene start failed");
			ok = ok && Expect(transition.GetState() == ESceneTransitionState::FadingOut && transition.IsTransitioning(),
				"State should be FadingOut");
			ok = ok && Expect(transition.TransitionToScene("Duplicate.bamscene.json", 0.5f, 0.5f) == EResult::Fail,
				"Duplicate transition must be rejected");
		}

		if (ok)
		{
			transition.Update(0.25f);
			ok = Expect(ApproxEqual(transition.GetFadeAlpha(), 0.5f, 0.01f), "FadeOut alpha at midpoint should be 0.5");
		}

		if (ok)
		{
			// FadeOut 완료 -> SceneManager 미생성/로드 실패 시 안전하게 Idle + alpha 0.0 복구
			transition.Update(0.30f);
			ok = Expect(transition.GetState() == ESceneTransitionState::Idle && ApproxEqual(transition.GetFadeAlpha(), 0.0f),
				"Failed load should safely reset to Idle with alpha 0");
		}

		if (createdHere)
			SceneTransitionManager::Destroy();
		return ok;
	}

	bool CheckActionCombatHelpers()
	{
		// 1. HitstopState: 중첩 시 최장 시간, 최소 배율, 만료 복귀
		HitstopState hitstop;
		hitstop.Trigger(0.2f, 0.2f);
		if (!Expect(hitstop.IsActive() && ApproxEqual(hitstop.GetEffectiveTimeScale(), 0.2f), "Hitstop initial trigger mismatch"))
			return false;

		hitstop.Trigger(0.5f, 0.0f);
		if (!Expect(hitstop.IsActive() && ApproxEqual(hitstop.GetEffectiveTimeScale(), 0.0f), "Hitstop overlap min timeScale mismatch"))
			return false;

		hitstop.Update(0.25f);
		if (!Expect(hitstop.IsActive(), "Hitstop should remain active for longest duration"))
			return false;

		hitstop.Update(0.30f);
		if (!Expect(!hitstop.IsActive() && ApproxEqual(hitstop.GetEffectiveTimeScale(), 1.0f), "Hitstop should expire and restore scale 1.0"))
			return false;

		// 2. CameraShakeState: 발동, 감쇠, 종료
		CameraShakeState shake;
		shake.Trigger(1.0f, 0.4f, 25.0f);
		if (!Expect(shake.IsActive(), "CameraShake should be active after Trigger"))
			return false;

		shake.Update(0.05f);
		if (!Expect(glm::length(shake.GetPositionOffset()) > 0.0f || glm::length(shake.GetRotationOffsetDeg()) > 0.0f,
			"CameraShake should produce non-zero offset while active"))
			return false;

		shake.Update(0.5f);
		if (!Expect(!shake.IsActive() &&
			ApproxEqual(glm::length(shake.GetPositionOffset()), 0.0f) &&
			ApproxEqual(glm::length(shake.GetRotationOffsetDeg()), 0.0f),
			"CameraShake should stop and zero offsets after duration"))
			return false;

		// 3. TargetLockOnSelector: FindBestTarget, SwitchTarget 좌/우, IsTargetValid
		const std::array<TargetCandidate, 3> candidates{ {
			{ 1, vec3(0.0f, 0.0f, 5.0f) },
			{ 2, vec3(3.0f, 0.0f, 5.0f) },
			{ 3, vec3(-3.0f, 0.0f, 5.0f) }
		} };
		const vec3 cameraPos(0.0f, 0.0f, 0.0f);
		const vec3 cameraForward(0.0f, 0.0f, 1.0f);
		const vec3 cameraRight(1.0f, 0.0f, 0.0f);

		const auto best = TargetLockOnSelector::FindBestTarget(candidates, cameraPos, cameraForward, 20.0f, 90.0f);
		if (!Expect(best.has_value() && *best == 1, "FindBestTarget should select center target 1"))
			return false;

		const auto rightTarget = TargetLockOnSelector::SwitchTarget(candidates, cameraPos, cameraForward, cameraRight, 20.0f, 90.0f, 1, true);
		if (!Expect(rightTarget.has_value() && *rightTarget == 2, "SwitchTarget(right) should select target 2"))
			return false;

		const auto leftTarget = TargetLockOnSelector::SwitchTarget(candidates, cameraPos, cameraForward, cameraRight, 20.0f, 90.0f, 1, false);
		if (!Expect(leftTarget.has_value() && *leftTarget == 3, "SwitchTarget(left) should select target 3"))
			return false;

		if (!Expect(TargetLockOnSelector::IsTargetValid(vec3(0.0f, 0.0f, 5.0f), cameraPos, 20.0f), "Target within maxDistance should be valid"))
			return false;
		if (!Expect(!TargetLockOnSelector::IsTargetValid(vec3(0.0f, 0.0f, 25.0f), cameraPos, 20.0f), "Target beyond maxDistance should be invalid"))
			return false;

		return true;
	}

	bool CheckPlatformer2DHelpers()
	{
		// 1. PlatformerJumpController: 코요테 타임 내 점프 소비 & 점프 버퍼링 후 착지 소비
		PlatformerJumpController jump(0.12f, 0.12f);
		jump.Update(true, 0.016f);
		jump.Update(false, 0.05f);
		jump.RequestJump();
		if (!Expect(jump.CanJump() && jump.ConsumeJump() && !jump.ConsumeJump(), "Coyote time jump consume failed"))
			return false;

		jump.Update(false, 0.20f);
		jump.RequestJump();
		if (!Expect(jump.HasBufferedJump(), "Jump should be buffered in mid-air"))
			return false;

		jump.Update(true, 0.05f);
		if (!Expect(jump.ConsumeJump(), "Buffered jump should be consumable upon landing"))
			return false;

		// 2. OneWayPlatform2D::ShouldCollide: 하강 착지 true, 상승/하향 드롭 false
		if (!Expect(OneWayPlatform2D::ShouldCollide(1.1f, 0.95f, -2.0f, 1.0f, false), "Falling onto one-way platform should collide"))
			return false;
		if (!Expect(!OneWayPlatform2D::ShouldCollide(0.9f, 1.1f, 2.0f, 1.0f, false), "Rising through one-way platform must not collide"))
			return false;
		if (!Expect(!OneWayPlatform2D::ShouldCollide(1.1f, 0.95f, -2.0f, 1.0f, true), "Drop-through request must not collide"))
			return false;

		// 3. Camera2DRoomBounds::ClampPosition: 대형 룸 클램프, 소형 룸 중앙 정렬
		const vec2 largeClamped = Camera2DRoomBounds::ClampPosition(vec2(2.0f, 58.0f), vec2(10.0f, 5.0f), vec2(0.0f, 0.0f), vec2(100.0f, 60.0f));
		if (!Expect(ApproxEqual(largeClamped.x, 10.0f) && ApproxEqual(largeClamped.y, 55.0f), "Large room camera clamp mismatch"))
			return false;

		const vec2 smallCentered = Camera2DRoomBounds::ClampPosition(vec2(0.0f, 0.0f), vec2(10.0f, 5.0f), vec2(0.0f, 0.0f), vec2(12.0f, 8.0f));
		if (!Expect(ApproxEqual(smallCentered.x, 6.0f) && ApproxEqual(smallCentered.y, 4.0f), "Small room camera centering mismatch"))
			return false;

		return true;
	}

	bool CheckNavGridDynamicObstacles()
	{
		NavGrid grid;
		if (!Expect(grid.Initialize(10, 10, 1.0f, vec3(0.0f)), "NavGrid::Initialize failed"))
			return false;

		const int32 box1Added = grid.SetDynamicObstacleBox(vec3(1.0f, 0.0f, 1.0f), vec3(3.0f, 1.0f, 3.0f), true);
		const int32 box2Added = grid.SetDynamicObstacleBox(vec3(2.0f, 0.0f, 2.0f), vec3(4.0f, 1.0f, 4.0f), true);
		if (!Expect(box1Added > 0 && box2Added > 0, "SetDynamicObstacleBox should affect cells"))
			return false;

		if (!Expect(grid.IsDynamicBlocked(1, 1) && !grid.IsWalkable(1, 1) &&
			grid.IsDynamicBlocked(2, 2) && !grid.IsWalkable(2, 2),
			"Overlapped and non-overlapped cells should be blocked"))
			return false;

		grid.SetDynamicObstacleBox(vec3(1.0f, 0.0f, 1.0f), vec3(3.0f, 1.0f, 3.0f), false);
		if (!Expect(!grid.IsDynamicBlocked(1, 1) && grid.IsWalkable(1, 1) &&
			grid.IsDynamicBlocked(2, 2) && !grid.IsWalkable(2, 2),
			"Removing first box should restore (1,1) while keeping overlapped (2,2) blocked"))
			return false;

		grid.SetDynamicObstacleBox(vec3(2.0f, 0.0f, 2.0f), vec3(4.0f, 1.0f, 4.0f), false);
		if (!Expect(!grid.IsDynamicBlocked(2, 2) && grid.IsWalkable(2, 2),
			"Removing second box should restore (2,2) walkability"))
			return false;

		const int32 circleAdded = grid.SetDynamicObstacleCircle(vec3(5.5f, 0.0f, 5.5f), 1.5f, true);
		if (!Expect(circleAdded > 0 && grid.IsDynamicBlocked(5, 5) && !grid.IsWalkable(5, 5),
			"SetDynamicObstacleCircle should block center cell (5,5)"))
			return false;

		grid.ClearDynamicObstacles();
		if (!Expect(!grid.IsDynamicBlocked(5, 5) && grid.IsWalkable(5, 5),
			"ClearDynamicObstacles should restore all cells"))
			return false;

		return true;
	}
}

int main()
{
	if (!CheckSaveGameSchemaMigration()) return 1;
	if (!CheckEntityPoolLifecycle()) return 2;
	if (!CheckUIProjectionAndTween()) return 3;
	if (!CheckInputRebindAndDeadzone()) return 4;
	if (!CheckSceneTransitionStateMachine()) return 5;
	if (!CheckActionCombatHelpers()) return 6;
	if (!CheckPlatformer2DHelpers()) return 7;
	if (!CheckNavGridDynamicObstacles()) return 8;

	std::cout << "[PASS] GameplayCommonCheck (SaveMigration, EntityPool, UIProjection/Tween, InputRebind/Deadzone, SceneTransition, ActionCombat, Platformer2D, NavGridDynamic)\n";
	return 0;
}
