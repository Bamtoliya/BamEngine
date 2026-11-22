#include "SceneRLEnvironment.h"

#include "ColliderComponent.h"
#include "PhysicsSystem.h"
#include "RigidBodyComponent.h"
#include "Scene.h"
#include "TransformComponent.h"
#include "TransformSystem.h"
#include "WorldTransformComponent.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace Engine
{
	namespace
	{
		bool IsFiniteVec3(const vec3& v)
		{
			return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
		}

		f32 SanitizeActionAxis(f32 value)
		{
			if (!std::isfinite(value)) return 0.0f;
			return std::clamp(value, -1.0f, 1.0f);
		}
	}

	SceneRLEnvironment::SceneRLEnvironment(SceneRLEnvConfig config)
		: m_Config(std::move(config))
	{
		Initialize();
	}

	SceneRLEnvironment::~SceneRLEnvironment()
	{
		Close();
	}

	void SceneRLEnvironment::Initialize()
	{
		m_Rng.seed(std::random_device{}());

		if (!m_Scene)
		{
			SceneCreateDesc desc{};
			desc.name = L"SceneRLEnvironment";
			m_Scene = Scene::Create(&desc);
			if (m_Scene)
			{
				m_Scene->SetActive(true);
				m_Scene->SetPaused(false);
			}
		}

		if (!m_PhysicsSystem)
		{
			m_PhysicsSystem = PhysicsSystem::Create();
		}

		m_StepCount = 0;
		m_TargetPosition = (m_Config.TargetMin + m_Config.TargetMax) * 0.5f;
		RebuildSceneEntities((m_Config.AgentSpawnMin + m_Config.AgentSpawnMax) * 0.5f);
		m_PreviousDistance = glm::length(m_TargetPosition - GetAgentPosition());
	}

	void SceneRLEnvironment::Close()
	{
		m_AgentEntity = entt::null;
		m_FloorEntity = entt::null;
		m_ObstacleEntity = entt::null;
		m_StepCount = 0;

		if (m_Scene)
		{
			m_Scene->ClearEntities();
			m_Scene->Release();
			m_Scene = nullptr;
		}

		if (m_PhysicsSystem)
		{
			m_PhysicsSystem->Release();
			m_PhysicsSystem = nullptr;
		}
	}

	vec3 SceneRLEnvironment::SampleVec3(const vec3& minVal, const vec3& maxVal)
	{
		auto sampleAxis = [this](f32 lo, f32 hi) -> f32
		{
			if (hi <= lo) return lo;
			std::uniform_real_distribution<f32> dist(lo, hi);
			return dist(m_Rng);
		};
		return vec3(
			sampleAxis(minVal.x, maxVal.x),
			sampleAxis(minVal.y, maxVal.y),
			sampleAxis(minVal.z, maxVal.z));
	}

	void SceneRLEnvironment::RebuildSceneEntities(const vec3& agentSpawnPos)
	{
		if (!m_Scene) return;

		m_Scene->ClearEntities();
		PhysicsSystem::ResetTriggerState3D(*m_Scene);
		m_AgentEntity = entt::null;
		m_FloorEntity = entt::null;
		m_ObstacleEntity = entt::null;

		auto& registry = m_Scene->GetRegistry();

		// 1. 바닥(Floor) 정적 엔티티
		{
			m_FloorEntity = m_Scene->CreateEntity().GetEntityHandle();
			auto& tr = registry.get_or_emplace<TransformComponent>(m_FloorEntity);
			auto& worldTr = registry.get_or_emplace<WorldTransformComponent>(m_FloorEntity);
			const bool posOk = tr.SetLocalPosition(m_Config.FloorCenter);
			if (posOk)
			{
				(void)TransformSystem::UpdateRootWorld(tr, worldTr);
			}

			auto& col = registry.emplace<ColliderComponent>(m_FloorEntity);
			BoxShape floorBox{};
			floorBox.Size = m_Config.FloorSize;
			ColliderSettings floorSettings{};
			floorSettings.Friction = 0.1f;
			floorSettings.Restitution = 0.0f;
			const bool shapeOk = col.SetShape(floorBox);
			const bool settingsOk = col.SetSettings(floorSettings);
			(void)shapeOk;
			(void)settingsOk;

			auto& body = registry.emplace<RigidBodyComponent>(m_FloorEntity);
			body.BodyType = ERigidBodyType::Static;
		}

		// 2. 선택적 정적 장애물 엔티티
		if (m_Config.SpawnObstacle)
		{
			m_ObstacleEntity = m_Scene->CreateEntity().GetEntityHandle();
			auto& tr = registry.get_or_emplace<TransformComponent>(m_ObstacleEntity);
			auto& worldTr = registry.get_or_emplace<WorldTransformComponent>(m_ObstacleEntity);
			const bool posOk = tr.SetLocalPosition(m_Config.ObstaclePosition);
			if (posOk)
			{
				(void)TransformSystem::UpdateRootWorld(tr, worldTr);
			}

			auto& col = registry.emplace<ColliderComponent>(m_ObstacleEntity);
			BoxShape obstacleBox{};
			obstacleBox.Size = m_Config.ObstacleSize;
			const bool shapeOk = col.SetShape(obstacleBox);
			(void)shapeOk;

			auto& body = registry.emplace<RigidBodyComponent>(m_ObstacleEntity);
			body.BodyType = ERigidBodyType::Static;
		}

		// 3. 에이전트(Agent) 동적 강체 엔티티
		{
			m_AgentEntity = m_Scene->CreateEntity().GetEntityHandle();
			auto& tr = registry.get_or_emplace<TransformComponent>(m_AgentEntity);
			auto& worldTr = registry.get_or_emplace<WorldTransformComponent>(m_AgentEntity);
			const bool posOk = tr.SetLocalPosition(agentSpawnPos);
			if (posOk)
			{
				(void)TransformSystem::UpdateRootWorld(tr, worldTr);
			}

			auto& col = registry.emplace<ColliderComponent>(m_AgentEntity);
			BoxShape agentBox{};
			const f32 diameter = std::max(m_Config.AgentRadius * 2.0f, 0.1f);
			agentBox.Size = vec3(diameter, diameter, diameter);
			ColliderSettings agentSettings{};
			agentSettings.Friction = 0.1f;
			agentSettings.Restitution = 0.0f;
			const bool shapeOk = col.SetShape(agentBox);
			const bool settingsOk = col.SetSettings(agentSettings);
			(void)shapeOk;
			(void)settingsOk;

			auto& body = registry.emplace<RigidBodyComponent>(m_AgentEntity);
			body.BodyType = ERigidBodyType::Dynamic;
			body.Mass = std::max(m_Config.AgentMass, 0.01f);
			body.LinearDamping = std::max(m_Config.AgentLinearDamping, 0.0f);
			body.GravityScale = 1.0f;
			body.FreezeRotation = true;
			body.LinearVelocity = vec3(0.0f);
			body.AngularVelocity = vec3(0.0f);
			body.AccumulatedForce = vec3(0.0f);
			body.AccumulatedTorque = vec3(0.0f);
		}

		(void)TransformSystem::UpdateHierarchy(registry);
	}

	RLObservation SceneRLEnvironment::Reset(std::optional<uint64_t> seed)
	{
		if (!m_Scene || !m_PhysicsSystem)
		{
			Initialize();
		}

		if (seed.has_value())
		{
			m_Rng.seed(seed.value());
		}

		const vec3 agentSpawn = SampleVec3(m_Config.AgentSpawnMin, m_Config.AgentSpawnMax);
		m_TargetPosition = SampleVec3(m_Config.TargetMin, m_Config.TargetMax);
		RebuildSceneEntities(agentSpawn);

		m_StepCount = 0;
		m_PreviousDistance = glm::length(m_TargetPosition - GetAgentPosition());
		return BuildObservation();
	}

	vec3 SceneRLEnvironment::ComputeActionForce(const RLAction& action) const
	{
		const f32 maxForce = m_Config.MaxControlForce;
		if (action.Type == ERLActionType::Continuous)
		{
			f32 fx = 0.0f;
			f32 fy = 0.0f;
			f32 fz = 0.0f;
			if (action.ContinuousActions.size() >= 3)
			{
				fx = SanitizeActionAxis(action.ContinuousActions[0]);
				fy = SanitizeActionAxis(action.ContinuousActions[1]);
				fz = SanitizeActionAxis(action.ContinuousActions[2]);
			}
			else if (action.ContinuousActions.size() == 2)
			{
				fx = SanitizeActionAxis(action.ContinuousActions[0]);
				fz = SanitizeActionAxis(action.ContinuousActions[1]);
			}
			else if (action.ContinuousActions.size() == 1)
			{
				fx = SanitizeActionAxis(action.ContinuousActions[0]);
			}
			return vec3(fx, fy, fz) * maxForce;
		}

		// Discrete: 0=+X, 1=-X, 2=+Z, 3=-Z, 4=+Y, 5=NoOp
		switch (action.DiscreteAction)
		{
		case 0: return vec3(maxForce, 0.0f, 0.0f);
		case 1: return vec3(-maxForce, 0.0f, 0.0f);
		case 2: return vec3(0.0f, 0.0f, maxForce);
		case 3: return vec3(0.0f, 0.0f, -maxForce);
		case 4: return vec3(0.0f, maxForce, 0.0f);
		default: return vec3(0.0f);
		}
	}

	RLStepResult SceneRLEnvironment::Step(const RLAction& action)
	{
		if (!m_Scene || !m_PhysicsSystem || m_AgentEntity == entt::null)
		{
			Initialize();
		}

		auto& registry = m_Scene->GetRegistry();
		const vec3 force = ComputeActionForce(action);
		const int32 subSteps = std::max(m_Config.SubStepsPerAction, 1);
		const f32 dt = std::max(m_Config.FixedDeltaTime, 0.001f);
		const std::vector<Scene*> activeScenes = { m_Scene };

		for (int32 i = 0; i < subSteps; ++i)
		{
			if (auto* body = registry.try_get<RigidBodyComponent>(m_AgentEntity))
			{
				body->AccumulatedForce += force;
			}
			m_PhysicsSystem->OnFixedUpdate(registry, activeScenes, dt);
		}

		++m_StepCount;

		const vec3 pos = GetAgentPosition();
		const vec3 vel = GetAgentVelocity();
		const bool validState = IsFiniteVec3(pos) && IsFiniteVec3(vel);
		const f32 currDistance = validState ? glm::length(m_TargetPosition - pos) : m_PreviousDistance;

		// Dense distance-shaping reward + small step penalty
		f32 reward = (m_PreviousDistance - currDistance) * m_Config.DistanceRewardScale + m_Config.StepPenalty;
		m_PreviousDistance = currDistance;

		bool terminated = false;
		const char* reason = "running";

		if (!validState)
		{
			reward = m_Config.FallPenalty;
			terminated = true;
			reason = "invalid_state";
		}
		else if (currDistance <= m_Config.TargetReachRadius)
		{
			reward += m_Config.GoalReward;
			terminated = true;
			reason = "goal";
		}
		else if (pos.y < m_Config.FallYThreshold)
		{
			reward += m_Config.FallPenalty;
			terminated = true;
			reason = "fall";
		}
		else if (std::abs(pos.x) > m_Config.ArenaHalfExtent || std::abs(pos.z) > m_Config.ArenaHalfExtent)
		{
			reward += m_Config.FallPenalty;
			terminated = true;
			reason = "out_of_bounds";
		}

		const bool truncated = (!terminated) && (m_StepCount >= m_Config.MaxSteps);
		if (truncated)
		{
			reason = "max_steps";
		}

		RLStepResult result;
		result.Observation = BuildObservation();
		result.Reward = std::isfinite(reward) ? reward : 0.0f;
		result.Terminated = terminated;
		result.Truncated = truncated;

		std::ostringstream info;
		info << "{\"steps\":" << m_StepCount
			<< ",\"distance\":" << currDistance
			<< ",\"reason\":\"" << reason << "\"}";
		result.InfoJson = info.str();
		return result;
	}

	RLObservation SceneRLEnvironment::BuildObservation() const
	{
		const vec3 pos = GetAgentPosition();
		const vec3 vel = GetAgentVelocity();
		const vec3 rel = m_TargetPosition - pos;
		const f32 dist = IsFiniteVec3(rel) ? glm::length(rel) : 0.0f;

		auto sanitize = [](f32 v) -> f32 { return std::isfinite(v) ? v : 0.0f; };

		RLObservation obs;
		obs.Data = {
			sanitize(pos.x), sanitize(pos.y), sanitize(pos.z),
			sanitize(vel.x), sanitize(vel.y), sanitize(vel.z),
			sanitize(rel.x), sanitize(rel.y), sanitize(rel.z),
			sanitize(dist)
		};
		return obs;
	}

	RLSpace SceneRLEnvironment::GetObservationSpace() const
	{
		RLSpace space;
		space.Type = ERLActionType::Continuous;
		space.Box.Shape = { ObservationDim };
		const f32 inf = std::numeric_limits<f32>::infinity();
		space.Box.Low = {
			-m_Config.ArenaHalfExtent, m_Config.FallYThreshold, -m_Config.ArenaHalfExtent,
			-inf, -inf, -inf,
			-inf, -inf, -inf,
			0.0f
		};
		space.Box.High = {
			m_Config.ArenaHalfExtent, inf, m_Config.ArenaHalfExtent,
			inf, inf, inf,
			inf, inf, inf,
			inf
		};
		return space;
	}

	RLSpace SceneRLEnvironment::GetActionSpace() const
	{
		RLSpace space;
		space.Type = m_Config.ActionType;
		if (m_Config.ActionType == ERLActionType::Continuous)
		{
			space.Box.Shape = { ContinuousActionDim };
			space.Box.Low = { -1.0f, -1.0f, -1.0f };
			space.Box.High = { 1.0f, 1.0f, 1.0f };
		}
		else
		{
			space.Discrete.N = DiscreteActionCount;
		}
		return space;
	}

	vec3 SceneRLEnvironment::GetAgentPosition() const
	{
		if (!m_Scene || m_AgentEntity == entt::null) return vec3(0.0f);
		const auto& registry = m_Scene->GetRegistry();
		if (const auto* tr = registry.try_get<TransformComponent>(m_AgentEntity))
		{
			return tr->position;
		}
		return vec3(0.0f);
	}

	vec3 SceneRLEnvironment::GetAgentVelocity() const
	{
		if (!m_Scene || m_AgentEntity == entt::null) return vec3(0.0f);
		const auto& registry = m_Scene->GetRegistry();
		if (const auto* body = registry.try_get<RigidBodyComponent>(m_AgentEntity))
		{
			return body->LinearVelocity;
		}
		return vec3(0.0f);
	}

	void SceneRLEnvironment::SetTargetPosition(const vec3& target)
	{
		if (!IsFiniteVec3(target)) return;
		m_TargetPosition = target;
		m_PreviousDistance = glm::length(m_TargetPosition - GetAgentPosition());
	}
}
