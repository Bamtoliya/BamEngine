#include "GraphSystem.h"
#include "GraphFile.h"
#include "GraphReflection.h"
#include "ReflectionRegistry.h"
#include "GraphBindingComponent.h"
#include "Scene.h"
#include "Logger.h"
#include "PlatformPath.h"
#include "InputManager.h"
namespace
{
	Scene* s_CurrentScene = nullptr;
	entt::entity s_CurrentEntity = entt::null;
	float s_CurrentDeltaTime = 0.0f;
#ifdef BAM_EDITOR
	GraphSystem::NodeExecuteHook s_NodeExecuteHook = nullptr;
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
}

Scene* GraphSystem::GetCurrentScene() { return s_CurrentScene; }
entt::entity GraphSystem::GetCurrentEntity() { return s_CurrentEntity; }
float GraphSystem::GetCurrentDeltaTime() { return s_CurrentDeltaTime; }
void GraphSystem::SetCurrentContext(Scene* scene, entt::entity entity, float dt)
{
	s_CurrentScene = scene;
	s_CurrentEntity = entity;
	s_CurrentDeltaTime = dt;
}
void GraphSystem::ClearCurrentContext()
{
	s_CurrentScene = nullptr;
	s_CurrentEntity = entt::null;
	s_CurrentDeltaTime = 0.0f;
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
	m_Initialized = true;
}

const Graph* GraphSystem::GetOrLoadGraph(const std::wstring& path)
{
	if (path.empty() || !ReflectionRegistry::IsCreated()) return nullptr;
	auto it = m_GraphCache.find(path);
	if (it != m_GraphCache.end()) return &it->second;
	// 레지스트리 초기화 보장
	OnInitialize();
	// ── 스마트 경로 탐색 (프로젝트 루트 기준 결합) ──
	auto tryResolveFallback = [](std::filesystem::path& p) -> bool {
		if (std::filesystem::exists(p)) return true;
		const std::string ext = p.extension().string();
		if (ext == ".json")
		{
			const std::string stem = p.stem().string();
			if (!stem.ends_with(".bamgraph"))
			{
				const auto candidate = p.parent_path() / (stem + ".bamgraph.json");
				if (std::filesystem::exists(candidate)) { p = candidate; return true; }
			}
		}
		return false;
	};

	std::filesystem::path targetPath(path);
	if (targetPath.is_relative())
		targetPath = PlatformPath::ResolveProjectRelative(targetPath);
	if (!tryResolveFallback(targetPath))
	{
		// 1. Resources/Scripts/ 아래에서 검색
		auto scriptsPath = PlatformPath::ResolveProjectRelative(std::filesystem::path(L"Resources/Scripts") / path);
		if (tryResolveFallback(scriptsPath))
			targetPath = scriptsPath;
		// 2. Resources/ 아래에서 검색
		else
		{
			auto resPath = PlatformPath::ResolveProjectRelative(std::filesystem::path(L"Resources") / path);
			if (tryResolveFallback(resPath))
				targetPath = resPath;
		}
	}
	if (!std::filesystem::is_regular_file(targetPath))
		targetPath = PlatformPath::ResolveUri(path);

	if (!std::filesystem::is_regular_file(targetPath))
	{
		BAM_LOG(Error, "GraphSystem", "Load", "Graph file not found: '{}'", WStrToStr(path));
		return nullptr;
	}
	const auto& reflection = ReflectionRegistry::Get();
	Graph loadedGraph;
	const auto result = LoadGraphJson(targetPath, loadedGraph, m_Registry,
		reflection.GetRegistry(), reflection.GetPropertyCodecs());
	if (!result.Succeeded())
	{
		BAM_LOG(Error, "GraphSystem", "Load", "Failed to load graph JSON '{}': Stage {}, Error {}",
			targetPath.string(), static_cast<int>(result.Stage), result.GraphError.Message);
		return nullptr;
	}
	BAM_LOG(Info, "GraphSystem", "Load", "Successfully loaded graph: '{}'", targetPath.string());
	auto [insertedIt, success] = m_GraphCache.emplace(path, std::move(loadedGraph));
	return success ? &insertedIt->second : nullptr;
}

void GraphSystem::OnUpdate(entt::registry& globalRegistry, const vector<Scene*>& activeScenes, f32 dt)
{
	(void)globalRegistry;
	if (!std::isfinite(dt) || dt < 0.0f) return;

	const reflection::Registry* sourceRegistry =
		ReflectionRegistry::IsCreated() ? &ReflectionRegistry::Get().GetRegistry() : nullptr;

	for (Scene* scene : activeScenes)
	{
		if (!scene || !scene->IsActive() || scene->IsPaused()) continue;

		auto& world = scene->GetRegistry();
		auto view = world.view<GraphBindingComponent>();

		for (auto entity : view)
		{
			const auto& binding = view.get<GraphBindingComponent>(entity);
			if (!binding.enabled || binding.graphFile.empty()) continue;

			auto* runtime = world.try_get<GraphRuntimeState>(entity);

			// 1. 인스턴스 초기화 또는 파일 변경 감지 시 리로드
			if (!runtime || runtime->loadedPath != binding.graphFile)
			{
				const Graph* graphTemplate = GetOrLoadGraph(binding.graphFile);
				if (!graphTemplate) continue;

				graphTemplate->RegisterVariableNodes(m_Registry);

				GraphRuntimeState newState;
				newState.loadedPath = binding.graphFile;
				newState.executor.SetNodeStepCallback([path = binding.graphFile](GraphNodeId nodeId) {
#ifdef BAM_EDITOR
					GraphSystem::TriggerNodeExecuteHook(path, nodeId);
#endif
					});
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
#ifdef BAM_EDITOR
						if (s_NodeExecuteHook) s_NodeExecuteHook(binding.graphFile, startEntry);
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

				if (InputManager::IsCreated())
				{
					const auto& input = InputManager::Get();
					for (const auto& entry : runtime->actionEventEntries)
					{
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

				if (!runtime->isValid)
				{
					ClearCurrentContext();
					continue;
				}

				// 2. Event.Update (Tick) 매 프레임 디스패치
				for (GraphNodeId updateEntry : runtime->updateNodes)
				{
#ifdef BAM_EDITOR
					if (s_NodeExecuteHook) s_NodeExecuteHook(binding.graphFile, updateEntry);
#endif

					if (runtime->executor.Begin(updateEntry, dt).Succeeded())
					{
						runtime->executor.Resume();
						while (runtime->executor.GetState() == EGraphExecutionState::Running)
						{
							runtime->executor.Tick();

#ifdef BAM_EDITOR
							// 와이어를 타고 진입하는 노드들도 훅에 통보
							if (s_NodeExecuteHook)
							{
								for (const auto& event : runtime->executor.GetTrace())
								{
									if (event.Kind == EGraphTraceKind::Enter)
										s_NodeExecuteHook(binding.graphFile, event.Node);
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
			ClearCurrentContext();
		}
	}
}

#ifdef BAM_EDITOR
const GraphExecutor* GraphSystem::GetDebugExecutor(Scene* scene, entt::entity entity)
{
	if (!scene || entity == entt::null) return nullptr;
	const auto* runtime = scene->GetRegistry().try_get<GraphRuntimeState>(entity);
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
			if (binding.graphFile.empty()) continue;

			const Graph* graphTemplate = GetOrLoadGraph(binding.graphFile);
			if (!graphTemplate) continue;

			graphTemplate->RegisterVariableNodes(m_Registry);

			// 기존 런타임 변수 값 및 실행 상태 보존
			auto oldVariables = runtime.executor.GetVariables();
			const bool wasStarted = runtime.hasStarted;

			runtime.executor.Clear();
#ifdef BAM_EDITOR
			runtime.executor.SetNodeStepCallback([path = binding.graphFile](GraphNodeId nodeId) {
				GraphSystem::TriggerNodeExecuteHook(path, nodeId);
				});
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
			runtime.actionEventEntries = CollectActionEventEntries(*graphTemplate, m_Registry);
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
		const int64_t otherId = static_cast<int64_t>(static_cast<uint32_t>(other));
		if (runtime->executor.BeginTrigger(nodeId, otherId).Succeeded())
		{
			runtime->executor.Resume();
			while (runtime->executor.GetState() == EGraphExecutionState::Running)
				runtime->executor.Tick();
		}
	}

	ClearCurrentContext();
}