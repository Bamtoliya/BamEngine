#pragma once

#include "Engine_API.h"
#include "RLEnvironment.h"
#include "Types.h"

#include <entt/entt.hpp>
#include <limits>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace Engine
{
	class Scene;
	class PhysicsSystem;
	class SaveGameManager;

	struct ENGINE_API SceneRLEnvConfig
	{
		std::string Name = "SceneNavigation-v0";
		f32 FixedDeltaTime = 0.02f;
		int32 SubStepsPerAction = 1;
		int32 MaxSteps = 300;

		// 바닥(Floor) 정적 충돌체 설정 (상면 Y = FloorCenter.y + FloorSize.y * 0.5f)
		vec3 FloorCenter = vec3(0.0f, -0.5f, 0.0f);
		vec3 FloorSize = vec3(16.0f, 1.0f, 16.0f);

		// 에이전트(Agent) 물리·스폰 설정
		f32 AgentRadius = 0.5f;
		f32 AgentMass = 1.0f;
		f32 AgentLinearDamping = 0.5f;
		f32 MaxControlForce = 25.0f;
		vec3 AgentSpawnMin = vec3(-2.0f, 0.5f, -2.0f);
		vec3 AgentSpawnMax = vec3(2.0f, 0.5f, 2.0f);

		// 목표(Target) 스폰 및 도달 반경 설정
		vec3 TargetMin = vec3(3.0f, 0.5f, -3.0f);
		vec3 TargetMax = vec3(6.0f, 0.5f, 3.0f);
		f32 TargetReachRadius = 0.85f;

		// 선택적 정적/동적 장애물 설정
		bool SpawnObstacle = true;
		vec3 ObstaclePosition = vec3(1.5f, 0.5f, 2.5f);
		vec3 ObstacleSize = vec3(1.0f, 1.0f, 1.0f);
		bool EnableMovingObstacle = false;
		vec3 ObstaclePatrolAxis = vec3(0.0f, 0.0f, 1.0f);
		f32 ObstaclePatrolRange = 2.5f;
		f32 ObstaclePatrolSpeed = 1.5f;

		// 다중 웨이포인트 및 커리큘럼 설정 (Waypoints가 비어 있으면 기존 단일 TargetMin..TargetMax 모드 유지)
		std::vector<vec3> Waypoints{};
		f32 WaypointReward = 3.0f;
		int32 CurriculumLevel = 0;

		// 최고 보상 에피소드 SaveGame 스냅샷 자동 저장 설정
		bool AutoSaveBestEpisode = false;
		std::string BestEpisodeSlotName = "rl_best_episode";

		// 종료 경계 및 보상 파라미터
		f32 FallYThreshold = -2.0f;
		f32 ArenaHalfExtent = 12.0f;
		f32 DistanceRewardScale = 1.0f;
		f32 StepPenalty = -0.01f;
		f32 GoalReward = 10.0f;
		f32 FallPenalty = -5.0f;

		ERLActionType ActionType = ERLActionType::Continuous;
	};

	class ENGINE_API SceneRLEnvironment final : public IRLEnvironment
	{
	public:
		static constexpr int64_t ObservationDim = 10; // [pos(3), vel(3), relTarget(3), dist(1)]
		static constexpr int64_t ContinuousActionDim = 3;
		static constexpr int64_t DiscreteActionCount = 6;

		explicit SceneRLEnvironment(SceneRLEnvConfig config = {});
		virtual ~SceneRLEnvironment() override;

		SceneRLEnvironment(const SceneRLEnvironment&) = delete;
		SceneRLEnvironment& operator=(const SceneRLEnvironment&) = delete;

		virtual void Initialize() override;
		virtual RLObservation Reset(std::optional<uint64_t> seed = std::nullopt) override;
		virtual RLStepResult Step(const RLAction& action) override;
		virtual RLSpace GetObservationSpace() const override;
		virtual RLSpace GetActionSpace() const override;
		virtual void Close() override;

		virtual const std::string& GetName() const override { return m_Config.Name; }

		const SceneRLEnvConfig& GetConfig() const { return m_Config; }
		vec3 GetAgentPosition() const;
		vec3 GetAgentVelocity() const;
		vec3 GetObstaclePosition() const;
		vec3 GetTargetPosition() const { return m_TargetPosition; }
		void SetTargetPosition(const vec3& target);
		int32 GetStepCount() const { return m_StepCount; }
		Scene* GetScene() const { return m_Scene; }

		void SetCurriculumLevel(int32 level);
		int32 GetCurriculumLevel() const { return m_Config.CurriculumLevel; }
		int32 GetCurrentWaypointIndex() const { return m_CurrentWaypointIndex; }
		int32 GetTotalWaypoints() const { return m_Config.Waypoints.empty() ? 1 : static_cast<int32>(m_Config.Waypoints.size()); }
		f32 GetEpisodeReward() const { return m_EpisodeReward; }
		f32 GetBestEpisodeReward() const { return m_BestEpisodeReward; }
		bool HasBestEpisode() const { return m_HasBestEpisode; }

		void SetSnapshotManager(SaveGameManager* manager) { m_SnapshotManager = manager; }
		[[nodiscard]] bool SaveSnapshotToSlot(SaveGameManager& manager, std::string_view slotName,
			std::string_view checkpointId = "rl_snapshot") const;
		[[nodiscard]] bool LoadSnapshotFromSlot(const SaveGameManager& manager, std::string_view slotName);

	private:
		void RebuildSceneEntities(const vec3& agentSpawnPos);
		void UpdateMovingObstacle(f32 dt);
		RLObservation BuildObservation() const;
		vec3 ComputeActionForce(const RLAction& action) const;
		vec3 SampleVec3(const vec3& minVal, const vec3& maxVal);

	private:
		SceneRLEnvConfig m_Config;
		Scene* m_Scene = nullptr;
		PhysicsSystem* m_PhysicsSystem = nullptr;
		entt::entity m_AgentEntity = entt::null;
		entt::entity m_FloorEntity = entt::null;
		entt::entity m_ObstacleEntity = entt::null;

		vec3 m_TargetPosition = vec3(4.0f, 0.5f, 0.0f);
		f32 m_PreviousDistance = 0.0f;
		int32 m_StepCount = 0;
		int32 m_CurrentWaypointIndex = 0;
		f32 m_ElapsedTime = 0.0f;
		f32 m_EpisodeReward = 0.0f;
		f32 m_BestEpisodeReward = -std::numeric_limits<f32>::infinity();
		bool m_HasBestEpisode = false;
		SaveGameManager* m_SnapshotManager = nullptr;
		std::mt19937_64 m_Rng;
	};
}
