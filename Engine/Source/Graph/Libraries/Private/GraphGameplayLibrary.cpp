#include "GraphGameplayLibrary.h"
#include "AnimationIK.h"
#include "AnimationSceneQuery.h"
#include "ComponentEnable.h"
#include "Scene.h"
#include "SaveGameManager.h"
#include "NavAgentComponent.h"
#include "AIPerceptionComponent.h"
#include "AudioManager.h"
#include "AudioClip.h"
#include "AudioMixer.h"
#include "ResourceManager.h"
#include "InputManager.h"
#include "SceneManager.h"
#include "SceneTransitionManager.h"
#include "SystemManager.h"
#include "DataTable.h"
#include "ActionCombatHelpers.h"
#include "StrategyRpgHelpers.h"
#include "PlatformPath.h"
#include "UITween.h"
#include "UIProjection.h"
#include "RectTransformComponent.h"
#include "CameraComponent.h"
#if !defined(__EMSCRIPTEN__)
#include "NetworkSystem.h"
#endif
#include <cmath>
#include <limits>
#include <mutex>
#include <string_view>
#include <unordered_map>

namespace Engine
{
	namespace
	{
		entt::entity ResolveTargetEntity(const entt::registry& reg, uint64 entityId)
		{
			if (entityId == 0)
			{
				return GraphSystem::GetCurrentEntity();
			}
			if (entityId > static_cast<uint64>(std::numeric_limits<uint32>::max()))
			{
				return entt::null;
			}
			const auto target = static_cast<entt::entity>(static_cast<uint32>(entityId));
			return reg.valid(target) ? target : entt::null;
		}

		AnimationIKComponent* FindTargetIKComponent(entt::registry& reg, entt::entity entity)
		{
			if (entity == entt::null || !reg.valid(entity)) return nullptr;
			auto* ik = reg.try_get<AnimationIKComponent>(entity);
			if (!ik)
			{
				const entt::entity animEntity = FindAnimatorEntityInHierarchy(reg, entity);
				if (animEntity != entt::null)
					ik = reg.try_get<AnimationIKComponent>(animEntity);
			}
			return ik;
		}
	}

	void GraphGameplayLibrary::SetAnimationIKEnabled(uint64 entityId, bool enabled)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return;
		SetAnimationIKEnabled(*scene, entityId, enabled);
	}

	void GraphGameplayLibrary::SetAnimationIKEnabled(Scene& scene, uint64 entityId, bool enabled)
	{
		auto& reg = scene.GetRegistry();
		const entt::entity target = ResolveTargetEntity(reg, entityId);
		if (auto* ik = FindTargetIKComponent(reg, target))
		{
			ik->enabled = enabled;
		}
	}

	void GraphGameplayLibrary::SetTwoBoneIKTarget(uint64 entityId, int32 chainIndex, vec3 targetPos, float weight)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return;
		SetTwoBoneIKTarget(*scene, entityId, chainIndex, targetPos, weight);
	}

	void GraphGameplayLibrary::SetTwoBoneIKTarget(Scene& scene, uint64 entityId, int32 chainIndex, vec3 targetPos, float weight)
	{
		auto& reg = scene.GetRegistry();
		const entt::entity target = ResolveTargetEntity(reg, entityId);
		if (auto* ik = FindTargetIKComponent(reg, target))
		{
			if (chainIndex >= 0 && static_cast<size_t>(chainIndex) < ik->twoBoneChains.size())
			{
				ik->twoBoneChains[chainIndex].targetPosition = targetPos;
				ik->twoBoneChains[chainIndex].weight = weight;
				ik->twoBoneChains[chainIndex].targetInWorldSpace = true;
			}
		}
	}

	void GraphGameplayLibrary::SetLookAtIKTarget(uint64 entityId, int32 targetIndex, vec3 targetPos, float weight)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return;
		SetLookAtIKTarget(*scene, entityId, targetIndex, targetPos, weight);
	}

	void GraphGameplayLibrary::SetLookAtIKTarget(Scene& scene, uint64 entityId, int32 targetIndex, vec3 targetPos, float weight)
	{
		auto& reg = scene.GetRegistry();
		const entt::entity target = ResolveTargetEntity(reg, entityId);
		if (auto* ik = FindTargetIKComponent(reg, target))
		{
			if (targetIndex >= 0 && static_cast<size_t>(targetIndex) < ik->lookAtTargets.size())
			{
				ik->lookAtTargets[targetIndex].targetPosition = targetPos;
				ik->lookAtTargets[targetIndex].weight = weight;
				ik->lookAtTargets[targetIndex].targetInWorldSpace = true;
			}
		}
	}

	void GraphGameplayLibrary::SetFootGroundIKEnabled(uint64 entityId, bool enabled, float footHeightOffset)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return;
		SetFootGroundIKEnabled(*scene, entityId, enabled, footHeightOffset);
	}

	void GraphGameplayLibrary::SetFootGroundIKEnabled(Scene& scene, uint64 entityId, bool enabled, float footHeightOffset)
	{
		auto& reg = scene.GetRegistry();
		const entt::entity target = ResolveTargetEntity(reg, entityId);
		if (auto* ik = FindTargetIKComponent(reg, target))
		{
			ik->enableFootGroundIK = enabled;
			if (footHeightOffset >= 0.0f)
			{
				ik->footHeightOffset = footHeightOffset;
			}
		}
	}

	// ── SaveGame ──
	bool GraphGameplayLibrary::WriteCheckpointSlot(const std::string& slotName, const std::string& checkpointId)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		const entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return false;
		return WriteCheckpointSlot(*scene, entity, slotName, checkpointId);
	}

	bool GraphGameplayLibrary::WriteCheckpointSlot(Scene& scene, entt::entity entity, const std::string& slotName, const std::string& checkpointId)
	{
		if (slotName.empty()) return false;

		SaveGameData data;
		const std::string sceneUri = Engine::Unicode::WideToUtf8(scene.GetName());

		if (!SaveGameManager::CaptureCheckpointFromEntity(scene, entity, sceneUri, checkpointId, data))
		{
			return false;
		}

		return SaveGameManager::Get().WriteSlot(slotName, data);
	}

	bool GraphGameplayLibrary::LoadCheckpointSlot(const std::string& slotName)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		const entt::entity entity = GraphSystem::GetCurrentEntity();
		if (!scene || entity == entt::null) return false;
		return LoadCheckpointSlot(*scene, entity, slotName);
	}

	bool GraphGameplayLibrary::LoadCheckpointSlot(Scene& scene, entt::entity entity, const std::string& slotName)
	{
		if (slotName.empty()) return false;
		SaveGameData data;
		if (!SaveGameManager::Get().ReadSlot(slotName, data))
		{
			return false;
		}
		return SaveGameManager::ApplyCheckpointToEntity(scene, entity, data);
	}

	bool GraphGameplayLibrary::HasSlot(const std::string& slotName)
	{
		if (slotName.empty()) return false;
		return SaveGameManager::Get().HasSlot(slotName);
	}

	bool GraphGameplayLibrary::DeleteSlot(const std::string& slotName)
	{
		if (slotName.empty()) return false;
		return SaveGameManager::Get().DeleteSlot(slotName);
	}

	// ── Navigation & AI ──
	void GraphGameplayLibrary::SetNavDestination(int64 entityId, float x, float y, float z)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return;
		auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null) return;
		SetNavDestination(*scene, target, vec3(x, y, z));
	}

	void GraphGameplayLibrary::SetNavDestination(Scene& scene, entt::entity entity, const vec3& dest)
	{
		auto& reg = scene.GetRegistry();
		if (entity == entt::null || !reg.valid(entity)) return;
		auto* agent = TryGetEnabledComponent<NavAgentComponent>(reg, entity);
		if (!agent) return;
		agent->SetDestination(dest);
	}

	void GraphGameplayLibrary::StopNav(int64 entityId)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return;
		auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null) return;
		StopNav(*scene, target);
	}

	void GraphGameplayLibrary::StopNav(Scene& scene, entt::entity entity)
	{
		auto& reg = scene.GetRegistry();
		if (entity == entt::null || !reg.valid(entity)) return;
		auto* agent = TryGetEnabledComponent<NavAgentComponent>(reg, entity);
		if (!agent) return;
		agent->Stop();
	}

	bool GraphGameplayLibrary::HasReachedNavDestination(int64 entityId)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return false;
		const auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null) return false;
		return HasReachedNavDestination(*scene, target);
	}

	bool GraphGameplayLibrary::HasReachedNavDestination(const Scene& scene, entt::entity entity)
	{
		const auto& reg = scene.GetRegistry();
		if (entity == entt::null || !reg.valid(entity)) return false;
		const auto* agent = TryGetEnabledComponent<NavAgentComponent>(reg, entity);
		if (!agent) return false;
		return agent->reachedDestination;
	}

	int64 GraphGameplayLibrary::GetAIDetectedTarget(int64 entityId)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return -1;
		const auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null) return -1;
		return GetAIDetectedTarget(*scene, target);
	}

	int64 GraphGameplayLibrary::GetAIDetectedTarget(const Scene& scene, entt::entity entity)
	{
		const auto& reg = scene.GetRegistry();
		if (entity == entt::null || !reg.valid(entity)) return -1;
		const auto* perception = TryGetEnabledComponent<AIPerceptionComponent>(reg, entity);
		if (!perception || perception->detectedTarget == entt::null || !reg.valid(perception->detectedTarget))
		{
			return -1;
		}
		return static_cast<int64>(static_cast<uint32>(perception->detectedTarget));
	}

	bool GraphGameplayLibrary::HasAILineOfSight(int64 entityId)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return false;
		const auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null) return false;
		return HasAILineOfSight(*scene, target);
	}

	bool GraphGameplayLibrary::HasAILineOfSight(const Scene& scene, entt::entity entity)
	{
		const auto& reg = scene.GetRegistry();
		if (entity == entt::null || !reg.valid(entity)) return false;
		const auto* perception = TryGetEnabledComponent<AIPerceptionComponent>(reg, entity);
		if (!perception) return false;
		return perception->hasLineOfSight;
	}

	// ── AudioMixer ──
	void GraphGameplayLibrary::SetBusVolume(const std::string& busName, float volume)
	{
		if (!AudioManager::IsCreated()) return;
		const auto busOpt = AudioMixer::ParseBusName(busName);
		if (!busOpt.has_value()) return;
		AudioManager::Get().GetMixer().SetBusVolume(busOpt.value(), volume);
	}

	float GraphGameplayLibrary::GetBusVolume(const std::string& busName)
	{
		if (!AudioManager::IsCreated()) return 0.0f;
		const auto busOpt = AudioMixer::ParseBusName(busName);
		if (!busOpt.has_value()) return 0.0f;
		return AudioManager::Get().GetMixer().GetBusVolume(busOpt.value());
	}

	void GraphGameplayLibrary::SetBusMuted(const std::string& busName, bool muted)
	{
		if (!AudioManager::IsCreated()) return;
		const auto busOpt = AudioMixer::ParseBusName(busName);
		if (!busOpt.has_value()) return;
		AudioManager::Get().GetMixer().SetBusMuted(busOpt.value(), muted);
	}

	void GraphGameplayLibrary::FadeBusVolume(const std::string& busName, float targetVolume, float duration)
	{
		if (!AudioManager::IsCreated()) return;
		const auto busOpt = AudioMixer::ParseBusName(busName);
		if (!busOpt.has_value()) return;
		AudioManager::Get().GetMixer().FadeBusVolume(busOpt.value(), targetVolume, duration);
	}

	// ── SceneStreaming ──
	void GraphGameplayLibrary::MarkDontDestroyOnLoad(int64 entityId, const std::string& group)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene || !SceneManager::IsCreated()) return;
		auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null) return;
		MarkDontDestroyOnLoad(*scene, target, group);
	}

	void GraphGameplayLibrary::MarkDontDestroyOnLoad(Scene& scene, entt::entity entity, const std::string& group)
	{
		if (!SceneManager::IsCreated() || entity == entt::null) return;
		(void)SceneManager::Get().MarkDontDestroyOnLoad(scene, entity, group);
	}

	bool GraphGameplayLibrary::UnloadAdditiveScene(const std::string& subSceneName)
	{
		if (!SceneManager::IsCreated() || subSceneName.empty()) return false;
		const EResult result = SceneManager::Get().UnloadAdditiveScene(subSceneName);
		return (result == EResult::Success);
	}

	// ── Network RPC & Diagnostics ──
	namespace
	{
		bool DispatchGraphRpc(int64 netId, const std::string& rpcName, const std::string& payloadText, Network::ERpcTarget target)
		{
#if !defined(__EMSCRIPTEN__)
			if (netId <= 0 || rpcName.empty() || !SystemManager::IsCreated()) return false;
			auto* netSystem = SystemManager::Get().GetSystem<NetworkSystem>();
			if (!netSystem) return false;

			Network::RpcMessage msg{};
			msg.netId = static_cast<Network::NetId>(netId);
			msg.rpcNameHash = Network::HashRpcName(rpcName);
			msg.target = target;
			msg.senderClientId = netSystem->GetDriver().GetLocalClientId();
			msg.payload.assign(payloadText.begin(), payloadText.end());
			return netSystem->SendRpc(msg);
#else
			(void)netId;
			(void)rpcName;
			(void)payloadText;
			(void)target;
			return false;
#endif
		}
	}

	bool GraphGameplayLibrary::SendServerRpc(int64 netId, const std::string& rpcName, const std::string& payloadText)
	{
		return DispatchGraphRpc(netId, rpcName, payloadText, Network::ERpcTarget::Server);
	}

	bool GraphGameplayLibrary::SendMulticastRpc(int64 netId, const std::string& rpcName, const std::string& payloadText)
	{
		return DispatchGraphRpc(netId, rpcName, payloadText, Network::ERpcTarget::Multicast);
	}

	bool GraphGameplayLibrary::IsNetworkServer()
	{
#if !defined(__EMSCRIPTEN__)
		if (!SystemManager::IsCreated()) return false;
		const auto* netSystem = SystemManager::Get().GetSystem<NetworkSystem>();
		return netSystem && netSystem->GetDriver().IsServer();
#else
		return false;
#endif
	}

	bool GraphGameplayLibrary::IsNetworkClient()
	{
#if !defined(__EMSCRIPTEN__)
		if (!SystemManager::IsCreated()) return false;
		const auto* netSystem = SystemManager::Get().GetSystem<NetworkSystem>();
		return netSystem && netSystem->GetDriver().IsClient();
#else
		return false;
#endif
	}

	float GraphGameplayLibrary::GetNetworkSimulatedLatencyMs()
	{
#if !defined(__EMSCRIPTEN__)
		if (!SystemManager::IsCreated()) return 0.0f;
		const auto* netSystem = SystemManager::Get().GetSystem<NetworkSystem>();
		return netSystem ? netSystem->GetDriver().GetSimSettings().simulatedLatencyMs : 0.0f;
#else
		return 0.0f;
#endif
	}

	// ── Camera Sequencer (ActionTimeline) ──
	bool GraphGameplayLibrary::PlayCameraSequence(int64 entityId, float startTime)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene || !scene->IsActive() || scene->IsPaused()) return false;
		auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null) return false;
		auto* timeline = TryGetEnabledComponent<ActionTimelineComponent>(reg, target);
		if (!timeline) return false;
		timeline->Play();
		if (startTime > 0.0f)
		{
			timeline->currentTime = startTime;
			timeline->prevTime = startTime;
		}
		return true;
	}

	void GraphGameplayLibrary::StopCameraSequence(int64 entityId)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return;
		auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null) return;
		if (auto* timeline = TryGetEnabledComponent<ActionTimelineComponent>(reg, target))
		{
			timeline->Stop();
		}
	}

	bool GraphGameplayLibrary::IsCameraSequencePlaying(int64 entityId)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene) return false;
		const auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null) return false;
		const auto* timeline = TryGetEnabledComponent<ActionTimelineComponent>(reg, target);
		return timeline && timeline->isPlaying;
	}

	// ── PakArchive Mount ──
	bool GraphGameplayLibrary::IsPakMounted()
	{
		return PakMountManager::Get().GetMountedPakCount() > 0;
	}

	bool GraphGameplayLibrary::HasPakEntry(const std::string& uri)
	{
		if (uri.empty()) return false;
		return PakMountManager::Get().HasEntry(uri);
	}

	// ── SceneTransition ──
	bool GraphGameplayLibrary::TransitionToScene(const std::string& scenePath, f32 fadeOutDuration, f32 fadeInDuration)
	{
		if (scenePath.empty()) return false;
		if (!SceneTransitionManager::IsCreated())
		{
			if (!SceneTransitionManager::Create()) return false;
		}
		return SceneTransitionManager::Get().TransitionToScene(
			std::filesystem::path(scenePath), fadeOutDuration, fadeInDuration) == EResult::Success;
	}

	bool GraphGameplayLibrary::IsSceneTransitioning()
	{
		if (!SceneTransitionManager::IsCreated()) return false;
		return SceneTransitionManager::Get().IsTransitioning();
	}

	// ── Audio BGM ──
	bool GraphGameplayLibrary::CrossfadeBGMByPath(const std::string& audioPath, f32 fadeDuration, f32 volume)
	{
		if (audioPath.empty() || !AudioManager::IsCreated() || !ResourceManager::IsCreated()) return false;

		const auto handle = ResourceManager::Get().LoadFile(audioPath);
		if (!handle.IsValid()) return false;

		auto* clip = dynamic_cast<AudioClip*>(ResourceManager::Get().GetResource(handle));
		if (!clip) return false;

		auto* sound = clip->GetFMODSound();
		if (!sound) return false;

		const f32 clampedVol = std::isfinite(volume) ? std::clamp(volume, 0.0f, 1.0f) : 1.0f;
		const f32 safeFade = (std::isfinite(fadeDuration) && fadeDuration > 0.0f) ? fadeDuration : 0.0f;
		return AudioManager::Get().CrossfadeBGM(sound, safeFade, clampedVol, true) == EResult::Success;
	}

	void GraphGameplayLibrary::StopBGM()
	{
		if (!AudioManager::IsCreated()) return;
		AudioManager::Get().StopBGM();
	}

	bool GraphGameplayLibrary::IsBGMPlaying()
	{
		if (!AudioManager::IsCreated()) return false;
		return AudioManager::Get().IsBGMPlaying();
	}

	bool GraphGameplayLibrary::IsBGMCrossfading()
	{
		if (!AudioManager::IsCreated()) return false;
		return AudioManager::Get().IsCrossfading();
	}

	// ── Gamepad ──
	bool GraphGameplayLibrary::SetGamepadRumble(f32 lowFreq, f32 highFreq, int32 durationMs, int32 gamepadIndex)
	{
		if (!InputManager::IsCreated() || durationMs < 0 || gamepadIndex < 0) return false;
		return InputManager::Get().SetGamepadRumble(
			lowFreq, highFreq, static_cast<uint32>(durationMs), static_cast<uint32>(gamepadIndex));
	}

	// ── DataTable / Combat / Quest ──
	namespace
	{
		std::mutex& GetGameplayCacheMutex()
		{
			static std::mutex s_mutex;
			return s_mutex;
		}

		std::unordered_map<std::string, DataTable>& GetDataTableCache()
		{
			static std::unordered_map<std::string, DataTable> s_tables;
			return s_tables;
		}

		QuestTracker& GetQuestTracker()
		{
			static QuestTracker s_questTracker;
			return s_questTracker;
		}

		InventoryContainer& GetInventoryContainer()
		{
			static InventoryContainer s_inventory;
			return s_inventory;
		}

		uint64 HashFnv1a64(std::string_view key) noexcept
		{
			uint64 hash = 14695981039346656037ULL;
			for (const unsigned char ch : key)
			{
				hash ^= static_cast<uint64>(ch);
				hash *= 1099511628211ULL;
			}
			return hash;
		}

		uint32 ClampPositiveToUint32(int64 value) noexcept
		{
			constexpr int64 kMaxUint32 = static_cast<int64>(std::numeric_limits<uint32>::max());
			return (value > kMaxUint32) ? std::numeric_limits<uint32>::max() : static_cast<uint32>(value);
		}
	}

	bool GraphGameplayLibrary::LoadDataTableCsv(const std::string& tableId, const std::string& filePath, const std::string& keyColumn)
	{
		if (tableId.empty() || filePath.empty()) return false;
		const std::filesystem::path resolvedPath = PlatformPath::ResolveUri(filePath);
		if (resolvedPath.empty()) return false;

		DataTable table;
		std::string error;
		if (!table.LoadFromCsvFile(resolvedPath, &error, keyColumn))
		{
			return false;
		}

		std::lock_guard<std::mutex> lock(GetGameplayCacheMutex());
		GetDataTableCache()[tableId] = std::move(table);
		return true;
	}

	std::string GraphGameplayLibrary::GetDataTableString(const std::string& tableId, const std::string& key, const std::string& column, const std::string& defaultValue)
	{
		if (tableId.empty() || key.empty() || column.empty()) return defaultValue;
		std::lock_guard<std::mutex> lock(GetGameplayCacheMutex());
		const auto& cache = GetDataTableCache();
		const auto it = cache.find(tableId);
		if (it == cache.end()) return defaultValue;

		const auto rowOpt = it->second.FindRowIndexByKey(key);
		if (!rowOpt.has_value()) return defaultValue;
		return it->second.GetString(*rowOpt, column, defaultValue);
	}

	int64 GraphGameplayLibrary::GetDataTableInt(const std::string& tableId, const std::string& key, const std::string& column, int64 defaultValue)
	{
		if (tableId.empty() || key.empty() || column.empty()) return defaultValue;
		std::lock_guard<std::mutex> lock(GetGameplayCacheMutex());
		const auto& cache = GetDataTableCache();
		const auto it = cache.find(tableId);
		if (it == cache.end()) return defaultValue;

		const auto rowOpt = it->second.FindRowIndexByKey(key);
		if (!rowOpt.has_value()) return defaultValue;

		if (defaultValue >= std::numeric_limits<int32>::min() && defaultValue <= std::numeric_limits<int32>::max())
		{
			return static_cast<int64>(it->second.GetInt(*rowOpt, column, static_cast<int32>(defaultValue)));
		}

		const int32 probeZero = it->second.GetInt(*rowOpt, column, 0);
		const int32 probeOne = it->second.GetInt(*rowOpt, column, 1);
		return (probeZero == probeOne) ? static_cast<int64>(probeZero) : defaultValue;
	}

	float GraphGameplayLibrary::GetDataTableFloat(const std::string& tableId, const std::string& key, const std::string& column, float defaultValue)
	{
		if (tableId.empty() || key.empty() || column.empty()) return defaultValue;
		std::lock_guard<std::mutex> lock(GetGameplayCacheMutex());
		const auto& cache = GetDataTableCache();
		const auto it = cache.find(tableId);
		if (it == cache.end()) return defaultValue;

		const auto rowOpt = it->second.FindRowIndexByKey(key);
		if (!rowOpt.has_value()) return defaultValue;
		return it->second.GetFloat(*rowOpt, column, defaultValue);
	}

	bool GraphGameplayLibrary::GetDataTableBool(const std::string& tableId, const std::string& key, const std::string& column, bool defaultValue)
	{
		if (tableId.empty() || key.empty() || column.empty()) return defaultValue;
		std::lock_guard<std::mutex> lock(GetGameplayCacheMutex());
		const auto& cache = GetDataTableCache();
		const auto it = cache.find(tableId);
		if (it == cache.end()) return defaultValue;

		const auto rowOpt = it->second.FindRowIndexByKey(key);
		if (!rowOpt.has_value()) return defaultValue;
		return it->second.GetBool(*rowOpt, column, defaultValue);
	}

	void GraphGameplayLibrary::TriggerHitstop(float duration, float timeScale)
	{
		CombatEffectService::TriggerHitstop(duration, timeScale);
	}

	bool GraphGameplayLibrary::IsHitstopActive()
	{
		return CombatEffectService::IsHitstopActive();
	}

	void GraphGameplayLibrary::TriggerCameraShake(float intensity, float duration, float frequency)
	{
		CombatEffectService::TriggerCameraShake(intensity, duration, frequency);
	}

	bool GraphGameplayLibrary::IsCameraShakeActive()
	{
		return CombatEffectService::IsCameraShakeActive();
	}

	float GraphGameplayLibrary::UpdateCombatEffects(float unscaledDt)
	{
		return CombatEffectService::Update(unscaledDt);
	}

	vec3 GraphGameplayLibrary::GetCameraShakePositionOffset()
	{
		return CombatEffectService::GetCameraShakePositionOffset();
	}

	vec3 GraphGameplayLibrary::GetCameraShakeRotationOffsetDeg()
	{
		return CombatEffectService::GetCameraShakeRotationOffsetDeg();
	}

	bool GraphGameplayLibrary::StartQuest(const std::string& questId, int64 targetCount)
	{
		if (questId.empty() || targetCount < 0 || targetCount > std::numeric_limits<uint32>::max())
		{
			return false;
		}
		std::lock_guard<std::mutex> lock(GetGameplayCacheMutex());
		return GetQuestTracker().StartQuest(HashFnv1a64(questId), static_cast<uint32>(targetCount));
	}

	bool GraphGameplayLibrary::AdvanceQuest(const std::string& questId, int64 delta)
	{
		if (questId.empty() || delta <= 0 || delta > std::numeric_limits<uint32>::max())
		{
			return false;
		}
		std::lock_guard<std::mutex> lock(GetGameplayCacheMutex());
		return GetQuestTracker().AdvanceQuest(HashFnv1a64(questId), static_cast<uint32>(delta));
	}

	bool GraphGameplayLibrary::FailQuest(const std::string& questId)
	{
		if (questId.empty()) return false;
		std::lock_guard<std::mutex> lock(GetGameplayCacheMutex());
		return GetQuestTracker().FailQuest(HashFnv1a64(questId));
	}

	bool GraphGameplayLibrary::IsQuestCompleted(const std::string& questId)
	{
		if (questId.empty()) return false;
		std::lock_guard<std::mutex> lock(GetGameplayCacheMutex());
		return GetQuestTracker().GetQuestState(HashFnv1a64(questId)) == QuestState::Completed;
	}

	// ── UI Tween & Projection ──
	namespace
	{
		const CameraComponent* FindActiveMainCamera(const entt::registry& reg)
		{
			const CameraComponent* fallback = nullptr;
			for (const entt::entity camEntity : reg.view<const CameraComponent>())
			{
				if (!IsComponentEnabled<CameraComponent>(reg, camEntity))
					continue;
				if (const auto* flag = reg.try_get<FlagComponent>(camEntity);
					flag && (!flag->IsActiveInHierarchy() || HasFlag(flag->effectiveFlags, EEntityFlag::Dead)))
					continue;

				const auto& cam = reg.get<const CameraComponent>(camEntity);
				if (!cam.GetCameraBuffer().has_value())
					continue;

				if (cam.IsMainCamera)
					return &cam;
				if (!fallback)
					fallback = &cam;
			}
			return fallback;
		}
	}

	bool GraphGameplayLibrary::UIPunchScale(int64 entityId, float peakScale, float duration)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene || !std::isfinite(peakScale) || !std::isfinite(duration) || duration < 0.0f)
			return false;

		auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null)
			return false;

		vec2 baseScale(1.0f);
		if (const auto* rect = reg.try_get<RectTransformComponent>(target))
			baseScale = rect->scale;

		return UITweenSystem::PunchScale(reg, target, baseScale, baseScale * peakScale, duration) != 0;
	}

	bool GraphGameplayLibrary::UIFadeAlpha(int64 entityId, float fromAlpha, float toAlpha, float duration)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene || !std::isfinite(fromAlpha) || !std::isfinite(toAlpha) || !std::isfinite(duration) || duration < 0.0f)
			return false;

		auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null)
			return false;

		const float clampedFrom = std::clamp(fromAlpha, 0.0f, 1.0f);
		const float clampedTo = std::clamp(toAlpha, 0.0f, 1.0f);
		return UITweenSystem::PlayAlpha(reg, target, clampedFrom, clampedTo, duration) != 0;
	}

	bool GraphGameplayLibrary::UIMoveOffset(int64 entityId, vec2 fromOffset, vec2 toOffset, float duration)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene || !std::isfinite(fromOffset.x) || !std::isfinite(fromOffset.y) ||
			!std::isfinite(toOffset.x) || !std::isfinite(toOffset.y) ||
			!std::isfinite(duration) || duration < 0.0f)
		{
			return false;
		}

		auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null)
			return false;

		return UITweenSystem::PlayOffset(reg, target, fromOffset, toOffset, duration) != 0;
	}

	void GraphGameplayLibrary::UIStopTweens(int64 entityId)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene)
			return;

		auto& reg = scene->GetRegistry();
		const entt::entity target = GraphGameplay::Detail::ResolveGameplayTargetEntity(reg, entityId);
		if (target == entt::null)
			return;

		UITweenSystem::StopAll(reg, target);
	}

	vec2 GraphGameplayLibrary::UIWorldToScreen(vec3 worldPos, vec2 viewportSize)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene)
			return vec2(0.0f);

		const CameraComponent* cam = FindActiveMainCamera(scene->GetRegistry());
		if (!cam)
			return vec2(0.0f);

		const UIWorldToScreenResult res = UIProjection::WorldToScreen(worldPos, *cam, viewportSize);
		return res.isInFront ? res.screenPosition : vec2(0.0f);
	}

	bool GraphGameplayLibrary::UIIsWorldPointInFront(vec3 worldPos, vec2 viewportSize)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene)
			return false;

		const CameraComponent* cam = FindActiveMainCamera(scene->GetRegistry());
		if (!cam)
			return false;

		const UIWorldToScreenResult res = UIProjection::WorldToScreen(worldPos, *cam, viewportSize);
		return res.isInFront;
	}

	// ── Inventory ──
	int64 GraphGameplayLibrary::AddInventoryItem(const std::string& itemId, int64 count, int64 maxStack)
	{
		if (itemId.empty() || count <= 0 || maxStack <= 0)
			return 0;

		std::lock_guard<std::mutex> lock(GetGameplayCacheMutex());
		return static_cast<int64>(GetInventoryContainer().AddItem(
			HashFnv1a64(itemId), ClampPositiveToUint32(count), ClampPositiveToUint32(maxStack)));
	}

	int64 GraphGameplayLibrary::RemoveInventoryItem(const std::string& itemId, int64 count)
	{
		if (itemId.empty() || count <= 0)
			return 0;

		std::lock_guard<std::mutex> lock(GetGameplayCacheMutex());
		return static_cast<int64>(GetInventoryContainer().RemoveItem(
			HashFnv1a64(itemId), ClampPositiveToUint32(count)));
	}

	int64 GraphGameplayLibrary::GetInventoryItemCount(const std::string& itemId)
	{
		if (itemId.empty())
			return 0;

		std::lock_guard<std::mutex> lock(GetGameplayCacheMutex());
		return static_cast<int64>(GetInventoryContainer().GetItemCount(HashFnv1a64(itemId)));
	}

	void GraphGameplayLibrary::ClearInventory()
	{
		std::lock_guard<std::mutex> lock(GetGameplayCacheMutex());
		GetInventoryContainer().Clear();
	}

	// ── NavGrid Dynamic Obstacles ──
	int64 GraphGameplayLibrary::SetNavGridObstacleBox(const vec3& minBounds, const vec3& maxBounds, bool addObstacle)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene ||
			!std::isfinite(minBounds.x) || !std::isfinite(minBounds.y) || !std::isfinite(minBounds.z) ||
			!std::isfinite(maxBounds.x) || !std::isfinite(maxBounds.y) || !std::isfinite(maxBounds.z))
		{
			return 0;
		}

		const std::shared_ptr<NavGrid> grid = NavigationSystem::GetSceneNavGrid(scene);
		if (!grid)
			return 0;

		return static_cast<int64>(grid->SetDynamicObstacleBox(minBounds, maxBounds, addObstacle));
	}

	int64 GraphGameplayLibrary::SetNavGridObstacleCircle(const vec3& center, float radius, bool addObstacle)
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene ||
			!std::isfinite(center.x) || !std::isfinite(center.y) || !std::isfinite(center.z) ||
			!std::isfinite(radius) || radius < 0.0f)
		{
			return 0;
		}

		const std::shared_ptr<NavGrid> grid = NavigationSystem::GetSceneNavGrid(scene);
		if (!grid)
			return 0;

		return static_cast<int64>(grid->SetDynamicObstacleCircle(center, radius, addObstacle));
	}

	void GraphGameplayLibrary::ClearNavGridObstacles()
	{
		Scene* scene = GraphSystem::GetCurrentScene();
		if (!scene)
			return;

		const std::shared_ptr<NavGrid> grid = NavigationSystem::GetSceneNavGrid(scene);
		if (!grid)
			return;

		grid->ClearDynamicObstacles();
	}

	void GraphGameplayLibrary::ResetSessionState()
	{
		{
			std::lock_guard<std::mutex> lock(GetGameplayCacheMutex());
			GetDataTableCache().clear();
			GetQuestTracker().Clear();
			GetInventoryContainer().Clear();
		}
		CombatEffectService::Reset();
	}
}
