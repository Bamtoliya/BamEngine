#include "NavGrid.h"
#include "NavAgentComponent.h"
#include "AIPerceptionComponent.h"
#include "NavigationSystem.h"
#include "Scene.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "TransformSystem.h"
#include "ColliderComponent.h"
#include "TagComponent.h"

#include <iostream>
#include <memory>
#include <cmath>

namespace
{
	using namespace Engine;

	struct ReleaseObject
	{
		void operator()(Base* object) const
		{
			if (object)
			{
				object->Release();
			}
		}
	};

	bool TestNavGridAStar()
	{
		// 1-1. 평탄한 열린 공간 경로 탐색
		NavGrid openGrid(20, 20, 1.0f, vec3(0.0f));
		if (openGrid.GetWidth() != 20 || openGrid.GetHeight() != 20 || openGrid.GetCellSize() != 1.0f)
		{
			std::cerr << "NavGrid dimension or cell size mismatch.\n";
			return false;
		}

		std::vector<vec3> waypoints;
		const vec3 start(1.5f, 0.0f, 1.5f);
		const vec3 goal(18.5f, 0.0f, 18.5f);

		if (!openGrid.FindPath(start, goal, waypoints))
		{
			std::cerr << "Failed to find path in open grid.\n";
			return false;
		}

		if (waypoints.empty() || glm::distance(waypoints.back(), goal) > 1e-3f)
		{
			std::cerr << "Goal waypoint is incorrect in open grid path.\n";
			return false;
		}

		// 1-2. 장애물 우회 경로 탐색 (Wall 우회)
		NavGrid obstacleGrid(11, 11, 1.0f, vec3(0.0f));
		// z = 5에 x = 1..9 장애물 배치 (x = 0, 10은 통행 가능)
		for (int32 x = 1; x <= 9; ++x)
		{
			obstacleGrid.SetWalkable(x, 5, false);
		}

		const vec3 obsStart(5.5f, 0.0f, 2.5f);
		const vec3 obsGoal(5.5f, 0.0f, 8.5f);
		waypoints.clear();

		if (!obstacleGrid.FindPath(obsStart, obsGoal, waypoints))
		{
			std::cerr << "Failed to find path around obstacle wall.\n";
			return false;
		}

		// 경로 선분이 장애물 셀을 통과하지 않는지 검증
		for (size_t i = 0; i + 1 < waypoints.size(); ++i)
		{
			if (!obstacleGrid.IsLineWalkable(waypoints[i], waypoints[i + 1]))
			{
				std::cerr << "Smoothed waypoint line penetrated obstacle.\n";
				return false;
			}
		}

		// 1-3. 막힌 목표 실패 반환
		obstacleGrid.SetWalkable(5, 8, false);
		waypoints.clear();
		if (obstacleGrid.FindPath(obsStart, obsGoal, waypoints))
		{
			std::cerr << "FindPath should fail when goal cell is blocked.\n";
			return false;
		}

		// 1-4. 대각선 코너 컷 방지 검증 (Corner Cut Prevention)
		NavGrid cornerGrid(4, 4, 1.0f, vec3(0.0f));
		cornerGrid.SetWalkable(1, 0, false);
		cornerGrid.SetWalkable(0, 1, false);
		waypoints.clear();
		// (0,0)에서 (1,1)로의 대각선 이동 시 직교 인접 셀(1,0), (0,1)이 막혀있으므로 차단되어야 함
		if (cornerGrid.FindPath(vec3(0.5f, 0.0f, 0.5f), vec3(1.5f, 0.0f, 1.5f), waypoints, true))
		{
			std::cerr << "Diagonal path cutting through blocked corners should not be allowed.\n";
			return false;
		}

		// 1-5. 씬 장애물 베이크 검증 (BakeObstaclesFromScene)
		std::unique_ptr<Scene, ReleaseObject> scene(Scene::Create());
		if (!scene)
		{
			std::cerr << "Failed to create scene for bake test.\n";
			return false;
		}

		auto& world = scene->GetRegistry();
		const auto wallEntity = scene->CreateEntity().GetEntityHandle();
		auto& tr = world.get_or_emplace<TransformComponent>(wallEntity);
		auto& wt = world.get_or_emplace<WorldTransformComponent>(wallEntity);
		tr.SetLocalPosition(vec3(5.0f, 0.0f, 5.0f));
		TransformSystem::UpdateRootWorld(tr, wt);

		auto& col = world.emplace<ColliderComponent>(wallEntity);
		col.SetShape(BoxShape{ vec3(2.0f, 2.0f, 2.0f) });

		NavGrid bakeGrid(15, 15, 1.0f, vec3(0.0f));
		bakeGrid.BakeObstaclesFromScene(*scene, 0.4f);

		// 중심 셀 (5, 5) 및 에이전트 반경으로 확장된 셀 (4, 5)는 비통행 상태여야 함
		if (bakeGrid.IsWalkable(5, 5) || bakeGrid.IsWalkable(4, 5) || bakeGrid.IsWalkable(6, 6))
		{
			std::cerr << "Baked scene obstacle did not mark expected cells as unwalkable.\n";
			return false;
		}

		// 장애물에서 먼 셀 (1, 1)은 통행 가능해야 함
		if (!bakeGrid.IsWalkable(1, 1))
		{
			std::cerr << "Unrelated cell was unexpectedly marked unwalkable during scene bake.\n";
			return false;
		}

		return true;
	}

	bool TestNavAgentMovement()
	{
		std::unique_ptr<Scene, ReleaseObject> scene(Scene::Create());
		if (!scene)
		{
			std::cerr << "Failed to create scene for agent movement test.\n";
			return false;
		}

		auto& world = scene->GetRegistry();
		auto grid = std::make_shared<NavGrid>(10, 10, 1.0f, vec3(0.0f));
		NavigationSystem::SetSceneNavGrid(scene.get(), grid);

		const auto agentEntity = scene->CreateEntity().GetEntityHandle();
		auto& tr = world.get_or_emplace<TransformComponent>(agentEntity);
		auto& wt = world.get_or_emplace<WorldTransformComponent>(agentEntity);
		tr.SetLocalPosition(vec3(1.0f, 0.0f, 1.0f));
		TransformSystem::UpdateRootWorld(tr, wt);

		auto& agent = world.emplace<NavAgentComponent>(agentEntity);
		agent.moveSpeed = 5.0f;
		agent.stoppingDistance = 0.25f;
		agent.SetDestination(vec3(7.0f, 0.0f, 1.0f));

		if (!agent.hasDestination || !agent.pathPending || agent.reachedDestination)
		{
			std::cerr << "NavAgent initial destination state is incorrect.\n";
			return false;
		}

		// 이동 시뮬레이션 루프
		for (int step = 0; step < 40; ++step)
		{
			NavigationSystem::ProcessScene(*scene, 0.05f);
			if (agent.reachedDestination)
			{
				break;
			}
		}

		if (!agent.reachedDestination)
		{
			std::cerr << "NavAgent failed to reach destination within time limit.\n";
			return false;
		}

		if (agent.HasActivePath())
		{
			std::cerr << "NavAgent should not have active path after reaching destination.\n";
			return false;
		}

		const f32 finalDist = glm::distance(tr.position, vec3(7.0f, 0.0f, 1.0f));
		if (finalDist > agent.stoppingDistance)
		{
			std::cerr << "NavAgent stopped beyond stopping distance threshold.\n";
			return false;
		}

		if (glm::length(agent.velocity) > 1e-3f)
		{
			std::cerr << "NavAgent velocity should be zero after arrival.\n";
			return false;
		}

		NavigationSystem::RemoveSceneNavGrid(scene.get());
		return true;
	}

	bool TestAIPerception()
	{
		std::unique_ptr<Scene, ReleaseObject> scene(Scene::Create());
		if (!scene)
		{
			std::cerr << "Failed to create scene for AI perception test.\n";
			return false;
		}

		auto& world = scene->GetRegistry();

		// AI 엔티티 생성 (+Z 전방 응시)
		const auto aiEntity = scene->CreateEntity().GetEntityHandle();
		auto& aiTr = world.get_or_emplace<TransformComponent>(aiEntity);
		auto& aiWt = world.get_or_emplace<WorldTransformComponent>(aiEntity);
		aiTr.SetLocalPosition(vec3(0.0f, 0.0f, 0.0f));
		aiTr.SetEulerRotation(vec3(0.0f, 0.0f, 0.0f));
		TransformSystem::UpdateRootWorld(aiTr, aiWt);

		auto& perception = world.emplace<AIPerceptionComponent>(aiEntity);
		perception.sightRange = 10.0f;
		perception.sightHalfAngleDegrees = 45.0f;
		perception.hearingRange = 3.0f;
		perception.targetTag = "Player";
		perception.requireLineOfSight = true;

		// 타깃(Player) 엔티티 생성 (전방 5.0m 위치)
		const auto playerEntity = scene->CreateEntity().GetEntityHandle();
		auto& playerTr = world.get_or_emplace<TransformComponent>(playerEntity);
		auto& playerWt = world.get_or_emplace<WorldTransformComponent>(playerEntity);
		playerTr.SetLocalPosition(vec3(0.0f, 0.0f, 5.0f));
		TransformSystem::UpdateRootWorld(playerTr, playerWt);

		auto& tagComp = world.emplace<TagComponent>(playerEntity);
		tagComp.tags.insert(L"Player");

		// 3-1. 시야각 내부 및 시야 범위 내 타깃 감지 (시야선 확보)
		NavigationSystem::ProcessScene(*scene, 0.1f);
		if (!perception.HasTarget() || perception.detectedTarget != playerEntity || !perception.hasLineOfSight)
		{
			std::cerr << "Failed to detect player within sight cone and range with LOS.\n";
			return false;
		}
		if (glm::distance(perception.lastKnownTargetPos, vec3(0.0f, 0.0f, 5.0f)) > 1e-3f)
		{
			std::cerr << "Perception lastKnownTargetPos is incorrect.\n";
			return false;
		}

		// 3-2. 시야각 외부 (후방) 및 청각 범위 밖 (거리 6.0m)
		playerTr.SetLocalPosition(vec3(0.0f, 0.0f, -6.0f));
		TransformSystem::UpdateRootWorld(playerTr, playerWt);
		perception.ClearTarget();

		NavigationSystem::ProcessScene(*scene, 0.1f);
		if (perception.HasTarget())
		{
			std::cerr << "AI should not detect player behind outside hearing range.\n";
			return false;
		}

		// 3-3. 시야각 외부 (후방)이지만 청각 범위 내 (거리 2.0m)
		playerTr.SetLocalPosition(vec3(0.0f, 0.0f, -2.0f));
		TransformSystem::UpdateRootWorld(playerTr, playerWt);
		perception.ClearTarget();

		NavigationSystem::ProcessScene(*scene, 0.1f);
		if (!perception.HasTarget() || perception.detectedTarget != playerEntity || perception.hasLineOfSight)
		{
			std::cerr << "AI should hear player behind within hearing range without LOS.\n";
			return false;
		}

		// 3-4. 전방이지만 시야 범위 초과 (거리 20.0m)
		playerTr.SetLocalPosition(vec3(0.0f, 0.0f, 20.0f));
		TransformSystem::UpdateRootWorld(playerTr, playerWt);
		perception.ClearTarget();

		NavigationSystem::ProcessScene(*scene, 0.1f);
		if (perception.HasTarget())
		{
			std::cerr << "AI should not detect player beyond sight range.\n";
			return false;
		}

		// 3-5. 시야각 및 시야 범위 내이지만 벽에 의해 LOS 차폐 (거리 6.0m, 청각 범위 3.0m 초과)
		playerTr.SetLocalPosition(vec3(0.0f, 0.0f, 6.0f));
		TransformSystem::UpdateRootWorld(playerTr, playerWt);

		// AI(0,0,0)와 Player(0,0,6) 사이 z = 3.0에 벽 장애물 배치
		const auto wallEntity = scene->CreateEntity().GetEntityHandle();
		auto& wallTr = world.get_or_emplace<TransformComponent>(wallEntity);
		auto& wallWt = world.get_or_emplace<WorldTransformComponent>(wallEntity);
		wallTr.SetLocalPosition(vec3(0.0f, 0.0f, 3.0f));
		TransformSystem::UpdateRootWorld(wallTr, wallWt);

		auto& wallCol = world.emplace<ColliderComponent>(wallEntity);
		wallCol.SetShape(BoxShape{ vec3(6.0f, 6.0f, 1.0f) });

		perception.ClearTarget();
		NavigationSystem::ProcessScene(*scene, 0.1f);

		if (perception.hasLineOfSight || perception.HasTarget())
		{
			std::cerr << "AI should not see player occluded by wall collider.\n";
			return false;
		}

		// 3-6. 벽 차폐 비활성화 후 정상 재감지 확인
		wallCol.SetSettings(ColliderSettings{ .Enabled = false });
		NavigationSystem::ProcessScene(*scene, 0.1f);

		if (!perception.HasTarget() || perception.detectedTarget != playerEntity || !perception.hasLineOfSight)
		{
			std::cerr << "AI failed to redetect player after wall occlusion was removed.\n";
			return false;
		}

		return true;
	}
}

int main()
{
	std::cout << "[NavigationAICheck] Running NavGrid & A* Pathfinding tests...\n";
	if (!TestNavGridAStar())
	{
		std::cerr << "[NavigationAICheck] FAILED: NavGrid A* Pathfinding\n";
		return 1;
	}

	std::cout << "[NavigationAICheck] Running NavAgentComponent movement tests...\n";
	if (!TestNavAgentMovement())
	{
		std::cerr << "[NavigationAICheck] FAILED: NavAgent movement\n";
		return 1;
	}

	std::cout << "[NavigationAICheck] Running AIPerceptionComponent tests...\n";
	if (!TestAIPerception())
	{
		std::cerr << "[NavigationAICheck] FAILED: AI perception\n";
		return 1;
	}

	std::cout << "[NavigationAICheck] All tests PASSED successfully!\n";
	return 0;
}
