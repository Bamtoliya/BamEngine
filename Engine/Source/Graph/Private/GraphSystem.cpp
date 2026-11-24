#include "GraphSystem.h"
#include "ComponentEnable.h"
#include "GraphFile.h"
#include "GraphReflection.h"
#include "ReflectionRegistry.h"
#include "GraphBindingComponent.h"
#include "Scene.h"
#include "Logger.h"
#include "PakArchive.h"
#include "PlatformPath.h"
#include "InputManager.h"
#include "ActionTimelineComponent.h"
#include "ActionTimelineEvents.h"
#include "FlagComponent.h"
#include <chrono>
#include <utility>
#if BAM_EDITOR
#include <string_view>
#include <unordered_set>
#endif

namespace
{
	using Clock = std::chrono::steady_clock;
	using Milliseconds = std::chrono::duration<double, std::milli>;

	Scene* s_CurrentScene = nullptr;
	entt::entity s_CurrentEntity = entt::null;
	float s_CurrentDeltaTime = 0.0f;
	bool s_CurrentEntityDestroyRequested = false;
#if BAM_EDITOR
	GraphSystem::NodeExecuteHook s_NodeExecuteHook = nullptr;

	void NotifyCurrentGraphNode(GraphNodeId node)
	{
		if (!s_NodeExecuteHook || !s_CurrentScene || s_CurrentEntity == entt::null) return;
		const auto* runtime = s_CurrentScene->GetRegistry().try_get<GraphRuntimeState>(s_CurrentEntity);
		if (runtime) s_NodeExecuteHook(runtime->sourcePath, node);
	}
#endif

	std::vector<GraphRuntimeState::ActionEventEntry> CollectActionEventEntries(const Graph& graph, const GraphRegistry& registry)
	{
		using Phase = GraphRuntimeState::ActionEventPhase;
		std::vector<GraphRuntimeState::ActionEventEntry> entries;

		for (const auto& node : graph.GetNodes())
		{
			const bool released = node.Type == "Event.OnActionReleased";
			if (!released && node.Type != "Event.OnActionPressed")
				continue;

			const auto readText = [&](const char* key) -> std::string
				{
					const auto found = node.InputValues.find(key);
					if (found != node.InputValues.end())
						return std::get<std::string>(found->second);

					const auto* pin = registry.FindPin(node.Type, key);
					return pin ? std::get<std::string>(pin->DefaultValue) : std::string{};
				};

			auto mapId = readText("mapId");
			auto actionId = readText("actionId");
			if (!mapId.empty() && !actionId.empty())
			{
				const auto phase = released ? Phase::Released : Phase::Pressed;
				entries.push_back({ node.Id, std::move(mapId), std::move(actionId), phase });
			}
		}

		std::stable_partition(entries.begin(), entries.end(), [](const auto& entry)
			{
				return entry.Phase == Phase::Pressed;
			});

		return entries;
	}

	std::unordered_map<std::string, std::vector<GraphNodeId>> CollectTimelineEventEntries(const Graph& graph)
	{
		std::unordered_map<std::string, std::vector<GraphNodeId>> entries;
		for (const auto& node : graph.GetNodes())
		{
			if (node.Type != "Event.OnActionTimelineEvent") continue;
			const auto input = node.InputValues.find("eventTag");
			if (input == node.InputValues.end()) continue;
			const auto* tag = std::get_if<std::string>(&input->second);
			if (tag && !tag->empty()) entries[*tag].push_back(node.Id);
		}
		return entries;
	}

	std::vector<GraphNodeId> CollectTimelineEndEntries(const Graph& graph)
	{
		std::vector<GraphNodeId> entries;
		for (const auto& node : graph.GetNodes())
			if (node.Type == "Event.OnActionTimelineEnded") entries.push_back(node.Id);
		return entries;
	}

	struct PendingTimelineEvents
	{
		std::wstring graphPath;
		uint64 sceneActivityRevision = 0;
		uint32 playSerial = 0;
		std::vector<std::string> tags;
	};

	using SceneTimelineEvents = std::unordered_map<entt::entity, PendingTimelineEvents>;

	bool IsTimelineEventCurrent(entt::registry& world, entt::entity entity, uint32 playSerial)
	{
		const auto* timeline = TryGetEnabledComponent<ActionTimelineComponent>(world, entity);
		if (!timeline || timeline->playSerial != playSerial) return false;
		const auto* flag = world.try_get<FlagComponent>(entity);
		return !flag || (flag->IsActiveInHierarchy() && !HasFlag(flag->effectiveFlags, EEntityFlag::Dead) &&
			!HasFlag(flag->effectiveFlags, EEntityFlag::Paused));
	}

	void QueueTimelineEvent(const ActionTimelineEvent& event)
	{
		if (!event.scene || !event.scene->IsActive() || event.scene->IsPaused() || event.eventTag.empty()) return;
		auto& world = event.scene->GetRegistry();
		if (!IsTimelineEventCurrent(world, event.entity, event.playSerial)) return;
		const auto* binding = TryGetEnabledComponent<GraphBindingComponent>(world, event.entity);
		if (!binding || binding->graphFile.empty()) return;

		auto& pending = world.ctx().emplace<SceneTimelineEvents>()[event.entity];
		const auto activityRevision = event.scene->GetActivityRevision();
		if (pending.graphPath != binding->graphFile || pending.sceneActivityRevision != activityRevision ||
			pending.playSerial != event.playSerial)
		{
			pending.graphPath = binding->graphFile;
			pending.sceneActivityRevision = activityRevision;
			pending.playSerial = event.playSerial;
			pending.tags.clear();
		}
		pending.tags.push_back(event.eventTag);
	}

	// 재생 종료는 끝난 세션의 알림이라 현재 세션 번호와 비교하지 않는다. 대신 그래프 파일과 씬 활성 회차가 그대로일 때만 전달한다.
	struct PendingTimelineEnds
	{
		std::wstring graphPath;
		uint64 sceneActivityRevision = 0;
		std::vector<EActionTimelineEndReason> reasons;
	};

	using SceneTimelineEnds = std::unordered_map<entt::entity, PendingTimelineEnds>;

	bool CanReceiveTimelineEnd(entt::registry& world, entt::entity entity)
	{
		if (!world.valid(entity) || !world.all_of<ActionTimelineComponent>(entity)) return false;
		const auto* flag = world.try_get<FlagComponent>(entity);
		return !flag || (flag->IsActiveInHierarchy() && !HasFlag(flag->effectiveFlags, EEntityFlag::Dead));
	}

	void QueueTimelineEnd(const ActionTimelineEndEvent& event)
	{
		if (!event.scene || !event.scene->IsActive() || event.scene->IsPaused()) return;
		auto& world = event.scene->GetRegistry();
		if (!CanReceiveTimelineEnd(world, event.entity)) return;
		const auto* binding = TryGetEnabledComponent<GraphBindingComponent>(world, event.entity);
		if (!binding || binding->graphFile.empty()) return;

		auto& pending = world.ctx().emplace<SceneTimelineEnds>()[event.entity];
		const auto activityRevision = event.scene->GetActivityRevision();
		if (pending.graphPath != binding->graphFile || pending.sceneActivityRevision != activityRevision)
		{
			pending.graphPath = binding->graphFile;
			pending.sceneActivityRevision = activityRevision;
			pending.reasons.clear();
		}
		pending.reasons.push_back(event.reason);
	}

	void DispatchTimelineEnds(Scene& scene, entt::entity entity, GraphRuntimeState& runtime, const PendingTimelineEnds& pending)
	{
		if (pending.graphPath != runtime.loadedPath) return;
		auto& world = scene.GetRegistry();
		for (const auto reason : pending.reasons)
		{
			for (const auto node : runtime.timelineEndEntries)
			{
				const auto* binding = TryGetEnabledComponent<GraphBindingComponent>(world, entity);
				if (s_CurrentEntityDestroyRequested || !binding || binding->graphFile != pending.graphPath ||
					scene.GetActivityRevision() != pending.sceneActivityRevision || !CanReceiveTimelineEnd(world, entity)) return;
				if (!runtime.executor.BeginTimelineEnded(node, static_cast<int64_t>(reason)).Succeeded()) continue;
				runtime.executor.Resume();
				while (runtime.executor.GetState() == EGraphExecutionState::Running)
					runtime.executor.Tick();
				if (runtime.executor.GetDiagnostic().Code == EGraphError::ExecutionLimit)
				{
					runtime.isValid = false;
					return;
				}
			}
		}
	}

	void DispatchTimelineEvents(Scene& scene, entt::entity entity, GraphRuntimeState& runtime,
		const PendingTimelineEvents& pending, float dt)
	{
		if (pending.graphPath != runtime.loadedPath) return;
		auto& world = scene.GetRegistry();
		for (const auto& tag : pending.tags)
		{
			const auto entries = runtime.timelineEventEntries.find(tag);
			if (entries == runtime.timelineEventEntries.end()) continue;
			for (const auto node : entries->second)
			{
				const auto* binding = TryGetEnabledComponent<GraphBindingComponent>(world, entity);
				if (s_CurrentEntityDestroyRequested || !binding || binding->graphFile != pending.graphPath ||
					scene.GetActivityRevision() != pending.sceneActivityRevision ||
					!IsTimelineEventCurrent(world, entity, pending.playSerial)) return;
				if (!runtime.executor.Begin(node, dt).Succeeded()) continue;
				runtime.executor.Resume();
				while (runtime.executor.GetState() == EGraphExecutionState::Running)
					runtime.executor.Tick();
				if (runtime.executor.GetDiagnostic().Code == EGraphError::ExecutionLimit)
				{
					runtime.isValid = false;
					return;
				}
			}
		}
	}
}

Scene* GraphSystem::GetCurrentScene() { return s_CurrentScene; }
entt::entity GraphSystem::GetCurrentEntity() { return s_CurrentEntity; }
float GraphSystem::GetCurrentDeltaTime() { return s_CurrentDeltaTime; }
void GraphSystem::RequestCurrentEntityDestroy()
{
	if (s_CurrentScene && s_CurrentEntity != entt::null) s_CurrentEntityDestroyRequested = true;
}
void GraphSystem::SetCurrentContext(Scene* scene, entt::entity entity, float dt)
{
	s_CurrentScene = scene;
	s_CurrentEntity = entity;
	s_CurrentDeltaTime = dt;
	s_CurrentEntityDestroyRequested = false;
}
void GraphSystem::ClearCurrentContext()
{
	s_CurrentScene = nullptr;
	s_CurrentEntity = entt::null;
	s_CurrentDeltaTime = 0.0f;
	s_CurrentEntityDestroyRequested = false;
}

GraphSystem* GraphSystem::Create(void* arg)
{
	auto* instance = new GraphSystem();
	if (instance->Initialize(arg) != EResult::Success)
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

EResult GraphSystem::Initialize(void* arg)
{
	(void)arg;
	OnInitialize(); // 시스템 생성 시 레지스트리 즉시 등록
	return EResult::Success;
}

void GraphSystem::Free()
{
	ActionTimelineEvents::Unsubscribe(m_TimelineEventHandle);
	m_TimelineEventHandle = 0;
	ActionTimelineEvents::UnsubscribeEnd(m_TimelineEndHandle);
	m_TimelineEndHandle = 0;
	m_GraphCache.clear();
	ClearCurrentContext();
	m_Initialized = false;
}
void GraphSystem::OnInitialize()
{
	if (m_Initialized) return;
	const auto builtinDiag = m_Registry.RegisterBuiltins();
	if (!builtinDiag.Succeeded())
	{
		BAM_LOG(Error, "GraphSystem", "Init", "RegisterBuiltins failed: {}", builtinDiag.Message);
		return;
	}
	if (ReflectionRegistry::IsCreated())
	{
		const auto reflectDiag = RegisterReflectedGraphNodes(m_Registry, ReflectionRegistry::Get().GetRegistry());
		if (!reflectDiag.Succeeded())
		{
			BAM_LOG(Error, "GraphSystem", "Init", "RegisterReflectedGraphNodes failed: {}", reflectDiag.Message);
		}
	}
	m_TimelineEventHandle = ActionTimelineEvents::Subscribe([this](const ActionTimelineEvent& event)
		{
			if (IsActive()) QueueTimelineEvent(event);
		});
	m_TimelineEndHandle = ActionTimelineEvents::SubscribeEnd([this](const ActionTimelineEndEvent& event)
		{
			if (IsActive()) QueueTimelineEnd(event);
		});
	m_Initialized = true;
}

std::filesystem::path GraphSystem::ResolveGraphPath(const std::filesystem::path& path)
{
	if (path.empty()) return {};
	const auto findFile = [](std::filesystem::path candidate) -> std::filesystem::path
	{
		if (PakMountManager::Get().ExistsOrOnDisk(candidate)) return PlatformPath::Normalize(candidate);
		if (candidate.extension() == L".json" && candidate.stem().extension() != L".bamgraph")
		{
			candidate.replace_extension(L".bamgraph.json");
			if (PakMountManager::Get().ExistsOrOnDisk(candidate)) return PlatformPath::Normalize(candidate);
		}
		return {};
	};

	if (path.is_absolute() || PlatformPath::IsVirtualUri(path.wstring()))
		return findFile(PlatformPath::ResolveUri(path));

	for (const auto* prefix : { L"", L"Resources/Scripts", L"Resources" })
	{
		if (auto found = findFile(PlatformPath::ResolveProjectRelative(std::filesystem::path(prefix) / path)); !found.empty())
			return found;
	}
	if (auto found = findFile(PlatformPath::ResolveUri(path)); !found.empty()) return found;
	if (path.extension() == L".json" && path.stem().extension() != L".bamgraph")
	{
		auto compound = path;
		compound.replace_extension(L".bamgraph.json");
		return findFile(PlatformPath::ResolveUri(compound));
	}
	return {};
}

const Graph* GraphSystem::GetOrLoadGraph(const std::wstring& path, std::wstring* sourcePath)
{
	if (path.empty() || !ReflectionRegistry::IsCreated()) return nullptr;
	const auto resolveStart = Clock::now();
	const auto targetPath = ResolveGraphPath(path);
	if (targetPath.empty())
	{
		BAM_LOG(Error, "GraphSystem", "Load", "Graph file not found: '{}'", WStrToStr(path));
		return nullptr;
	}
	const auto key = targetPath.wstring();
	if (sourcePath) *sourcePath = key;
	CachedGraph candidate;
#if BAM_EDITOR
	std::error_code timestampError;
	candidate.LastWriteTime = std::filesystem::last_write_time(targetPath, timestampError);
#endif
	const auto resolveEnd = Clock::now();
	const auto it = m_GraphCache.find(key);
	if (it != m_GraphCache.end())
	{
#if BAM_EDITOR
		if (!timestampError && it->second.LastWriteTime == candidate.LastWriteTime)
			return &it->second.Data;
		m_GraphCache.erase(it);
#else
		return &it->second.Data;
#endif
	}

	const auto loadStart = Clock::now();
	OnInitialize();
	const auto& reflection = ReflectionRegistry::Get();
	const auto result = LoadGraphJson(targetPath, candidate.Data, m_Registry,
		reflection.GetRegistry(), reflection.GetPropertyCodecs());
	if (!result.Succeeded())
	{
		BAM_LOG(Error, "GraphSystem", "Load", "Failed to load graph JSON '{}': Stage {}, Error {}",
			WStrToStr(key), static_cast<int>(result.Stage), result.GraphError.Message);
		return nullptr;
	}
	auto [insertedIt, success] = m_GraphCache.emplace(key, std::move(candidate));
	BAM_LOG(Info, "GraphSystem", "Load", "Successfully loaded graph: '{}' (resolve={:.2f} ms, load={:.2f} ms)",
		WStrToStr(key), Milliseconds(resolveEnd - resolveStart).count(), Milliseconds(Clock::now() - loadStart).count());
	return success ? &insertedIt->second.Data : nullptr;
}

#if BAM_EDITOR
void GraphSystem::PrepareSceneGraphs(const Scene& scene)
{
	const auto& world = scene.GetRegistry();
	const auto bindings = world.view<GraphBindingComponent>();
	std::unordered_set<std::wstring_view> preparedPaths;
	for (const auto entity : bindings)
	{
		const auto& binding = bindings.get<GraphBindingComponent>(entity);
		if (!IsComponentEnabled(world, entity, binding) || binding.graphFile.empty()) continue;

		if (preparedPaths.emplace(binding.graphFile).second)
			GetOrLoadGraph(binding.graphFile);
	}
}
#endif

void GraphSystem::InvalidateGraphCache(const std::function<bool(const std::filesystem::path&)>& affects)
{
	std::erase_if(m_GraphCache, [&affects](const auto& entry) { return affects(entry.first); });
}

void GraphSystem::OnUpdate(entt::registry& globalRegistry, const vector<Scene*>& activeScenes, f32 dt)
{
	(void)globalRegistry;
	if (!std::isfinite(dt) || dt < 0.0f) return;

	const reflection::Registry* sourceRegistry =
		ReflectionRegistry::IsCreated() ? &ReflectionRegistry::Get().GetRegistry() : nullptr;

	for (Scene* scene : activeScenes)
	{
		if (!scene) continue;

		auto& world = scene->GetRegistry();
		// 타임라인 시스템은 그래프보다 먼저 갱신되므로 첫 틱의 이벤트도 초기화 후 전달한다.
		SceneTimelineEvents timelineEvents;
		if (auto* pending = world.ctx().find<SceneTimelineEvents>()) timelineEvents.swap(*pending);
		SceneTimelineEnds timelineEnds;
		if (auto* pending = world.ctx().find<SceneTimelineEnds>()) timelineEnds.swap(*pending);
		if (!scene->IsActive() || scene->IsPaused()) continue;

		std::vector<entt::entity> destroyRequests;
		auto view = world.view<GraphBindingComponent>();

		for (auto entity : view)
		{
			const auto& binding = view.get<GraphBindingComponent>(entity);
			if (!IsComponentEnabled(world, entity, binding) || binding.graphFile.empty()) continue;

			auto* runtime = world.try_get<GraphRuntimeState>(entity);

			// 1. 인스턴스 초기화 또는 파일 변경 감지 시 리로드
			if (!runtime || runtime->loadedPath != binding.graphFile)
			{
				GraphRuntimeState newState;
				const Graph* graphTemplate = GetOrLoadGraph(binding.graphFile, &newState.sourcePath);
				if (!graphTemplate) continue;

				const auto executorStart = Clock::now();
				graphTemplate->RegisterVariableNodes(m_Registry);

				newState.loadedPath = binding.graphFile;
#if BAM_EDITOR
				newState.executor.SetNodeStepCallback(NotifyCurrentGraphNode);
#endif
				const auto diag = newState.executor.Load(*graphTemplate, m_Registry, sourceRegistry);
				if (!diag.Succeeded())
				{
					BAM_LOG(Error, "GraphSystem", "Load", "Failed to load graph: {}", diag.Message);
					continue;
				}

				for (const auto& node : graphTemplate->GetNodes())
				{
					if (node.Type == "Event.Start") newState.startNodes.push_back(node.Id);
					else if (node.Type == "Event.Update") newState.updateNodes.push_back(node.Id);
					else if (node.Type == "Event.OnTriggerEnter") newState.triggerEnterNodes.push_back(node.Id);
					else if (node.Type == "Event.OnTriggerStay") newState.triggerStayNodes.push_back(node.Id);
					else if (node.Type == "Event.OnTriggerExit") newState.triggerExitNodes.push_back(node.Id);
				}
				newState.actionEventEntries = CollectActionEventEntries(*graphTemplate, m_Registry);
				newState.timelineEventEntries = CollectTimelineEventEntries(*graphTemplate);
				newState.timelineEndEntries = CollectTimelineEndEntries(*graphTemplate);
				const auto executorMs = Milliseconds(Clock::now() - executorStart).count();
				if (executorMs >= 10.0)
				{
					BAM_LOG(Info, "GraphSystem", "Load", "Initialized graph executor: '{}' ({:.2f} ms)",
						WStrToStr(newState.sourcePath), executorMs);
				}
				runtime = &world.emplace_or_replace<GraphRuntimeState>(entity, std::move(newState));
			}

			if (!runtime || !runtime->isValid) continue;

			// 실행 전 현재 엔티티 및 씬 컨텍스트 바인딩
			SetCurrentContext(scene, entity, dt);

			try
			{
				// 1. Event.Start (BeginPlay) 최초 1회 실행
				if (!runtime->hasStarted)
				{
					for (GraphNodeId startEntry : runtime->startNodes)
					{
						if (s_CurrentEntityDestroyRequested) break;
#if BAM_EDITOR
						if (s_NodeExecuteHook) s_NodeExecuteHook(runtime->sourcePath, startEntry);
#endif

						if (runtime->executor.Begin(startEntry, 0.0f).Succeeded())
						{
							runtime->executor.Resume();
							while (runtime->executor.GetState() == EGraphExecutionState::Running)
								runtime->executor.Tick();
						}
					}
					runtime->hasStarted = true;
				}

				if (const auto found = timelineEvents.find(entity); found != timelineEvents.end())
					DispatchTimelineEvents(*scene, entity, *runtime, found->second, dt);
				// 같은 프레임의 노티파이 이벤트를 먼저 보낸 뒤 종료를 보낸다(타임라인 시스템의 발행 순서와 같다).
				if (const auto found = timelineEnds.find(entity); found != timelineEnds.end() && runtime->isValid)
					DispatchTimelineEnds(*scene, entity, *runtime, found->second);

				if (runtime->isValid && !s_CurrentEntityDestroyRequested && InputManager::IsCreated())
				{
					const auto& input = InputManager::Get();
					for (const auto& entry : runtime->actionEventEntries)
					{
						if (s_CurrentEntityDestroyRequested) break;
						const bool triggered = entry.Phase == GraphRuntimeState::ActionEventPhase::Pressed
							? input.WasActionPressed(entry.MapId, entry.ActionId)
							: input.WasActionReleased(entry.MapId, entry.ActionId);
						if (!triggered)
							continue;

						if (!runtime->executor.Begin(entry.Node, dt).Succeeded())
							continue;

						runtime->executor.Resume();
						while (runtime->executor.GetState() == EGraphExecutionState::Running)
							runtime->executor.Tick();

						if (runtime->executor.GetDiagnostic().Code == EGraphError::ExecutionLimit)
						{
							runtime->isValid = false;
							break;
						}
					}
				}

				// 2. Event.Update (Tick) 매 프레임 디스패치
				for (GraphNodeId updateEntry : runtime->updateNodes)
				{
					if (!runtime->isValid || s_CurrentEntityDestroyRequested) break;
#if BAM_EDITOR
					if (s_NodeExecuteHook) s_NodeExecuteHook(runtime->sourcePath, updateEntry);
#endif

					if (runtime->executor.Begin(updateEntry, dt).Succeeded())
					{
						runtime->executor.Resume();
						while (runtime->executor.GetState() == EGraphExecutionState::Running)
						{
							runtime->executor.Tick();

#if BAM_EDITOR
							// 와이어를 타고 진입하는 노드들도 훅에 통보
							if (s_NodeExecuteHook)
							{
								for (const auto& event : runtime->executor.GetTrace())
								{
									if (event.Kind == EGraphTraceKind::Enter)
										s_NodeExecuteHook(runtime->sourcePath, event.Node);
								}
							}
#endif
						}
					}
					else if (runtime->executor.GetDiagnostic().Code == EGraphError::ExecutionLimit)
					{
						runtime->isValid = false;
						break;
					}
				}
			}
			catch (const std::exception& e)
			{
				BAM_LOG(Error, "GraphSystem", "Execution", "Script error on entity {}: {}",
					entt::to_integral(entity), e.what());
				runtime->isValid = false;
			}
			catch (...)
			{
				BAM_LOG(Error, "GraphSystem", "Execution", "Unknown script error on entity {}",
					entt::to_integral(entity));
				runtime->isValid = false;
			}

			// 실행 완료 후 컨텍스트 해제
			if (s_CurrentEntityDestroyRequested) destroyRequests.push_back(entity);
			ClearCurrentContext();
		}

		// 자기 삭제는 현재 실행기와 뷰가 사용하는 컴포넌트를 순회 이후에 제거한다.
		for (const auto entity : destroyRequests)
		{
			if (auto* wrapper = scene->FindEntity(entity)) (void)scene->DestroyEntity(*wrapper);
		}
	}
}

#if BAM_EDITOR
const GraphExecutor* GraphSystem::GetDebugExecutor(Scene* scene, entt::entity entity)
{
	if (!scene || entity == entt::null) return nullptr;
	const auto& world = scene->GetRegistry();
	if (!TryGetEnabledComponent<GraphBindingComponent>(world, entity)) return nullptr;
	const auto* runtime = world.try_get<GraphRuntimeState>(entity);
	return runtime ? &runtime->executor : nullptr;
}

void GraphSystem::SetNodeExecuteHook(NodeExecuteHook hook)
{
	s_NodeExecuteHook = std::move(hook);
}

void GraphSystem::TriggerNodeExecuteHook(const std::wstring& graphPath, GraphNodeId nodeId)
{
	if (s_NodeExecuteHook)
	{
		s_NodeExecuteHook(graphPath, nodeId);
	}
}
#endif

void GraphSystem::ReloadReflectedNodes(const std::vector<Scene*>& activeScenes)
{
	m_GraphCache.clear();

	if (!ReflectionRegistry::IsCreated()) return;
	const auto& reflection = ReflectionRegistry::Get().GetRegistry();

	// 1. 전역 노드 레지스트리의 리플렉션 노드 재생성
	m_Registry.ClearReflectedNodes();
	const auto reflectDiag = RegisterReflectedGraphNodes(m_Registry, reflection, true);
	if (!reflectDiag.Succeeded())
	{
		BAM_LOG(Error, "GraphSystem", "HotSwap", "RegisterReflectedGraphNodes failed: {}", reflectDiag.Message);
	}

	// 2. 활성 씬의 모든 GraphBinding 엔티티 상태 보존 핫스왑
	for (Scene* scene : activeScenes)
	{
		if (!scene) continue;
		auto& world = scene->GetRegistry();
		auto view = world.view<GraphBindingComponent, GraphRuntimeState>();
		for (auto entity : view)
		{
			const auto& binding = view.get<GraphBindingComponent>(entity);
			auto& runtime = view.get<GraphRuntimeState>(entity);
			if (!IsComponentEnabled(world, entity, binding))
			{
				runtime.loadedPath.clear();
				runtime.isValid = false;
				continue;
			}
			if (binding.graphFile.empty()) continue;

			std::wstring sourcePath;
			const Graph* graphTemplate = GetOrLoadGraph(binding.graphFile, &sourcePath);
			if (!graphTemplate) continue;

			graphTemplate->RegisterVariableNodes(m_Registry);

			// 기존 런타임 변수 값 및 실행 상태 보존
			auto oldVariables = runtime.executor.GetVariables();
			const bool wasStarted = runtime.hasStarted;

			runtime.executor.Clear();
#if BAM_EDITOR
			runtime.executor.SetNodeStepCallback(NotifyCurrentGraphNode);
#endif

			const auto diag = runtime.executor.Load(*graphTemplate, m_Registry, &reflection);
			if (!diag.Succeeded())
			{
				BAM_LOG(Error, "GraphSystem", "HotSwap", "Failed to reload graph: {}", diag.Message);
				runtime.isValid = false;
				continue;
			}

			// 변수 값 복원
			for (const auto& [name, val] : oldVariables)
			{
				runtime.executor.SetVariable(name, val);
			}
			runtime.loadedPath = binding.graphFile;
			runtime.sourcePath = std::move(sourcePath);
			runtime.actionEventEntries = CollectActionEventEntries(*graphTemplate, m_Registry);
			runtime.timelineEventEntries = CollectTimelineEventEntries(*graphTemplate);
			runtime.timelineEndEntries = CollectTimelineEndEntries(*graphTemplate);
			runtime.hasStarted = wasStarted;
			runtime.isValid = true;
		}
	}

	BAM_LOG(Info, "GraphSystem", "HotSwap", "Successfully hot-swapped runtime graph executors.");
}

void GraphSystem::DispatchTriggerEvent(Scene* scene, entt::entity self, entt::entity other, uint8 triggerType)
{
	if (!scene) return;
	auto& world = scene->GetRegistry();
	if (!TryGetEnabledComponent<GraphBindingComponent>(world, self)) return;
	auto* runtime = world.try_get<GraphRuntimeState>(self);
	if (!runtime || !runtime->isValid) return;

	const std::vector<GraphNodeId>* targetNodes = nullptr;
	if (triggerType == 0) targetNodes = &runtime->triggerEnterNodes;       // Enter
	else if (triggerType == 1) targetNodes = &runtime->triggerStayNodes;  // Stay
	else if (triggerType == 2) targetNodes = &runtime->triggerExitNodes;  // Exit

	if (!targetNodes || targetNodes->empty()) return;

	SetCurrentContext(scene, self, 0.0f);

	for (GraphNodeId nodeId : *targetNodes)
	{
		if (s_CurrentEntityDestroyRequested) break;
		const int64_t otherId = static_cast<int64_t>(static_cast<uint32_t>(other));
		if (runtime->executor.BeginTrigger(nodeId, otherId).Succeeded())
		{
			runtime->executor.Resume();
			while (runtime->executor.GetState() == EGraphExecutionState::Running)
				runtime->executor.Tick();
		}
	}

	const bool destroyRequested = s_CurrentEntityDestroyRequested;
	ClearCurrentContext();
	if (destroyRequested)
	{
		if (auto* wrapper = scene->FindEntity(self)) (void)scene->DestroyEntity(*wrapper);
	}
}
