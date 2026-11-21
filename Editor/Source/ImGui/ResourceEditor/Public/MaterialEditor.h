#pragma once

#include "ResourceEditorInterface.h"
#include "ResourceDocumentHistory.h"
#include "MaterialInterface.h"
#include "MaterialPreviewRenderer.h"
#include "NodeEditorUI.h"
#include "Graph.h"
#include "GraphLink.h"
#include "GraphPin.h"
#include "GraphNodePalette.h"
#include "GraphFile.h"
#include "ShaderCompiler.h"
#include <optional>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <string>
#include <filesystem>

BEGIN(Engine)
class MaterialInterface;
END

BEGIN(Editor)

class MaterialEditor final : public ResourceEditorInterface
{
public:
	struct NodeView
	{
		Engine::GraphNodeId Id = 0;
		ImVec2 Size{ 0.0f, 0.0f };
		float HeaderHeight = 0.0f;
		bool NeedsLayout = true;
	};

	struct ShaderBuildState
	{
		bool SourceCurrent = false;
		bool MaterialCurrent = false;
		std::string Source;
		std::string StatusKey = "MaterialEditor.ShaderNotConfigured";
		std::string Details;
		Engine::ShaderCompileResult LastGood;
		Engine::ShaderCompileResult MaterialLastGood;
		Engine::ERHIType LastTarget = Engine::ERHIType::DirectX12;
		Engine::ResourceHandle<Engine::Material> RuntimeMaterial;
		std::filesystem::path RuntimeMaterialArtifact;
	};

	// 머티리얼 속성(파이프라인/셰이더/텍스처/파라미터)과 셰이더 그래프를 단일 타임라인으로 묶는 통합 스냅샷
	struct DocumentSnapshot
	{
		std::string MaterialJson;
		bool HasGraph = false;
		std::filesystem::path GraphFilePath;
		Engine::Graph GraphData{ Engine::EGraphDomain::Shader };
	};

	struct ShaderGraphDocument
	{
		Engine::Graph Data{ Engine::EGraphDomain::Shader };
		std::filesystem::path FilePath;
		std::filesystem::path LoadedPath;
		ax::NodeEditor::EditorContext* Context = nullptr;
		Engine::GraphNodeId PendingSelection = 0;
		ShaderBuildState ShaderBuild;

		ShaderGraphDocument() = default;
		~ShaderGraphDocument()
		{
			if (Context)
			{
				ax::NodeEditor::DestroyEditor(Context);
				Context = nullptr;
			}
		}

		ShaderGraphDocument(const ShaderGraphDocument&) = delete;
		ShaderGraphDocument& operator=(const ShaderGraphDocument&) = delete;

		ShaderGraphDocument(ShaderGraphDocument&& other) noexcept
			: Data(std::move(other.Data))
			, FilePath(std::move(other.FilePath))
			, LoadedPath(std::move(other.LoadedPath))
			, Context(other.Context)
			, PendingSelection(other.PendingSelection)
			, ShaderBuild(std::move(other.ShaderBuild))
		{
			other.Context = nullptr;
		}

		ShaderGraphDocument& operator=(ShaderGraphDocument&& other) noexcept
		{
			if (this != &other)
			{
				if (Context)
				{
					ax::NodeEditor::DestroyEditor(Context);
				}
				Data = std::move(other.Data);
				FilePath = std::move(other.FilePath);
				LoadedPath = std::move(other.LoadedPath);
				Context = other.Context;
				PendingSelection = other.PendingSelection;
				ShaderBuild = std::move(other.ShaderBuild);
				other.Context = nullptr;
			}
			return *this;
		}
	};

public:
	MaterialEditor();
	virtual ~MaterialEditor();

public:
	bool PrepareAssetPathRelocation(AssetPathRelocation& relocation) override;
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override;
	bool HasUnsavedResourceChanges() const override;
	virtual void SaveResource() override { SaveTarget(); }
	bool CanSaveDocument() const;
	EShortcutContext GetShortcutContext() const override { return EShortcutContext::MaterialEditor; }
	virtual void SetTargetResource(const std::filesystem::path& assetPath) override;
	virtual void Update(f32 dt) override;
	virtual void CaptureState(EditorWorkspace& workspace) const override;
	virtual void RestoreState(const EditorWorkspace& workspace) override;

	// 단축키 액션 핸들러 (Public)
	void LiveHotReloadShader();
	void AlignNodes(int alignment);
	void DuplicateSelectedNodes();
	void CreateCommentBox();
	void DeleteSelectedElements();
	void OpenAddNodeMenu();
	void ResetCanvasView();

private:
	void DrawCustomToolbar() override;
	void ReleaseResourceDocument() override;
	bool RestoreSavedResource() override;
	const char* GetSaveActionId() const override { return "MaterialEditor.Save"; }
	void DrawCustomQuickAccess() override;
	void DrawCustomContent() override;
	ImVec2 GetInitialResourceWindowSize() const override { return ImVec2(1280.0f, 760.0f); }
	void OnResourceWindowVisibility(bool visible) override;

	// 좌측 패널 (프리뷰 + 머티리얼 상세 설정)
	void DrawLeftPane(float width, float height);
	void DrawPipelineSettings();
	void DrawShaderSettings();
	void DrawTextureBindings();
	std::vector<Engine::MaterialTextureBinding> GetEffectiveTextureBindings() const;
	void DrawTextureBindingPicker(const Engine::MaterialTextureBinding& binding);
	void DrawTextureSampleBinding(const Engine::GraphNode& node);
	std::optional<Engine::MaterialTextureBinding> GetTextureSampleBinding(const Engine::GraphNode& node) const;
	void DrawTextureSampleThumbnail(const Engine::GraphNode& node) const;
	void DrawParameters();
	void DrawMaterialParameterValue(const std::string& name, const Engine::MaterialParameter& param);
	bool AddGraphParameter(const std::string& name, Engine::EGraphPinType type, Engine::GraphValue initial,
		const ImVec2* position = nullptr, bool adoptExisting = false);

	// 중앙 패널 (셰이더 그래프 캔버스 워크스페이스)
	void DrawCenterCanvasPane(float width, float height);
	void DrawShaderGraphToolbar(EditorUI::ToolbarLayout& toolbar);
	void DrawShaderGraphCanvas(float height);
	void DrawShaderGraphStatusBar(float height);
	void DrawNode(const Engine::GraphNode& node);
	void DrawPinValue(const Engine::GraphNode& node, const Engine::GraphPin& pin);
	void DrawLinks();
	void HandleConnections();
	void ReadSelection();
	void ReserveEditorIds();
	int FindPinId(const Engine::GraphPinReference& reference) const;
	Engine::GraphPinReference ResolveCanvasPin(ax::NodeEditor::PinId id, Engine::EGraphPinDirection* direction = nullptr) const;
	void DrawPinContextMenu();
	std::pair<int, int> GetConnectionPins(const Engine::GraphLink& link) const;
	ImU32 PinColor(Engine::EGraphPinType type) const;
	const char* PinTypeName(Engine::EGraphPinType type) const;
	void DrawCommentNode(const Engine::GraphNode& node);
	void CreateCommentBoxFromSelection(const ImVec2* anchor = nullptr);
	void DrawRerouteNode(const Engine::GraphNode& node);
	void InsertRerouteNodeOnLink(Engine::GraphLinkId linkId);
	void AlignSelectedNodes(int alignment);
	void DuplicateSelection();
	void BreakAllNodeLinks(Engine::GraphNodeId nodeId);
	void PrepareNodeMenu(const Engine::GraphPinReference& sourcePin);
	void DrawNodeMenu();

	// 우측 패널 (노드 인스펙터 + 셰이더 진단/코드)
	void DrawRightInspectorPane(float width, float height);
	void DrawSelectedNodeProperties();
	// 우측 속성 표용 핀 값 위젯(값 열 폭을 채움). 노드 안 입력칸은 DrawPinValue를 쓴다.
	void DrawPinInspectorValue(const Engine::GraphNode& node, const Engine::GraphPin& pin);
	void DrawShaderBuildDetails();
	void DrawShaderDiagnostics();
	void DrawGeneratedHlsl();

	// 저장 & 문서 관리
	void SaveTarget();
	bool LoadShaderGraph(const std::filesystem::path& path);
	bool SaveShaderGraph();
	// 연결된 그래프를 읽기만 한다. 파일 생성·머티리얼 경로 기록·셰이더 교체는 하지 않는다.
	bool HasLinkedShaderGraph() const;
	bool HasInheritedShaderGraph() const;
	bool CanEditShaderGraph() const;
	static bool SupportsGraphPass(Engine::ERenderPassType pass);
	static bool IsGraphShader(const Engine::ResourceHandle<Engine::Shader>& handle);
	void TryLoadLinkedShaderGraph();
	// 사용자가 요청했을 때만 기본 그래프를 만들어 머티리얼에 연결한다.
	void CreateShaderGraphForMaterial();
	void DrawNoShaderGraphState(float width, float height);
	// 저장 경로(가상 URI·프로젝트 상대·머티리얼 폴더 상대)를 실제 파일 경로로 바꾼다.
	std::filesystem::path ResolveShaderGraphPath(const std::string& storedPath) const;
	// 고른 그래프가 셰이더 그래프로 읽힐 때만 연결한다.
	bool LinkShaderGraph(const std::string& graphUri);
	void UnlinkShaderGraph();
	// 경로 피커(지정·해제) + 그래프가 없을 때 [새로 만들기].
	void DrawShaderGraphLinkControls();
	std::filesystem::path GenerateDefaultShaderGraphPath() const;
	bool CreateDefaultMaterialShaderGraph(const std::filesystem::path& path) const;

	// 셰이더 생성 및 실시간 핫스왑
	void InvalidateShaderBuild();
	bool GenerateShaderSource();
	bool CompileShaderGraph(Engine::ERHIType target);
	void CompileMaterialShader();
	void CreateRuntimeMaterial();

	// 그래프 ↔ 머티리얼 파라미터 양방향 동기화
	void SyncParametersFromGraph();
	void SyncGraphFromParameter(const std::string& name, const void* data, Engine::EMaterialParameterType type);

	// 문서 히스토리 (Undo / Redo)
	bool CommitDocumentEdit();
	std::optional<DocumentSnapshot> CaptureDocument() const;
	bool ApplyDocumentSnapshot(const DocumentSnapshot& snapshot);
	bool CommitShaderGraphEdit();
	bool CanUndoDocument() const override;
	bool CanRedoDocument() const override;
	void UndoDocument() override;
	void RedoDocument() override;

private:
	// 머티리얼 리소스
	Engine::ResourceHandle<Engine::MaterialInterface> m_MaterialHandle;
	Engine::MaterialInterface* m_Material = nullptr;
	ResourceDocumentHistory<DocumentSnapshot> m_History;
	bool m_IsReadOnly = false;

	// 새 파라미터 / 텍스처 / 패스 입력 버퍼
	char m_NewParamName[128] = "";
	int m_NewParamTypeIndex = 0;
	char m_NewTextureSlotName[128] = "";
	int m_NewTextureSlotNumber = 0;
	int m_NewPassTypeIndex = 3;
	char m_ParameterSearch[64] = {};

	// 3D 머티리얼 프리뷰
	MaterialPreviewRenderer m_PreviewRenderer;
	bool m_IsVisibleOnScreen = false;
	float m_PreviewDirtyTimer = 0.0f;

	// 3분할 레이아웃 너비/높이
	float m_LeftPaneWidth = 380.0f;
	float m_PreviewHeight = 280.0f;
	float m_RightPaneWidth = 360.0f;
	float m_RightNodePropsHeight = 300.0f;
	float m_RightDiagnosticsHeight = 150.0f;

	// 셰이더 그래프 자체 소유 상태
	Engine::GraphRegistry m_GraphRegistry;
	ShaderGraphDocument m_ShaderGraph;
	Engine::GraphDiagnostic m_LastGraphDiagnostic;
	bool m_GraphReady = false;

	std::unordered_map<Engine::GraphNodeId, NodeView> m_NodeViews;
	std::vector<GraphNodeCandidate> m_NodeCandidates;
	char m_NodeSearch[256] = {};
	char m_NewParameterName[64] = {};
	ImVec2 m_AddPosition{ 0.0f, 0.0f };
	Engine::GraphPinReference m_MenuSourcePin;
	// 핀을 빈 곳에 놓았을 때 노드 메뉴를 연다. 링크 생성 트랜잭션 밖(ed::Suspend 구간)에서 연다.
	bool m_OpenAddNodePopup = false;
	// 툴바·단축키로 연 노드 메뉴는 캔버스에서 마우스 위치를 추가 위치로 바꾼다.
	bool m_AddNodeAtMouse = false;

	Engine::GraphPinReference m_ContextPin;
	Engine::GraphNodeId m_TexturePickerNode = 0;
	bool m_OpenTexturePicker = false;
	std::string m_PendingParameterNode;
	std::string m_ParameterToReveal;
	std::string m_ParameterToDelete;
	std::unordered_set<std::string> m_HiddenParameters;
	bool m_ShowHiddenParameters = false;
	Engine::GraphNodeId m_EditingCommentNode = 0;
	std::string m_CommentDraft;
	bool m_FocusCommentTitle = false;
	ImVec2 m_PinMenuPosition{};
	Engine::GraphNodeId m_SelectedNode = 0;
	Engine::GraphLinkId m_SelectedLink = 0;
	std::vector<Engine::GraphNodeId> m_SelectedNodes;
	std::vector<Engine::GraphLinkId> m_SelectedLinks;

	EWireStyle m_WireStyle = EWireStyle::Curved;
	bool m_AutoCompileEnabled = true;
	float m_LiveCompileCooldown = 0.0f;
	std::uint64_t m_LastCompiledRevision = 0;
	Engine::ResourceHandle<Engine::Shader> m_LiveVertexShader;
	Engine::ResourceHandle<Engine::Shader> m_LivePixelShader;
	bool m_NeedsNavigateToContent = true;
};

END
