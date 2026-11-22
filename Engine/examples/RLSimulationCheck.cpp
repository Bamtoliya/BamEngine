#include "CartPoleEnvironment.h"
#include "SceneRLEnvironment.h"
#include "VectorizedRLEnvironment.h"
#if !defined(__EMSCRIPTEN__)
#include "RLServer.h"
#endif

#include <cmath>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace
{
	using namespace Engine;

	int Fail(const char* message)
	{
		std::cerr << "[RLSimulationCheck] FAIL: " << message << '\n';
		return 1;
	}

	bool AllFinite(const std::vector<float>& values)
	{
		for (float v : values)
		{
			if (!std::isfinite(v)) return false;
		}
		return true;
	}

	bool CheckSceneDeterministicReset()
	{
		SceneRLEnvConfig cfg;
		cfg.AgentSpawnMin = vec3(-2.0f, 0.5f, -2.0f);
		cfg.AgentSpawnMax = vec3(2.0f, 0.5f, 2.0f);
		cfg.TargetMin = vec3(3.0f, 0.5f, -3.0f);
		cfg.TargetMax = vec3(6.0f, 0.5f, 3.0f);

		SceneRLEnvironment env(cfg);
		const RLSpace obsSpace = env.GetObservationSpace();
		const RLSpace actSpace = env.GetActionSpace();
		if (obsSpace.Type != ERLActionType::Continuous || obsSpace.Box.Shape.size() != 1 ||
			obsSpace.Box.Shape[0] != SceneRLEnvironment::ObservationDim)
		{
			return false;
		}
		if (actSpace.Type != ERLActionType::Continuous || actSpace.Box.Shape.size() != 1 ||
			actSpace.Box.Shape[0] != SceneRLEnvironment::ContinuousActionDim)
		{
			return false;
		}

		const RLObservation obsA = env.Reset(12345ull);
		const RLObservation obsB = env.Reset(12345ull);
		const RLObservation obsC = env.Reset(99999ull);

		if (obsA.Size() != static_cast<std::size_t>(SceneRLEnvironment::ObservationDim) ||
			!AllFinite(obsA.Data) || !AllFinite(obsB.Data) || !AllFinite(obsC.Data))
		{
			return false;
		}

		if (obsA.Data != obsB.Data)
		{
			return false;
		}

		if (obsA.Data == obsC.Data)
		{
			return false;
		}

		return true;
	}

	bool CheckSceneGoalNavigation()
	{
		SceneRLEnvConfig cfg;
		cfg.SpawnObstacle = false;
		cfg.AgentSpawnMin = vec3(0.0f, 0.5f, 0.0f);
		cfg.AgentSpawnMax = vec3(0.0f, 0.5f, 0.0f);
		cfg.TargetMin = vec3(3.5f, 0.5f, 0.0f);
		cfg.TargetMax = vec3(3.5f, 0.5f, 0.0f);
		cfg.TargetReachRadius = 0.85f;
		cfg.MaxControlForce = 30.0f;
		cfg.MaxSteps = 200;

		SceneRLEnvironment env(cfg);
		const RLObservation initialObs = env.Reset(42ull);
		const float initialDistance = initialObs.Data[9];
		if (initialDistance < 3.0f) return false;

		RLAction pushTowardGoal;
		pushTowardGoal.Type = ERLActionType::Continuous;
		pushTowardGoal.ContinuousActions = { 1.0f, 0.0f, 0.0f };

		bool sawPositiveShapingReward = false;
		bool reachedGoal = false;
		float bestDistance = initialDistance;

		for (int i = 0; i < cfg.MaxSteps; ++i)
		{
			const RLStepResult res = env.Step(pushTowardGoal);
			if (!AllFinite(res.Observation.Data) || !std::isfinite(res.Reward)) return false;

			const float dist = res.Observation.Data[9];
			if (dist < bestDistance) bestDistance = dist;

			if (!res.Terminated && res.Reward > 0.0f)
			{
				sawPositiveShapingReward = true;
			}

			if (res.Terminated)
			{
				if (res.Truncated) return false;
				if (res.Reward < cfg.GoalReward * 0.5f) return false;
				if (res.InfoJson.find("\"goal\"") == std::string::npos) return false;
				reachedGoal = true;
				break;
			}
		}

		return sawPositiveShapingReward && reachedGoal && bestDistance < initialDistance;
	}

	bool CheckSceneFallTermination()
	{
		SceneRLEnvConfig cfg;
		cfg.SpawnObstacle = false;
		cfg.FloorCenter = vec3(0.0f, -0.5f, 0.0f);
		cfg.FloorSize = vec3(4.0f, 1.0f, 4.0f);
		cfg.AgentSpawnMin = vec3(1.5f, 0.5f, 0.0f);
		cfg.AgentSpawnMax = vec3(1.5f, 0.5f, 0.0f);
		cfg.TargetMin = vec3(-1.5f, 0.5f, 0.0f);
		cfg.TargetMax = vec3(-1.5f, 0.5f, 0.0f);
		cfg.FallYThreshold = -1.5f;
		cfg.ArenaHalfExtent = 40.0f;
		cfg.MaxControlForce = 20.0f;
		cfg.MaxSteps = 250;

		SceneRLEnvironment env(cfg);
		(void)env.Reset(7ull);

		RLAction pushOffEdge;
		pushOffEdge.Type = ERLActionType::Continuous;
		pushOffEdge.ContinuousActions = { 1.0f, 0.0f, 0.0f };

		bool fellOff = false;
		for (int i = 0; i < cfg.MaxSteps; ++i)
		{
			const RLStepResult res = env.Step(pushOffEdge);
			if (res.Terminated)
			{
				if (res.Truncated) return false;
				if (res.Reward >= 0.0f) return false;
				if (res.InfoJson.find("\"fall\"") == std::string::npos) return false;
				fellOff = true;
				break;
			}
		}

		return fellOff;
	}

	bool CheckSceneTruncation()
	{
		SceneRLEnvConfig cfg;
		cfg.SpawnObstacle = false;
		cfg.AgentSpawnMin = vec3(0.0f, 0.5f, 0.0f);
		cfg.AgentSpawnMax = vec3(0.0f, 0.5f, 0.0f);
		cfg.TargetMin = vec3(5.0f, 0.5f, 0.0f);
		cfg.TargetMax = vec3(5.0f, 0.5f, 0.0f);
		cfg.MaxSteps = 5;

		SceneRLEnvironment env(cfg);
		(void)env.Reset(11ull);

		RLAction idle;
		idle.Type = ERLActionType::Continuous;
		idle.ContinuousActions = { 0.0f, 0.0f, 0.0f };

		for (int i = 0; i < 4; ++i)
		{
			const RLStepResult res = env.Step(idle);
			if (res.Terminated || res.Truncated) return false;
		}

		const RLStepResult last = env.Step(idle);
		return !last.Terminated && last.Truncated && (last.InfoJson.find("\"max_steps\"") != std::string::npos);
	}

	bool CheckVectorizedEnvironmentAndAutoReset()
	{
		// 1. 4x SceneRLEnvironment batch stepping & truncation AutoReset
		SceneRLEnvConfig cfg;
		cfg.SpawnObstacle = false;
		cfg.AgentSpawnMin = vec3(-1.0f, 0.5f, -1.0f);
		cfg.AgentSpawnMax = vec3(1.0f, 0.5f, 1.0f);
		cfg.TargetMin = vec3(4.0f, 0.5f, -1.0f);
		cfg.TargetMax = vec3(5.0f, 0.5f, 1.0f);
		cfg.MaxControlForce = 25.0f;
		cfg.MaxSteps = 6;

		auto vecScene = VectorizedRLEnvironment::CreateSceneNavigation(4, cfg, true);
		if (!vecScene || vecScene->GetNumEnvs() != 4) return false;

		const auto initObs = vecScene->ResetAll(100ull);
		if (initObs.size() != 4) return false;
		for (std::size_t i = 0; i < 4; ++i)
		{
			if (initObs[i].Size() != static_cast<std::size_t>(SceneRLEnvironment::ObservationDim)) return false;
			if (!AllFinite(initObs[i].Data)) return false;
		}
		if (initObs[0].Data == initObs[1].Data) return false;

		const std::vector<std::vector<f32>> batchActions = {
			{ 1.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, 1.0f },
			{ -1.0f, 0.0f, 0.0f },
			{ 0.0f, 0.0f, -1.0f }
		};

		RLVectorStepResult lastBatch;
		for (int step = 0; step < cfg.MaxSteps; ++step)
		{
			lastBatch = vecScene->StepAll(batchActions);
			if (lastBatch.Observations.size() != 4 || lastBatch.Rewards.size() != 4) return false;
		}

		for (std::size_t i = 0; i < 4; ++i)
		{
			if (!lastBatch.Truncated[i] && !lastBatch.Terminated[i]) return false;
			if (!lastBatch.FinalObservations[i].has_value()) return false;
			if (lastBatch.FinalObservations[i]->Size() != static_cast<std::size_t>(SceneRLEnvironment::ObservationDim)) return false;
			if (lastBatch.Observations[i].Data == lastBatch.FinalObservations[i]->Data) return false;
			if (vecScene->GetEpisodeCount(i) != 1ull) return false;

			auto* subEnv = dynamic_cast<SceneRLEnvironment*>(vecScene->GetEnvironment(i));
			if (!subEnv || subEnv->GetStepCount() != 0) return false;
		}

		// 2. 4x CartPoleEnvironment termination AutoReset
		auto vecCartPole = VectorizedRLEnvironment::CreateCartPole(4, true);
		const auto cpInit = vecCartPole->ResetAll(777ull);
		if (cpInit.size() != 4 || cpInit[0].Size() != 4) return false;

		bool sawCartPoleAutoReset = false;
		const std::vector<int64_t> pushRight = { 1, 1, 1, 1 };
		for (int step = 0; step < 100; ++step)
		{
			const RLVectorStepResult res = vecCartPole->StepAllDiscrete(pushRight);
			for (std::size_t i = 0; i < 4; ++i)
			{
				if (res.Terminated[i])
				{
					if (!res.FinalObservations[i].has_value()) return false;
					if (res.FinalObservations[i]->Size() != 4) return false;
					// AutoReset된 새 관측값은 [-0.05, 0.05] 범위의 초기 상태여야 한다.
					const float resetTheta = res.Observations[i].Data[2];
					if (std::abs(resetTheta) > 0.06f) return false;
					sawCartPoleAutoReset = true;
				}
			}
			if (sawCartPoleAutoReset) break;
		}

		return sawCartPoleAutoReset;
	}

#if !defined(__EMSCRIPTEN__)
	bool CheckRLServerProtocol()
	{
		auto singleEnv = std::make_shared<CartPoleEnvironment>();
		auto vecEnv = VectorizedRLEnvironment::CreateSceneNavigation(4, true);

		RLServer server(singleEnv);
		server.SetVectorizedEnvironment(vecEnv);

		// 1. 기존 단일 환경 명령 하위 호환 (get_spaces / INFO / RESET / STEP)
		const std::string spacesResp = server.ProcessCommand("{\"cmd\":\"get_spaces\"}");
		if (spacesResp.find("\"status\":\"ok\"") == std::string::npos ||
			spacesResp.find("\"action_space\"") == std::string::npos)
		{
			return false;
		}

		const std::string infoResp = server.ProcessCommand("{\"cmd\":\"INFO\"}");
		if (infoResp.find("\"status\":\"ok\"") == std::string::npos ||
			infoResp.find("CartPole-v1") == std::string::npos)
		{
			return false;
		}

		const std::string resetResp = server.ProcessCommand("{\"cmd\":\"RESET\",\"seed\":42}");
		if (resetResp.find("\"status\":\"ok\"") == std::string::npos ||
			resetResp.find("\"observation\":[") == std::string::npos)
		{
			return false;
		}

		const std::string stepResp = server.ProcessCommand("{\"cmd\":\"STEP\",\"action\":1}");
		if (stepResp.find("\"status\":\"ok\"") == std::string::npos ||
			stepResp.find("\"reward\":") == std::string::npos ||
			stepResp.find("\"terminated\":") == std::string::npos)
		{
			return false;
		}

		// 2. 벡터화 환경 명령 (VEC_INFO / VEC_RESET / VEC_STEP)
		const std::string vecInfoResp = server.ProcessCommand("{\"cmd\":\"VEC_INFO\"}");
		if (vecInfoResp.find("\"status\":\"ok\"") == std::string::npos ||
			vecInfoResp.find("\"num_envs\":4") == std::string::npos ||
			vecInfoResp.find("\"auto_reset\":true") == std::string::npos)
		{
			return false;
		}

		const std::string vecResetResp = server.ProcessCommand("{\"cmd\":\"VEC_RESET\",\"seed\":2026}");
		if (vecResetResp.find("\"status\":\"ok\"") == std::string::npos ||
			vecResetResp.find("\"num_envs\":4") == std::string::npos ||
			vecResetResp.find("\"observations\":[") == std::string::npos)
		{
			return false;
		}

		const std::string vecStepResp = server.ProcessCommand(
			"{\"cmd\":\"VEC_STEP\",\"actions\":[[1.0,0.0,0.0],[0.0,0.0,1.0],[-1.0,0.0,0.0],[0.0,0.0,-1.0]]}");
		if (vecStepResp.find("\"status\":\"ok\"") == std::string::npos ||
			vecStepResp.find("\"num_envs\":4") == std::string::npos ||
			vecStepResp.find("\"rewards\":[") == std::string::npos ||
			vecStepResp.find("\"final_observations\":[") == std::string::npos)
		{
			return false;
		}

		// 3. 오류 처리 검증 (잘못된 JSON, 배치 크기 불일치)
		const std::string invalidJsonResp = server.ProcessCommand("{not-valid-json");
		if (invalidJsonResp.find("\"status\":\"error\"") == std::string::npos) return false;

		const std::string mismatchResp = server.ProcessCommand("{\"cmd\":\"VEC_STEP\",\"actions\":[[1.0,0.0,0.0]]}");
		if (mismatchResp.find("\"status\":\"error\"") == std::string::npos) return false;

		return true;
	}
#endif
}

int main()
{
	if (!CheckSceneDeterministicReset())
		return Fail("SceneRLEnvironment deterministic Reset(seed) failed.");

	if (!CheckSceneGoalNavigation())
		return Fail("SceneRLEnvironment goal navigation / reward / termination failed.");

	if (!CheckSceneFallTermination())
		return Fail("SceneRLEnvironment fall penalty termination failed.");

	if (!CheckSceneTruncation())
		return Fail("SceneRLEnvironment maxSteps truncation failed.");

	if (!CheckVectorizedEnvironmentAndAutoReset())
		return Fail("VectorizedRLEnvironment batch step / AutoReset / finalObservation failed.");

#if !defined(__EMSCRIPTEN__)
	if (!CheckRLServerProtocol())
		return Fail("RLServer ProcessCommand single/vectorized protocol check failed.");
#endif

	std::cout << "[RLSimulationCheck] All checks passed.\n";
	return 0;
}
