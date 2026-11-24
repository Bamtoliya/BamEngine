#pragma once

#include "Engine_Includes.h"
#include "System.h"
#include "Graph.h"
#include "GraphExecutor.h"
#include "GraphRegistry.h"
#include "Delegate.h"
#include <filesystem>
#include <unordered_map>
#include <string>
#include <vector>
#include <functional>

BEGIN(Engine)

// 씬 레지스트리 내부에서 엔티티별 런타임 실행 상태를 보관하는 컴포넌트
struct ENGINE_API GraphRuntimeState
{
	std::wstring loadedPath;
	std::wstring sourcePath;
	GraphExecutor executor;
	std::vector<GraphNodeId> startNodes;
	std::vector<GraphNodeId> updateNodes;
	std::vector<GraphNodeId> triggerEnterNodes;
	std::vector<GraphNodeId> triggerStayNodes;
	std::vector<GraphNodeId> triggerExitNodes;
	bool hasStarted = false;
	bool isValid = true;

	enum class ActionEventPhase
	{
		Pressed,
		Released
	};

	struct ActionEventEntry
	{
		GraphNodeId Node = 0;
		std::string MapId;
		std::string ActionId;
		ActionEventPhase Phase = ActionEventPhase::Pressed;
	};

	std::vector<ActionEventEntry> actionEventEntries;
	std::unordered_map<std::string, std::vector<GraphNodeId>> timelineEventEntries;
	std::vector<GraphNodeId> timelineEndEntries;
};

class ENGINE_API GraphSystem : public ISystem
{
private:
	GraphSystem() = default;
	virtual ~GraphSystem() = default;
	EResult Initialize(void* arg = nullptr);

public:
	static GraphSystem* Create(void* arg = nullptr);
	virtual void Free() override;

	virtual void OnInitialize() override;
	virtual void OnUpdate(entt::registry& globalRegistry, const vector<Scene*>& activeScenes, f32 dt) override;

	static std::filesystem::path ResolveGraphPath(const std::filesystem::path& path);
	const Graph* GetOrLoadGraph(const std::wstring& path, std::wstring* sourcePath = nullptr);
	void ClearGraphCache() { m_GraphCache.clear(); }
	void InvalidateGraphCache(const std::function<bool(const std::filesystem::path&)>& affects);

	void ReloadReflectedNodes(const std::vector<class Scene*>& activeScenes);

	// ── 노드 실행 컨텍스트 (Scene, Entity, DeltaTime) ──
	static void DispatchTriggerEvent(Scene* scene, entt::entity self, entt::entity other, uint8 triggerType);
	static Scene* GetCurrentScene();
	static entt::entity GetCurrentEntity();
	static float GetCurrentDeltaTime();
	static void RequestCurrentEntityDestroy();
	static void SetCurrentContext(Scene* scene, entt::entity entity, float dt);
	static void ClearCurrentContext();

	using NodeExecuteHook = std::function<void(const std::wstring& graphPath, GraphNodeId nodeId)>;
#if BAM_EDITOR
	void PrepareSceneGraphs(const Scene& scene);

	// ── 디버깅 인터페이스 (에디터 계층 연동용) ──
	// 1. 에디터 펄스 렌더링용 실행기 조회
	static const GraphExecutor* GetDebugExecutor(Scene* scene, entt::entity entity);

	// 2. 노드 실행 직전 알림 훅 (에디터가 등록하여 중단점 체크 및 PIE Pause 수행)
	static void SetNodeExecuteHook(NodeExecuteHook hook);
	static void TriggerNodeExecuteHook(const std::wstring& graphPath, GraphNodeId nodeId);
#else
	void PrepareSceneGraphs(const Scene&) {}
	static const GraphExecutor* GetDebugExecutor(Scene*, entt::entity) { return nullptr; }
	static void SetNodeExecuteHook(NodeExecuteHook) {}
	static void TriggerNodeExecuteHook(const std::wstring&, GraphNodeId) {}
#endif

private:
	struct CachedGraph
	{
		Graph Data;
		std::filesystem::file_time_type LastWriteTime{};
	};

	GraphRegistry m_Registry;
	std::unordered_map<std::wstring, CachedGraph> m_GraphCache;
	bool m_Initialized = false;
	DelegateHandle m_TimelineEventHandle = 0;
	DelegateHandle m_TimelineEndHandle = 0;
};

END
