#include "NavGrid.h"
#include "NavMesh.h"
#include "NavAgentComponent.h"
#include "AIPerceptionComponent.h"
#include "NavigationSystem.h"
#include "Scene.h"
#include "TransformComponent.h"
#include "WorldTransformComponent.h"
#include "TransformSystem.h"
#include "ColliderComponent.h"
#include "TerrainComponent.h"
#include "TagComponent.h"

#include <iostream>
#include <memory>
#include <cmath>
#include <filesystem>

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

	bool TestTerrainNavGridBakeAndPersistence()
	{
		std::unique_ptr<Scene, ReleaseObject> scene(Scene::Create());
		if (!scene)
		{
			std::cerr << "Failed to create scene for terrain NavGrid test.\n";
			return false;
		}

		auto& world = scene->GetRegistry();
		const auto terrainEntity = scene->CreateEntity().GetEntityHandle();
		auto& tr = world.get_or_emplace<TransformComponent>(terrainEntity);
		auto& wt = world.get_or_emplace<WorldTransformComponent>(terrainEntity);
		tr.SetLocalPosition(vec3(0.0f, 0.0f, 0.0f));
		TransformSystem::UpdateRootWorld(tr, wt);

		auto& terrain = world.emplace<TerrainComponent>(terrainEntity);
		terrain.Width = 16.0f;
		terrain.Length = 16.0f;
		terrain.MaxHeight = 20.0f;
		terrain.Resolution = 16;
		const size_t axis = 17;
		terrain.HeightData.assign(axis * axis, 0.0f);
		terrain.AttributeData.assign(axis * axis, static_cast<uint8>(ETerrainAttribute::None));

		// 상반부(vz 0..7): 중앙에 급경사 절벽(높이 10m -> 경사 > 45도)
		for (size_t vz = 0; vz <= 7; ++vz)
		{
			terrain.HeightData[vz * axis + 8] = 0.5f;
		}
		// 중하반부(vz 8..13): 중앙에 Blocked 속성 영역, vz 14..16은 우회 통로로 개방
		for (size_t vz = 8; vz <= 13; ++vz)
		{
			terrain.AttributeData[vz * axis + 7] = static_cast<uint8>(ETerrainAttribute::Blocked);
			terrain.AttributeData[vz * axis + 8] = static_cast<uint8>(ETerrainAttribute::Blocked);
		}

		BoundingBox bounds;
		if (!NavGrid::CalculateSceneBounds(*scene, bounds) || bounds.min.x > -7.9f || bounds.max.x < 7.9f)
		{
			std::cerr << "CalculateSceneBounds did not include terrain bounds.\n";
			return false;
		}

		NavGrid grid(16, 16, 1.0f, vec3(-8.0f, 0.0f, -8.0f));
		grid.BakeObstaclesFromScene(*scene, 0.4f, 45.0f, 0.5f);

		// 급경사(cell 7,3)와 Blocked(cell 7,10)는 비통행, 평탄 영역(cell 3,3 / 12,3 / 7,15)은 통행 가능
		if (grid.IsWalkable(7, 3))
		{
			std::cerr << "Steep terrain slope cell was unexpectedly walkable.\n";
			return false;
		}
		if (grid.IsWalkable(7, 10))
		{
			std::cerr << "Blocked terrain attribute cell was unexpectedly walkable.\n";
			return false;
		}
		if (!grid.IsWalkable(3, 3) || !grid.IsWalkable(12, 3) || !grid.IsWalkable(7, 15))
		{
			std::cerr << "Flat walkable terrain cells were unexpectedly blocked.\n";
			return false;
		}

		const vec3 start(-4.5f, 0.0f, -4.5f);
		const vec3 goal(4.5f, 0.0f, -4.5f);
		std::vector<vec3> path;
		if (!grid.FindPath(start, goal, path) || path.size() < 3)
		{
			std::cerr << "Failed to find detour path around steep slope and Blocked terrain.\n";
			return false;
		}

		// 경로는 남쪽 우회 통로(z > 5.0)를 거쳐야 함
		bool detouredSouth = false;
		for (const auto& wp : path)
		{
			if (wp.z > 5.0f) detouredSouth = true;
		}
		if (!detouredSouth)
		{
			std::cerr << "Path did not detour around terrain obstacles.\n";
			return false;
		}

		// UTF-8/한글 경로 저장 → 재로드 왕복 검증
		const auto folder = std::filesystem::temp_directory_path() / L"BamNavGridT4Check";
		std::error_code ec;
		std::filesystem::remove_all(folder, ec);
		std::filesystem::create_directories(folder, ec);
		const auto filePath = folder / L"지형_네비.bamnavgrid";

		if (!grid.SaveToFile(filePath))
		{
			std::cerr << "NavGrid::SaveToFile failed for UTF-8 path.\n";
			return false;
		}

		NavGrid loaded;
		if (!loaded.LoadFromFile(filePath) || loaded.GetWidth() != 16 || loaded.GetHeight() != 16 ||
			loaded.IsWalkable(7, 3) || loaded.IsWalkable(7, 10) || !loaded.IsWalkable(7, 15))
		{
			std::cerr << "NavGrid::LoadFromFile did not restore baked terrain walkability.\n";
			std::filesystem::remove_all(folder, ec);
			return false;
		}

		std::vector<vec3> reloadedPath;
		if (!loaded.FindPath(start, goal, reloadedPath) || reloadedPath.size() != path.size())
		{
			std::cerr << "Reloaded NavGrid produced a different path.\n";
			std::filesystem::remove_all(folder, ec);
			return false;
		}

		std::filesystem::remove_all(folder, ec);
		return true;
	}

	bool TestMultiLayerNavMesh()
	{
		// 5-1. 다층(1층 바닥 + 램프 + 2층 오버패스 다리 + 급경사 필터링) 베이크 검증
		NavMeshBuildInput input;
		input.settings.maxSlopeDegrees = 45.0f;
		input.settings.maxStepHeight = 0.5f;

		// 1층 바닥 (y = 0, x: -6..6, z: -6..6)
		input.AddQuad(vec3(-6.0f, 0.0f, -6.0f), vec3(-6.0f, 0.0f, 6.0f), vec3(6.0f, 0.0f, 6.0f), vec3(6.0f, 0.0f, -6.0f));

		// 1층(x=2) -> 2층(x=6)으로 올라가는 램프 (z: -1..1, 경사각 atan(3/4) ≈ 36.87 deg <= 45 deg)
		input.AddQuad(vec3(2.0f, 0.0f, -1.0f), vec3(2.0f, 0.0f, 1.0f), vec3(6.0f, 3.0f, 1.0f), vec3(6.0f, 3.0f, -1.0f));

		// 2층 오버패스 다리 (y = 3, x: -4..6, z: -1..1) — 1층 바닥과 XZ가 겹치는 2층 레이어
		input.AddQuad(vec3(-4.0f, 3.0f, -1.0f), vec3(-4.0f, 3.0f, 1.0f), vec3(6.0f, 3.0f, 1.0f), vec3(6.0f, 3.0f, -1.0f));

		// 60도 초과 급경사 삼각형 (필터링되어야 함)
		input.AddTriangle(vec3(-5.0f, 0.0f, -5.0f), vec3(-5.0f, 0.0f, -4.0f), vec3(-4.5f, 3.0f, -4.5f));

		NavMesh navMesh;
		if (!navMesh.Build(input))
		{
			std::cerr << "NavMesh::Build failed for multi-layer geometry.\n";
			return false;
		}

		// 1층(2) + 램프(2) + 2층(2) = 6개 삼각형만 남아야 함 (급경사 1개 제외)
		if (navMesh.GetPolygonCount() != 6)
		{
			std::cerr << "NavMesh slope filtering mismatch. Expected 6 polygons, got " << navMesh.GetPolygonCount() << ".\n";
			return false;
		}

		// 2층 다리 위 좌표는 1층(y=0)이 아닌 2층(y=3) 폴리곤을 선택해야 함
		vec3 snappedUpper(0.0f);
		const int32 upperPoly = navMesh.FindClosestPolygon(vec3(0.0f, 3.1f, 0.0f), &snappedUpper);
		if (upperPoly < 0 || std::abs(snappedUpper.y - 3.0f) > 0.1f)
		{
			std::cerr << "NavMesh::FindClosestPolygon did not select upper bridge layer.\n";
			return false;
		}

		// 5-2. 1층(-3, 0, 0) -> 2층 다리(-2, 3, 0) 3D 경로 탐색 (램프 경유 필수)
		const vec3 startPos(-3.0f, 0.0f, 0.0f);
		const vec3 goalPos(-2.0f, 3.0f, 0.0f);
		std::vector<vec3> path;
		if (!navMesh.FindPath(startPos, goalPos, path) || path.size() < 3)
		{
			std::cerr << "NavMesh::FindPath failed on multi-layer ramp + bridge.\n";
			return false;
		}

		bool visitedRampEntry = false;
		for (const auto& wp : path)
		{
			if (wp.x >= 1.9f)
			{
				visitedRampEntry = true;
			}
		}
		if (!visitedRampEntry || std::abs(path.back().y - 3.0f) > 0.1f)
		{
			std::cerr << "Multi-layer NavMesh path did not traverse ramp to reach upper bridge.\n";
			return false;
		}

		// 5-3. .bamnavmesh UTF-8/한글 경로 저장 → 재로드 왕복 검증
		const auto folder = std::filesystem::temp_directory_path() / L"BamNavMeshCheck";
		std::error_code ec;
		std::filesystem::remove_all(folder, ec);
		std::filesystem::create_directories(folder, ec);
		const auto filePath = folder / L"다층_네비.bamnavmesh";

		if (!navMesh.SaveToFile(filePath))
		{
			std::cerr << "NavMesh::SaveToFile failed.\n";
			return false;
		}

		auto loadedMesh = std::make_shared<NavMesh>();
		if (!loadedMesh->LoadFromFile(filePath) ||
			loadedMesh->GetPolygonCount() != navMesh.GetPolygonCount() ||
			loadedMesh->GetVertexCount() != navMesh.GetVertexCount())
		{
			std::cerr << "NavMesh::LoadFromFile failed or mismatched counts.\n";
			std::filesystem::remove_all(folder, ec);
			return false;
		}

		std::vector<vec3> reloadedPath;
		if (!loadedMesh->FindPath(startPos, goalPos, reloadedPath) || reloadedPath.size() != path.size())
		{
			std::cerr << "Reloaded NavMesh produced a different path.\n";
			std::filesystem::remove_all(folder, ec);
			return false;
		}
		std::filesystem::remove_all(folder, ec);

		// 5-4. NavigationSystem NavMesh 우선 사용 및 3D 에이전트 이동 검증
		std::unique_ptr<Scene, ReleaseObject> scene(Scene::Create());
		if (!scene)
		{
			std::cerr << "Failed to create scene for NavMesh NavigationSystem test.\n";
			return false;
		}
		NavigationSystem::SetSceneNavMesh(scene.get(), loadedMesh);

		auto& world = scene->GetRegistry();
		const auto agentEntity = scene->CreateEntity().GetEntityHandle();
		auto& tr = world.get_or_emplace<TransformComponent>(agentEntity);
		auto& wt = world.get_or_emplace<WorldTransformComponent>(agentEntity);
		tr.SetLocalPosition(startPos);
		TransformSystem::UpdateRootWorld(tr, wt);

		auto& agent = world.emplace<NavAgentComponent>(agentEntity);
		agent.moveSpeed = 8.0f;
		agent.stoppingDistance = 0.25f;
		agent.SetDestination(goalPos);

		for (int step = 0; step < 120; ++step)
		{
			NavigationSystem::ProcessScene(*scene, 0.05f);
			if (agent.reachedDestination)
			{
				break;
			}
		}

		if (!agent.reachedDestination || std::abs(tr.position.y - 3.0f) > 0.35f)
		{
			std::cerr << "NavAgent failed to climb ramp onto 2nd floor bridge via NavigationSystem NavMesh.\n";
			NavigationSystem::RemoveSceneNavMesh(scene.get());
			return false;
		}

		NavigationSystem::RemoveSceneNavMesh(scene.get());
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

	std::cout << "[NavigationAICheck] Running Terrain NavGrid bake & persistence tests...\n";
	if (!TestTerrainNavGridBakeAndPersistence())
	{
		std::cerr << "[NavigationAICheck] FAILED: Terrain NavGrid bake & persistence\n";
		return 1;
	}

	std::cout << "[NavigationAICheck] Running 3D Multi-Layer NavMesh tests...\n";
	if (!TestMultiLayerNavMesh())
	{
		std::cerr << "[NavigationAICheck] FAILED: 3D Multi-Layer NavMesh\n";
		return 1;
	}

	std::cout << "[NavigationAICheck] All tests PASSED successfully!\n";
	return 0;
}
