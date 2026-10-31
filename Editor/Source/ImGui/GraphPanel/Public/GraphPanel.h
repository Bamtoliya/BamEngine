#pragma once

#include "ImGuiInterface.h"
#include "Graph.h"
#include "GraphNodePalette.h"
#include "GraphReflection.h"
#include "GraphExecutor.h"
#include "GraphFile.h"
#include "ShaderCompiler.h"
#include "Material.h"

namespace ax::NodeEditor { struct EditorContext; }

BEGIN(Editor)
enum class EWireStyle : int { Curved = 0, ElectronicHybrid = 1, Straight = 2 };
class GraphShaderPreview;
class GraphPanel : public IImGui
{
public:
	GraphPanel();
	virtual ~GraphPanel() override;
	GraphPanel(const GraphPanel&) = delete;
	GraphPanel& operator=(const GraphPanel&) = delete;
	virtual void Draw() override;
	virtual void Free() override;

	const Engine::Graph& GetGraph() const;
	[[nodiscard]] Engine::GraphDiagnostic RegisterNode(Engine::GraphNodeDefinition definition);
	[[nodiscard]] Engine::GraphDiagnostic RegisterReflectionNodes(const reflection::Registry& registry);
public:
	virtual EShortcutContext GetShortcutContext() const override { return EShortcutContext::GraphEditor; }
	static GraphPanel* GetActiveGraphPanel() { return s_ActiveGraphPanel; }
	void AlignNodes(char dir) { AlignSelectedNodes(dir); }
	void DuplicateSelectedNodes() { DuplicateSelection(); }
	void CreateCommentBox() { CreateCommentBoxFromSelection(); }
	void DeleteSelectedElements() { DeleteSelection(); }
	void ToggleBreakpointOnSelected();
private:
	inline static GraphPanel* s_ActiveGraphPanel = nullptr;

private:
	struct NodeView
	{
		int Id = 0;
		std::unordered_map<std::string, int> Pins;
	};

	struct ShaderBuildState
	{
		std::string Source;
		Engine::ShaderCompileResult LastGood;
		Engine::ERHIType LastTarget = Engine::ERHIType::Unknown;
		std::string StatusKey = "GraphPanel.ShaderIdle";
		std::string Details;
		bool SourceCurrent = false;
		std::shared_ptr<GraphShaderPreview> Preview;
		std::string MaterialSource;
		Engine::ShaderCompileResult MaterialLastGood;
		bool MaterialCurrent = false;
		Engine::ResourceHandle<Engine::Material> RuntimeMaterial;
		std::filesystem::path RuntimeMaterialArtifact;
	};

	struct GraphDocument
	{
		explicit GraphDocument(Engine::EGraphDomain domain) : Data(domain) {}
		Engine::Graph Data;
		std::filesystem::path FilePath;
		Engine::GraphFileResult FileResult;
		ax::NodeEditor::EditorContext* Context = nullptr;
		std::unordered_map<Engine::GraphNodeId, NodeView> Nodes;
		std::unordered_map<int, Engine::GraphPinReference> Pins;
		std::unordered_map<Engine::GraphLinkId, int> Links;
		std::vector<Engine::GraphNodeId> SelectedNodes;
		std::vector<Engine::GraphLinkId> SelectedLinks;
		std::vector<Engine::GraphDiagnostic> Diagnostics;
		std::uint64_t NextId = 1;
		Engine::GraphNodeId PendingSelection = 0;
		bool ResetContext = false;
		bool RestoreSelection = false;
		ShaderBuildState ShaderBuild;
		std::unordered_map<Engine::GraphNodeId, std::string> CommentTexts;
	};

#pragma region DrawUI
private:
	void DrawMenuBar();
	void DrawToolbar();
	void DrawFileToolbar();
	void SaveDocument(bool saveAs);
	void OpenDocument();
	Engine::GraphDiagnostic BuildDocumentViews(GraphDocument& document);
	void DrawDebugToolbar();
	void UpdateLiveDebugging();
	void DrawExecutionToolbar();
	void DrawExecutionMenu();
	void DrawShaderMenu();
	void DrawExecutionTrace();
	void BeginExecution();
	void UpdateExecution();
	void InvalidateExecution();
	static std::string TraceValueText(const Engine::GraphValue& value);
	const Engine::GraphValue* FindObservedValue(const Engine::GraphPinReference& pin) const;
	void PrepareNodeMenu(Engine::GraphPinReference source = {});
	void DrawNodeMenu();
	void DrawProperties();
	void DrawVariables();
	void DrawInputValue(const Engine::GraphNode& node, const Engine::GraphPin& pin);
	void DrawDiagnostics();
	void DrawStatus();
	static bool DrawValue(Engine::EGraphPinType type, Engine::GraphValue& value);
	static Engine::GraphValue MakeDefaultValue(Engine::EGraphPinType type);
	
private:
	void DrawShaderToolbar();
	void DrawShaderBuildDetails();
	void DrawShaderPreview();
	bool GenerateShaderSource();
	bool CompileShaderGraph(Engine::ERHIType target);
	void BuildShaderPreview();
	void InvalidateShaderBuild();
	void CompileMaterialShader();
	void DrawMaterialShaderDetails();
	void CreateRuntimeMaterial();
	void DrawRuntimeMaterialDetails();
#pragma endregion

#pragma region NodeEditor
private:
	void ActivateDocument();
	void SuspendDocument();
	void DrawCanvas(float height);
	void DrawNode(const Engine::GraphNode& node);
	void DrawPinValue(const Engine::GraphNode& node, const Engine::GraphPin& pin, float width);
	void DrawLinks();
	void HandleConnections(const ImVec2& origin, bool canvasHovered);
	void ReadSelection();
	bool ReserveEditorIds(std::size_t count);
	int FindPinId(const Engine::GraphPinReference& pin) const;
	void GetConnectionPins(int first, int second, Engine::GraphPinReference& from, Engine::GraphPinReference& to) const;
	static ImU32 PinColor(Engine::EGraphPinType type);
	static const char* PinTypeName(Engine::EGraphPinType type);
	void DrawCommentNode(const Engine::GraphNode& node);
	void CreateCommentBoxFromSelection();

	void DrawRerouteNode(const Engine::GraphNode& node);
	void InsertRerouteNodeOnLink(uintptr_t hoveredLinkEdId, const ImVec2& canvasPos);
	void AlignSelectedNodes(char direction);

	void DuplicateSelection();
	void BreakAllNodeLinks(Engine::GraphNodeId nodeId);
#pragma endregion

#pragma region Debugging
private:
	void ToggleBreakpoint(Engine::GraphNodeId node);
	bool HasBreakpoint(Engine::GraphNodeId node) const;
#pragma endregion

#pragma region Editing
private:
	GraphDocument& GetDocument();
	const GraphDocument& GetDocument() const;
	bool AddNode(const GraphNodeCandidate& candidate);
	void DeleteSelection();
	void RefreshValidation();
	bool ApplyResult(Engine::GraphDiagnostic result);
#pragma endregion

#pragma region Variables
private:
	Engine::GraphRegistry m_Registry;
	GraphDocument m_Logic{ Engine::EGraphDomain::Logic };
	GraphDocument m_Shader{ Engine::EGraphDomain::Shader };
	Engine::EGraphDomain m_Domain = Engine::EGraphDomain::Logic;
	Engine::GraphDiagnostic m_LastDiagnostic;
	bool m_Ready = false;
	Engine::GraphExecutor m_Executor;
	reflection::Registry m_ExecutionReflection;
	Engine::GraphNodeId m_TraceNode = 0;
	Engine::GraphLinkId m_TraceLink = 0;
	std::uint64_t m_SelectedTrace = 0;
	bool m_FollowTrace = true;
	float m_StepInterval = 0.15f;
	float m_StepElapsed = 0.0f;
	Engine::GraphNodeId m_EntryNode = 0;
	float m_ExecutionDelta = 1.0f / 60.0f;
	int m_ExecutionBudget = 10000;

	ImVec2 m_AddPosition = { 40.0f, 40.0f };
	int m_DragPin = 0;
	Engine::GraphPinReference m_AddSource;
	std::vector<GraphNodeCandidate> m_NodeCandidates;
	char m_NodeSearch[256] = {};
	bool m_NodeMenuActive = false;
	bool m_FocusNodeSearch = false;
	bool m_OpenNodePopup = false;
	std::unordered_map<Engine::GraphNodeId, std::unordered_map<std::string, Engine::GraphValue>> m_ObservedValues;

	entt::entity m_DebugEntity = entt::null;
	std::unordered_map<Engine::GraphLinkId, float> m_LinkPulseTimers;
	std::uint64_t m_LastDebugTraceSequence = 0;

	char m_NewVarName[64] = {};
	int m_NewVarType = 2; // 기본값: Float

	std::unordered_map<std::wstring, std::unordered_set<Engine::GraphNodeId>> m_Breakpoints;
	Engine::GraphNodeId m_HitBreakpointNode = 0;

	uintptr_t m_ContextLinkId = 0;
	uintptr_t m_ContextNodeId = 0;
	uintptr_t m_ContextPinId = 0;
	ImVec2 m_ContextPosition = { 0.0f, 0.0f };

	Engine::GraphNodeId m_EditingCommentNode = 0;
	char m_CommentEditBuffer[128] = {};
	EWireStyle m_WireStyle = EWireStyle::ElectronicHybrid;
#pragma endregion
};

END