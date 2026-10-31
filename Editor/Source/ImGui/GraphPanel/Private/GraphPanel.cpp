#include "GraphPanel.h"
#include "GraphMessageLocalization.h"
#include "LocalizationManager.h"
#include "ReflectionRegistry.h"
#include "FileDialogs.h"
#include "PIEManager.h"
#include "Logger.h"
#include "HotReloadManager.h"
#include "ModuleHotReloadManager.h"
#include <imgui_node_editor.h>

namespace ed = ax::NodeEditor;

#pragma region Constructor&Destructor
GraphPanel::GraphPanel()
{
	m_Name = L"Graph"; m_DisplayNameKey = "GraphPanel.Title";
	m_LastDiagnostic = m_Registry.RegisterBuiltins();
	// ── 코멘트 그룹 노드 등록 ──
	Engine::GraphNodeDefinition commentDef;
	commentDef.Type = "Core.Comment";
	commentDef.DisplayName = "Comment";
	commentDef.Category = "Organization";
	commentDef.Domains = { Engine::EGraphDomain::Logic, Engine::EGraphDomain::Shader };

	// 크기(Size) 저장을 위한 Float2 핀 정의 (내부 저장용)
	Engine::GraphPin sizePin;
	sizePin.Key = "Size";
	sizePin.DisplayName = "Size";
	sizePin.Type = Engine::EGraphPinType::Float2;
	sizePin.Direction = Engine::EGraphPinDirection::Input;
	sizePin.DefaultValue = std::array<float, 2>{ 400.0f, 250.0f };
	commentDef.Pins.push_back(sizePin);

	(void)m_Registry.Register(std::move(commentDef));
	m_Ready = m_LastDiagnostic.Succeeded();
	if (m_Ready && ReflectionRegistry::IsCreated())
	{
		m_ExecutionReflection = ReflectionRegistry::Get().GetRegistry();
		m_LastDiagnostic = RegisterReflectedGraphNodes(m_Registry, m_ExecutionReflection);
	}

	Engine::GraphSystem::SetNodeExecuteHook([this](const std::wstring& path, Engine::GraphNodeId node)
		{
			const auto key = std::filesystem::path(path).filename().wstring();
			auto it = m_Breakpoints.find(key);
			if (it != m_Breakpoints.end() && it->second.contains(node))
			{
				m_HitBreakpointNode = node;
				if (PIEManager::IsCreated())
				{
					PIEManager::Get().Pause(); // 에디터가 게임을 즉시 일시정지!
				}
				BAM_LOG(Warn, "Editor", "Breakpoint", "Breakpoint HIT at node #{}! Game paused.", node);
			}
		});

	if (HotReloadManager::IsCreated())
	{
		HotReloadManager::Get().GetModuleReloader().RegisterPostReloadCallback([this]()
			{
				if (ReflectionRegistry::IsCreated())
				{
					m_ExecutionReflection = ReflectionRegistry::Get().GetRegistry();
					m_Registry.ClearReflectedNodes();
					(void)RegisterReflectedGraphNodes(m_Registry, m_ExecutionReflection);
				}
				m_NodeCandidates.clear(); // 우클릭 검색 팔레트 목록 즉시 갱신

				m_Logic.Data.Reconcile(m_Registry);
				m_Shader.Data.Reconcile(m_Registry);

				BuildDocumentViews(m_Logic); // 캔버스 노드의 신규 핀 실시간 갱신
				BuildDocumentViews(m_Shader);
				BAM_LOG(Info, "GraphPanel", "HotReload", "Graph nodes and pins synchronized with hot-reloaded module.");
			});
	}
}

GraphPanel::~GraphPanel()
{
	Free();
	Engine::GraphSystem::SetNodeExecuteHook(nullptr);
}

void GraphPanel::Free()
{
	InvalidateExecution();
	m_ExecutionReflection = {};
	for (auto* document : { &m_Logic, &m_Shader })
	{
		if (document->Context)
		{
			ed::DestroyEditor(document->Context);
			document->Context = nullptr;
		}
		document->ShaderBuild = {};
	}
	m_Ready = false;
	__super::Free();
}
#pragma endregion

#pragma region Loop
void GraphPanel::Draw()
{
	if (!m_Open)
	{
		SuspendDocument();
		m_Focused = false;
		m_Hovered = false;
		return;
	}

	auto* previousContext = ed::GetCurrentEditor();
	GraphDocument* previousDocument = nullptr;
	for (auto* document : { &m_Logic, &m_Shader })
	{
		if (previousContext && previousContext == document->Context)
			previousDocument = document;
	}
	const string windowTitle = LOCAL("GraphPanel.Title") + "###GraphPanel";
	ImGui::SetNextWindowSize(ImVec2(1000.0f, 650.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(windowTitle.c_str(), &m_Open, ImGuiWindowFlags_MenuBar))
	{
		m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		if (m_Focused) s_ActiveGraphPanel = this;
		m_Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
		if (m_Ready)
		{
			ActivateDocument();
			// ── 윈도우 상단 드롭다운 메뉴바 ──
			DrawMenuBar();
			// ── 메뉴바 아래 단일 행 퀵 액세스 툴바 ──
			DrawToolbar();
			UpdateExecution();
			UpdateLiveDebugging();
			ImGui::Separator();

			const float footerHeight = ImGui::GetTextLineHeightWithSpacing() * 2 +
				ImGui::GetStyle().CellPadding.y * 2 + ImGui::GetStyle().ItemSpacing.y;
			const float height = std::max(1.0f, ImGui::GetContentRegionAvail().y - footerHeight);
			ImGui::PushID(static_cast<int>(m_Domain));
			if (ImGui::BeginTable("##GraphWorkspace", 2, ImGuiTableFlags_Resizable, ImVec2(0.0f, height)))
			{
				ImGui::TableSetupColumn(LOCAL_LABEL("GraphPanel.Canvas").c_str(), ImGuiTableColumnFlags_WidthStretch, 3.0f);
				ImGui::TableSetupColumn(LOCAL_LABEL("GraphPanel.Properties").c_str(), ImGuiTableColumnFlags_WidthStretch, 1.0f);
				ImGui::TableNextColumn();
				DrawCanvas(height);
				ImGui::TableNextColumn();
				if (ImGui::BeginChild("##GraphProperties", ImVec2(0.0f, height)))
				{
					DrawProperties();
					ImGui::Separator();
					DrawVariables();
					ImGui::Separator();
					DrawDiagnostics();
					ImGui::Separator();
					DrawExecutionTrace();
					DrawShaderBuildDetails();
				}
				ImGui::EndChild();
				ImGui::EndTable();
			}
			else
			{
				SuspendDocument();
				m_Executor.Pause();
			}
			ImGui::PopID();
		}
		DrawStatus();
	}
	else
	{
		SuspendDocument();
		m_Focused = false;
		m_Hovered = false;
	}
	ImGui::End();
	ed::SetCurrentEditor(previousDocument ? previousDocument->Context : previousContext);
}
#pragma endregion

#pragma region DrawUI
void GraphPanel::DrawMenuBar()
{
	if (!ImGui::BeginMenuBar()) return;
	// 1. [파일 (File)] 메뉴
	if (ImGui::BeginMenu(LOCAL_LABEL("UI_FILE").c_str()))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.Open").c_str(), "Ctrl+O"))
		{
			if (GetDocument().Data.GetNodes().empty()) OpenDocument();
			else ImGui::OpenPopup(LOCAL_LABEL("GraphPanel.ConfirmOpen").c_str());
		}
		if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.Save").c_str(), "Ctrl+S"))
			SaveDocument(false);
		if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.SaveAs").c_str(), "Ctrl+Shift+S"))
			SaveDocument(true);
		ImGui::EndMenu();
	}
	// 2. [편집 (Edit)] 메뉴
	if (ImGui::BeginMenu(LOCAL_LABEL("UI_EDIT").c_str()))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.AddNode").c_str(), "Space"))
		{
			const auto mouse = InputManager::Get().GetScreenMousePosition();
			m_AddPosition = ed::ScreenToCanvas(ImVec2(mouse.x, mouse.y));
			PrepareNodeMenu();
			m_OpenNodePopup = true;
		}
		const bool hasSelection = !GetDocument().SelectedNodes.empty() || !GetDocument().SelectedLinks.empty();
		if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.Delete").c_str(), "Del", false, hasSelection))
			DeleteSelection();
		ImGui::Separator();
		if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.Validate").c_str(), "F7"))
			RefreshValidation();
		ImGui::EndMenu();
	}
	// 3. [보기 (View)] 메뉴
	if (ImGui::BeginMenu(LOCAL_LABEL("UI_WINDOW").c_str()))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.ResetView").c_str(), "F"))
			ed::NavigateToContent(0.5f);
		ImGui::Separator();
		if (ImGui::BeginMenu(LOCAL_LABEL("GraphPanel.Connection").c_str()))
		{
			if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.WireElectronic").c_str(), nullptr, m_WireStyle == EWireStyle::ElectronicHybrid))
				m_WireStyle = EWireStyle::ElectronicHybrid;
			if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.WireStraight").c_str(), nullptr, m_WireStyle == EWireStyle::Straight))
				m_WireStyle = EWireStyle::Straight;
			if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.WireCurved").c_str(), nullptr, m_WireStyle == EWireStyle::Curved))
				m_WireStyle = EWireStyle::Curved;
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}
	// 4. [모드 (Mode/Domain)] 메뉴
	if (ImGui::BeginMenu(LOCAL_LABEL("GraphPanel.Title").c_str()))
	{
		if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.Logic").c_str(), nullptr, m_Domain == EGraphDomain::Logic))
		{
			if (m_Domain != EGraphDomain::Logic)
			{
				SuspendDocument();
				m_Domain = EGraphDomain::Logic;
				ActivateDocument();
				m_LastDiagnostic = {};
			}
		}
		if (ImGui::MenuItem(LOCAL_LABEL("GraphPanel.Shader").c_str(), nullptr, m_Domain == EGraphDomain::Shader))
		{
			if (m_Domain != EGraphDomain::Shader)
			{
				SuspendDocument();
				m_Domain = EGraphDomain::Shader;
				ActivateDocument();
				m_LastDiagnostic = {};
			}
		}
		ImGui::EndMenu();
	}
	// 5. 도메인별 전용 메뉴 (Logic: Debug, Shader: Build)
	if (m_Domain == EGraphDomain::Logic) DrawExecutionMenu();
	else if (m_Domain == EGraphDomain::Shader) DrawShaderMenu();
	ImGui::EndMenuBar();
}
void GraphPanel::DrawToolbar()
{
	// ── 1줄 퀵 액세스 툴바: 도메인 콤보 + 핵심 빠른 실행 액션 + 파일명 ──
	const char* domainKey = m_Domain == EGraphDomain::Logic ? "GraphPanel.Logic" : "GraphPanel.Shader";
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7.5f);
	if (ImGui::BeginCombo("##GraphDomain", LOCAL_CSTR(domainKey)))
	{
		const EGraphDomain domains[]{ EGraphDomain::Logic, EGraphDomain::Shader };
		for (auto domain : domains)
		{
			const char* key = domain == EGraphDomain::Logic ? "GraphPanel.Logic" : "GraphPanel.Shader";
			ImGui::PushID(static_cast<int>(domain));
			if (ImGui::Selectable(LOCAL_CSTR(key), domain == m_Domain))
			{
				SuspendDocument();
				m_Domain = domain;
				ActivateDocument();
				m_LastDiagnostic = {};
			}
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}
	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();
	// 도메인별 퀵 액션 라인
	if (m_Domain == EGraphDomain::Logic)
	{
		DrawExecutionToolbar();
		DrawDebugToolbar();
	}
	else if (m_Domain == EGraphDomain::Shader)
	{
		DrawShaderToolbar();
	}
	ImGui::SameLine();
	DrawFileToolbar();
}

void GraphPanel::SaveDocument(bool saveAs)
{
	auto& document = GetDocument();
	auto path = document.FilePath;

	if (saveAs || path.empty())
	{
		const std::wstring defaultName = path.empty()
			? (m_Domain == EGraphDomain::Logic ? L"LogicGraph.json" : L"ShaderGraph.json")
			: path.filename().wstring();

		std::wstring selectedPath;
		const std::vector<FileDialogFilter> filters{
			{ StrToWStr(LOCAL("FileDialog.GraphFiles")), L"*.json" }
		};

		if (!FileDialogs::SaveFileDialog(selectedPath, filters, defaultName, L"json", EFileDialogContext::GraphSave))
			return;
		path = std::filesystem::path(selectedPath);
	}

	if (!ReflectionRegistry::IsCreated())
	{
		document.FileResult = {
			EGraphFileStage::Serialize,
			{ archive::ArchiveErrorCode::UnsupportedType, "ReflectionRegistry" }
		};
		return;
	}

	const auto& reflection = ReflectionRegistry::Get();
	document.FileResult = SaveGraphJson(path, document.Data, m_Registry,
		reflection.GetRegistry(), reflection.GetPropertyCodecs());

	if (document.FileResult.Succeeded()) document.FilePath = std::move(path);
}

void GraphPanel::OpenDocument()
{
	std::wstring selectedPath;
	const std::vector<FileDialogFilter> filters{
		{ StrToWStr(LOCAL("FileDialog.GraphFiles")), L"*.json" }
	};

	if (!FileDialogs::OpenFileDialog(selectedPath, filters, L"", EFileDialogContext::GraphOpen))
		return;

	auto& document = GetDocument();
	if (!ReflectionRegistry::IsCreated())
	{
		document.FileResult = {
			EGraphFileStage::Deserialize,
			{ archive::ArchiveErrorCode::UnsupportedType, "ReflectionRegistry" }
		};
		return;
	}

	GraphDocument candidate(m_Domain);
	candidate.FilePath = std::filesystem::path(selectedPath);

	const auto& reflection = ReflectionRegistry::Get();
	const auto loaded = LoadGraphJson(candidate.FilePath, candidate.Data, m_Registry,
		reflection.GetRegistry(), reflection.GetPropertyCodecs());

	if (!loaded.Succeeded())
	{
		document.FileResult = loaded;
		return;
	}

	// 로드된 그래프 파일의 변수 노드 스키마 등록
	candidate.Data.RegisterVariableNodes(m_Registry);

	if (candidate.Data.GetDomain() != m_Domain)
	{
		document.FileResult = {
			EGraphFileStage::Validate, {},
			{ EGraphError::UnsupportedDomain, 0, {}, 0, "GraphPanel.FileDomainMismatch" }
		};
		return;
	}

	const auto views = BuildDocumentViews(candidate);
	if (!views.Succeeded())
	{
		document.FileResult = { EGraphFileStage::Validate, {}, views };
		return;
	}

	candidate.Diagnostics = candidate.Data.Validate(m_Registry);

	ed::Config config;
	config.SettingsName = (m_Domain == EGraphDomain::Logic) ? "LogicGraph" : "ShaderGraph";
	auto* freshContext = ed::CreateEditor(&config);
	ed::SetCurrentEditor(freshContext);

	std::size_t index = 0;
	for (const auto& node : candidate.Data.GetNodes())
	{
		const float x = 40.0f + static_cast<float>(index % 3) * 420.0f;
		const float y = 40.0f + static_cast<float>(index / 3) * 360.0f;
		ed::SetNodePosition(candidate.Nodes.at(node.Id).Id, ImVec2(x, y));
		++index;
	}

	if (m_Domain == EGraphDomain::Logic)
	{
		InvalidateExecution();
		m_EntryNode = 0;
	}

	std::swap(document, candidate);
	document.Context = freshContext;
	if (candidate.Context) ed::DestroyEditor(candidate.Context);

	m_LastDiagnostic = {};
	m_AddPosition = ImVec2(40.0f, 40.0f);
	m_DragPin = 0;
	m_AddSource = {};
	m_NodeCandidates.clear();
	m_NodeSearch[0] = '\0';
	m_NodeMenuActive = false;
	m_FocusNodeSearch = false;
	m_OpenNodePopup = false;
}

GraphDiagnostic GraphPanel::BuildDocumentViews(GraphDocument& document)
{
	document.Nodes.clear();
	document.Pins.clear();
	document.Links.clear();
	document.NextId = 1;

	const auto maximum = static_cast<std::uint64_t>(std::numeric_limits<int>::max());
	const auto allocateId = [&]() -> int
		{
			if (document.NextId > maximum) return 0;
			return static_cast<int>(document.NextId++);
		};

	for (const auto& node : document.Data.GetNodes())
	{
		const auto* definition = m_Registry.Find(node.Type);
		if (!definition)
		{
			return { EGraphError::InvalidDefinition, node.Id, {}, 0,
				"GraphPanel.UnregisteredNode" };
		}

		NodeView view;
		view.Id = allocateId();
		if (view.Id == 0)
			return { EGraphError::IdExhausted, node.Id, {}, 0, "GraphPanel.IdExhausted" };

		for (const auto& pin : definition->Pins)
		{
			const int pinId = allocateId();
			if (pinId == 0)
				return { EGraphError::IdExhausted, node.Id, pin.Key, 0, "GraphPanel.IdExhausted" };

			view.Pins.emplace(pin.Key, pinId);
			document.Pins.emplace(pinId, GraphPinReference{ node.Id, pin.Key });
		}

		document.Nodes.emplace(node.Id, std::move(view));
	}

	for (const auto& link : document.Data.GetLinks())
	{
		const int linkId = allocateId();
		if (linkId == 0)
			return { EGraphError::IdExhausted, 0, {}, link.Id, "GraphPanel.IdExhausted" };

		document.Links.emplace(link.Id, linkId);
	}

	return {};
}

void GraphPanel::DrawFileToolbar()
{
	bool openRequested = false;
	const ImVec2 center = ImGui::GetMainViewport()->GetCenter();
	ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowSize(ImVec2(320.0f, 0.0f), ImGuiCond_Appearing);
	if (ImGui::BeginPopupModal(LOCAL_LABEL("GraphPanel.ConfirmOpen").c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::PushTextWrapPos(300.0f);
		ImGui::TextUnformatted(LOCAL_CSTR("GraphPanel.ReplaceWarning"));
		ImGui::PopTextWrapPos();
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		if (ImGui::Button(LOCAL_LABEL("GraphPanel.ReplaceCurrent").c_str(), ImVec2(120.0f, 0.0f)))
		{
			ImGui::CloseCurrentPopup();
			openRequested = true;
		}
		ImGui::SameLine(0.0f, 8.0f);
		if (ImGui::Button(LOCAL_LABEL("EditorCommon.Cancel").c_str(), ImVec2(120.0f, 0.0f)))
			ImGui::CloseCurrentPopup();
		ImGui::EndPopup();
	}
	if (openRequested) OpenDocument();
	const auto& document = GetDocument();
	const std::string filenameText = document.FilePath.empty()
		? LOCAL("GraphPanel.NoFile")
		: WStrToStr(document.FilePath.filename().wstring());
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();
	ImGui::TextDisabled("[%s]", filenameText.c_str());
	if (ImGui::IsItemHovered())
	{
		const std::string fullPathText = document.FilePath.empty()
			? LOCAL("GraphPanel.NoFile")
			: LOCAL_FMT("GraphPanel.FilePath", WStrToStr(document.FilePath.wstring()));
		ImGui::SetTooltip("%s", fullPathText.c_str());
	}
}

void GraphPanel::PrepareNodeMenu(GraphPinReference source)
{
	m_AddSource = std::move(source);

	// ── GameModule 등 외부 모듈에서 새로 등록된 리플렉션 노드를 항상 최신 동기화 ──
	if (ReflectionRegistry::IsCreated())
	{
		m_ExecutionReflection = ReflectionRegistry::Get().GetRegistry();
		m_Registry.ClearReflectedNodes();
		(void)RegisterReflectedGraphNodes(m_Registry, m_ExecutionReflection);
	}

	m_NodeCandidates = FindGraphNodeCandidates(GetDocument().Data, m_Registry, m_AddSource);
	m_NodeSearch[0] = '\0';
	m_NodeMenuActive = true;
	m_FocusNodeSearch = true;
}

void GraphPanel::DrawNodeMenu()
{
	if (!m_NodeMenuActive)
	{
		ImGui::CloseCurrentPopup();
		return;
	}
	if (m_AddSource.Node != 0)
	{
		const auto* node = GetDocument().Data.FindNode(m_AddSource.Node);
		const auto* pin = node ? m_Registry.FindPin(node->Type, m_AddSource.Pin) : nullptr;
		if (!pin) { ImGui::CloseCurrentPopup(); return; }
		const char* key = pin->Direction == EGraphPinDirection::Output ?
			"GraphPanel.ConnectFrom" : "GraphPanel.ConnectTo";
		ImGui::TextDisabled("%s: %s / %s", LOCAL_CSTR(key), pin->Key.c_str(),
			pin->Type == EGraphPinType::Object ? pin->ObjectType.c_str() : PinTypeName(pin->Type));
	}
	if (m_FocusNodeSearch)
	{
		ImGui::SetKeyboardFocusHere();
		m_FocusNodeSearch = false;
	}
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 28.0f);
	ImGui::InputTextWithHint("##NodeSearch", LOCAL_CSTR("GraphPanel.SearchNodes"),
		m_NodeSearch, IM_ARRAYSIZE(m_NodeSearch));
	ImGui::Separator();

	struct NodeEntry
	{
		std::string Type;
		std::string DisplayName;
		std::string Tooltip;
		std::vector<std::string> CompatiblePins;
	};

	struct CategoryNode
	{
		std::string DisplayName;
		std::map<std::string, CategoryNode> SubCategories;
		std::vector<NodeEntry> Nodes;
	};

	const bool hasSearch = m_NodeSearch[0] != '\0';
	CategoryNode root;

	for (const auto& candidate : m_NodeCandidates)
	{
		const auto* definition = m_Registry.Find(candidate.Type);
		if (!definition) continue;
		const string name = definition->DisplayName.empty() ? candidate.Type : LOCAL(definition->DisplayName);
		const string categoryRaw = definition->Category.empty() ? "General" : definition->Category;
		string label = LOCAL(categoryRaw) + " / " + name;
		if (!candidate.Pin.empty())
		{
			const auto* pin = m_Registry.FindPin(candidate.Type, candidate.Pin);
			if (pin) label += " / " + (pin->DisplayName.empty() ? pin->Key : LOCAL(pin->DisplayName));
		}
		const string searchText = label + " " + LOCAL(definition->Tooltip);
		if (!MatchesGraphNodeSearch(*definition, candidate, m_NodeSearch, searchText)) continue;

		CategoryNode* current = &root;
		std::string_view remaining = categoryRaw;
		while (!remaining.empty())
		{
			const auto slash = remaining.find('/');
			const auto part = slash == std::string_view::npos ? remaining : remaining.substr(0, slash);
			std::string partStr(part);
			auto& child = current->SubCategories[partStr];
			if (child.DisplayName.empty()) child.DisplayName = LOCAL(partStr);
			current = &child;
			if (slash == std::string_view::npos) break;
			remaining.remove_prefix(slash + 1);
		}

		auto nodeIt = std::find_if(current->Nodes.begin(), current->Nodes.end(),
			[&](const NodeEntry& n) { return n.Type == candidate.Type; });
		if (nodeIt == current->Nodes.end())
		{
			current->Nodes.push_back({ candidate.Type, name, definition->Tooltip, {} });
			nodeIt = current->Nodes.end() - 1;
		}
		if (!candidate.Pin.empty()) nodeIt->CompatiblePins.push_back(candidate.Pin);
	}

	ImGui::BeginChild("##NodeResults", ImVec2(ImGui::GetFontSize() * 28.0f, ImGui::GetTextLineHeightWithSpacing() * 18));

	auto DrawCategory = [&](auto& self, const CategoryNode& cat, bool isRoot) -> void
		{
			for (const auto& [rawName, subCat] : cat.SubCategories)
			{
				ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_SpanAvailWidth;
				if (hasSearch) flags |= ImGuiTreeNodeFlags_DefaultOpen;

				if (ImGui::TreeNodeEx(subCat.DisplayName.c_str(), flags))
				{
					self(self, subCat, false);
					ImGui::TreePop();
				}
			}

			for (const auto& nodeItem : cat.Nodes)
			{
				if (nodeItem.CompatiblePins.size() > 1)
				{
					ImGuiTreeNodeFlags nodeFlags = ImGuiTreeNodeFlags_SpanAvailWidth;
					if (hasSearch) nodeFlags |= ImGuiTreeNodeFlags_DefaultOpen;

					if (ImGui::TreeNodeEx((nodeItem.DisplayName + "###" + nodeItem.Type).c_str(), nodeFlags))
					{
						for (const auto& pinKey : nodeItem.CompatiblePins)
						{
							const auto* pin = m_Registry.FindPin(nodeItem.Type, pinKey);
							const string pinLabel = pin && !pin->DisplayName.empty() ? LOCAL(pin->DisplayName) : pinKey;

							ImGui::PushID(nodeItem.Type.c_str());
							ImGui::PushID(pinKey.c_str());
							if (ImGui::Selectable(pinLabel.c_str(), false, ImGuiSelectableFlags_NoAutoClosePopups))
							{
								if (AddNode({ nodeItem.Type, pinKey })) ImGui::CloseCurrentPopup();
							}
							ImGui::PopID();
							ImGui::PopID();
						}
						ImGui::TreePop();
					}
				}
				else
				{
					const std::string pinKey = nodeItem.CompatiblePins.empty() ? "" : nodeItem.CompatiblePins.front();
					std::string displayLabel = nodeItem.DisplayName;
					if (!pinKey.empty())
					{
						const auto* pin = m_Registry.FindPin(nodeItem.Type, pinKey);
						displayLabel += " (" + (pin && !pin->DisplayName.empty() ? LOCAL(pin->DisplayName) : pinKey) + ")";
					}

					ImGui::PushID(nodeItem.Type.c_str());
					ImGui::PushID(pinKey.c_str());
					if (ImGui::Selectable((displayLabel + "###Item").c_str(), false, ImGuiSelectableFlags_NoAutoClosePopups))
					{
						if (AddNode({ nodeItem.Type, pinKey })) ImGui::CloseCurrentPopup();
					}
					if (ImGui::IsItemHovered())
					{
						ImGui::BeginTooltip();
						ImGui::TextUnformatted(nodeItem.Type.c_str());
						if (!nodeItem.Tooltip.empty()) ImGui::TextUnformatted(LOCAL_CSTR(nodeItem.Tooltip));
						ImGui::EndTooltip();
					}
					ImGui::PopID();
					ImGui::PopID();
				}
			}
		};

	const bool any = !root.SubCategories.empty() || !root.Nodes.empty();
	if (any) DrawCategory(DrawCategory, root, true);
	else ImGui::TextDisabled("%s", LOCAL_CSTR("GraphPanel.NoMatchingNodes"));

	ImGui::EndChild();
}

void GraphPanel::DrawProperties()
{
	auto& document = GetDocument();
	if (document.SelectedNodes.size() + document.SelectedLinks.size() > 1)
	{
		ImGui::TextWrapped("%s", LOCAL_CSTR("GraphPanel.MultipleSelection"));
		return;
	}
	const auto selectedNode = document.SelectedNodes.empty() ? 0 : document.SelectedNodes.front();
	const auto* node = document.Data.FindNode(selectedNode);
	if (node)
	{
		ImGui::TextUnformatted(node->Type.c_str());
		ImGui::TextDisabled("ID: %llu", static_cast<unsigned long long>(node->Id));
		const auto* definition = m_Registry.Find(node->Type);
		if (!definition) return;
		if (!definition->Tooltip.empty()) ImGui::TextWrapped("%s", LOCAL_CSTR(definition->Tooltip));
		ImGui::PushID(std::to_string(node->Id).c_str());
		for (const auto& pin : definition->Pins)
		{
			if (pin.Direction == EGraphPinDirection::Input && pin.Type != EGraphPinType::Execution)
				DrawInputValue(*node, pin);
		}
		ImGui::PopID();
	}
	else if (document.SelectedNodes.empty() && document.SelectedLinks.size() == 1)
	{
		for (const auto& link : document.Data.GetLinks())
		{
			if (link.Id != document.SelectedLinks.front()) continue;
			ImGui::TextUnformatted(LOCAL_CSTR("GraphPanel.Connection"));
			const string from = fmt::format("{} / {}", link.From.Node, link.From.Pin);
			const string to = fmt::format("{} / {}", link.To.Node, link.To.Pin);
			ImGui::TextUnformatted(from.c_str());
			ImGui::TextUnformatted(to.c_str());
			break;
		}
		if (ImGui::Button(LOCAL_CSTR("GraphPanel.Disconnect"))) DeleteSelection();
	}
	else
	{
		ImGui::TextWrapped("%s", LOCAL_CSTR("GraphPanel.SelectHint"));
	}
}

void GraphPanel::DrawVariables()
{
	if (m_Domain != Engine::EGraphDomain::Logic) return;

	auto& document = GetDocument();
	ImGui::TextUnformatted(LOCAL_CSTR("GraphPanel.Variables"));
	ImGui::Separator();

	// 1. 기존 변수 목록 표시 및 Get/Set 노드 생성
	const auto& vars = document.Data.GetVariables();
	if (vars.empty())
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("GraphPanel.NoVariables"));
	}
	else
	{
		for (size_t i = 0; i < vars.size(); ++i)
		{
			const auto& var = vars[i];
			ImGui::PushID(static_cast<int>(i));

			ImGui::BulletText("%s (%s)", var.Name.c_str(), PinTypeName(var.Type));
			ImGui::SameLine();
			if (ImGui::SmallButton(LOCAL_LABEL("GraphPanel.AddVariableGet").c_str()))
			{
				m_AddSource = {};

				const auto mouse = InputManager::Get().GetScreenMousePosition();
				m_AddPosition = ed::ScreenToCanvas(ImVec2(mouse.x, mouse.y));
				GraphNodeCandidate candidate;
				candidate.Type = "Variable.Get." + var.Name;
				AddNode(candidate);
			}
			ImGui::SameLine();
			if (ImGui::SmallButton(LOCAL_LABEL("GraphPanel.AddVariableSet").c_str()))
			{
				m_AddSource = {};
				const auto mouse = InputManager::Get().GetScreenMousePosition();
				m_AddPosition = ed::ScreenToCanvas(ImVec2(mouse.x, mouse.y));
				GraphNodeCandidate candidate;
				candidate.Type = "Variable.Set." + var.Name;
				AddNode(candidate);
			}
			ImGui::SameLine();

			if (ImGui::SmallButton(ICON_FA_XMARK))
			{
				(void)document.Data.RemoveVariable(m_Registry, var.Name);
				ImGui::PopID();
				break;
			}

			ImGui::PopID();
		}
	}

	ImGui::Spacing();
	ImGui::Separator();

	// 2. 새 변수 추가 UI
	ImGui::TextUnformatted(LOCAL_CSTR("GraphPanel.AddVariable"));
	const string nameLabel = LOCAL("EditorCommon.Name") + "###GraphNewVarName";
	ImGui::InputText(nameLabel.c_str(), m_NewVarName, sizeof(m_NewVarName));

	const char* typeNames[] = { "Bool", "Int", "Float", "Float2", "Float3", "Float4" };
	const Engine::EGraphPinType typeValues[] = {
		Engine::EGraphPinType::Bool, Engine::EGraphPinType::Int, Engine::EGraphPinType::Float,
		Engine::EGraphPinType::Float2, Engine::EGraphPinType::Float3, Engine::EGraphPinType::Float4
	};

	const string typeLabel = LOCAL("EditorCommon.Type") + "###GraphNewVarType";
	ImGui::Combo(typeLabel.c_str(), &m_NewVarType, typeNames, IM_ARRAYSIZE(typeNames));

	if (ImGui::Button(LOCAL_LABEL("GraphPanel.CreateVariable").c_str()) && m_NewVarName[0] != '\0')
	{
		Engine::GraphVariable newVar;
		newVar.Name = m_NewVarName;
		newVar.Type = typeValues[m_NewVarType];
		newVar.DefaultValue = MakeDefaultValue(newVar.Type);

		if (document.Data.AddVariable(m_Registry, std::move(newVar)).Succeeded())
		{
			m_NewVarName[0] = '\0';
		}
	}
}

void GraphPanel::DrawInputValue(const GraphNode& node, const GraphPin& pin)
{
	auto& graph = GetDocument().Data;
	const GraphPinReference reference{ node.Id, pin.Key };
	const bool connected = std::any_of(graph.GetLinks().begin(), graph.GetLinks().end(),
		[&](const GraphLink& link) { return link.To == reference; });
	const auto found = node.InputValues.find(pin.Key);
	const bool overridden = found != node.InputValues.end();
	GraphValue value = overridden ? found->second : pin.DefaultValue;
	const bool missing = std::holds_alternative<std::monostate>(value);
	if (missing) value = MakeDefaultValue(pin.Type);

	// 핀 고유 키로 확실한 ID 스택 격리
	ImGui::PushID(pin.Key.c_str());
	ImGui::Separator();
	ImGui::TextUnformatted((pin.DisplayName.empty() ? pin.Key : LOCAL(pin.DisplayName)).c_str());
	if (connected) ImGui::TextDisabled("%s", LOCAL_CSTR("GraphPanel.Connected"));
	if (pin.Type == EGraphPinType::Object)
	{
		ImGui::TextDisabled("%s", pin.ObjectType.c_str());
		ImGui::TextWrapped("%s", LOCAL_CSTR("GraphPanel.ObjectLinkOnly"));
		ImGui::PopID();
		return;
	}
	ImGui::BeginDisabled(connected);
	ImGui::SetNextItemWidth(-1.0f);
	if (DrawValue(pin.Type, value)) ApplyResult(graph.SetInputValue(m_Registry, reference, value));

	// ## 접미사로 중복 ID 충돌 원천 차단!
	const std::string useLabel = fmt::format("{}##Use_{}", LOCAL_CSTR("GraphPanel.UseValue"), pin.Key);
	if (missing && ImGui::SmallButton(useLabel.c_str()))
		ApplyResult(graph.SetInputValue(m_Registry, reference, value));

	const std::string resetLabel = fmt::format("{}##Reset_{}", LOCAL_CSTR("GraphPanel.ResetValue"), pin.Key);
	if (overridden && ImGui::SmallButton(resetLabel.c_str()))
		ApplyResult(graph.ResetInputValue(m_Registry, reference));

	ImGui::EndDisabled();
	ImGui::PopID();
}

bool GraphPanel::DrawValue(EGraphPinType type, GraphValue& value)
{
	switch (type)
	{
	case EGraphPinType::Bool: return ImGui::Checkbox("##Value", &std::get<bool>(value));
	case EGraphPinType::Int: return ImGui::InputScalar("##Value", ImGuiDataType_S64, &std::get<std::int64_t>(value));
	case EGraphPinType::Float: return ImGui::InputFloat("##Value", &std::get<float>(value), 0.0f, 0.0f, "%.6g");
	case EGraphPinType::Float2: return ImGui::InputFloat2("##Value", std::get<std::array<float, 2>>(value).data(), "%.6g");
	case EGraphPinType::Float3: return ImGui::InputFloat3("##Value", std::get<std::array<float, 3>>(value).data(), "%.6g");
	case EGraphPinType::Float4: return ImGui::InputFloat4("##Value", std::get<std::array<float, 4>>(value).data(), "%.6g");
	case EGraphPinType::String:
	{
		auto& str = std::get<std::string>(value);
		char buffer[256];
		strncpy_s(buffer, str.c_str(), sizeof(buffer));
		if (ImGui::InputText("##Value", buffer, sizeof(buffer)))
		{
			str = buffer;
			return true;
		}
		return false;
	}
	default: return false;
	}
}

GraphValue GraphPanel::MakeDefaultValue(EGraphPinType type)
{
	switch (type)
	{
	case EGraphPinType::Bool: return false;
	case EGraphPinType::Int: return std::int64_t{ 0 };
	case EGraphPinType::Float: return 0.0f;
	case EGraphPinType::Float2: return std::array<float, 2>{};
	case EGraphPinType::Float3: return std::array<float, 3>{};
	case EGraphPinType::Float4: return std::array<float, 4>{};
	case EGraphPinType::String: return std::string{};
	default: return {};
	}
}

void GraphPanel::DrawDiagnostics()
{
	const auto& document = GetDocument();
	const string title = LOCAL("GraphPanel.Validation") + "###GraphValidation";
	if (!ImGui::CollapsingHeader(title.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) return;
	if (document.Diagnostics.empty()) ImGui::TextDisabled("%s", LOCAL_CSTR("GraphPanel.Valid"));
	for (const auto& diagnostic : document.Diagnostics)
	{
		const string location = fmt::format("{} / {}", diagnostic.Node, diagnostic.Pin);
		ImGui::TextColored(ImVec4(1.0f, 0.55f, 0.35f, 1.0f), "%s", location.c_str());
		ImGui::TextWrapped("%s", LocalizeGraphMessage(diagnostic.Message).c_str());
	}
}

void GraphPanel::DrawStatus()
{
	if (!m_LastDiagnostic.Succeeded())
	{
		const string message = fmt::format("{} / {}: {}", m_LastDiagnostic.Node,
			m_LastDiagnostic.Pin, LocalizeGraphMessage(m_LastDiagnostic.Message));
		ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", message.c_str());
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", message.c_str());
	}
	else
	{
		ImGui::TextDisabled("%s", LOCAL_CSTR("GraphPanel.SaveScope"));
	}
	ImGui::TextDisabled("%s", LOCAL_CSTR("GraphPanel.Controls"));
}
#pragma endregion

#pragma region Debugging
void GraphPanel::ToggleBreakpoint(Engine::GraphNodeId node)
{
	if (node == 0) return;
	const auto key = GetDocument().FilePath.filename().wstring();
	if (key.empty()) return;

	auto& set = m_Breakpoints[key];
	if (set.contains(node)) set.erase(node);
	else set.insert(node);
}

bool GraphPanel::HasBreakpoint(Engine::GraphNodeId node) const
{
	if (node == 0) return false;
	const auto key = GetDocument().FilePath.filename().wstring();
	if (key.empty()) return false;

	auto it = m_Breakpoints.find(key);
	return it != m_Breakpoints.end() && it->second.contains(node);
}
#pragma endregion

#pragma region Editing
GraphPanel::GraphDocument& GraphPanel::GetDocument()
{
	return m_Domain == EGraphDomain::Logic ? m_Logic : m_Shader;
}

const GraphPanel::GraphDocument& GraphPanel::GetDocument() const
{
	return m_Domain == EGraphDomain::Logic ? m_Logic : m_Shader;
}

const Graph& GraphPanel::GetGraph() const
{
	return GetDocument().Data;
}

GraphDiagnostic GraphPanel::RegisterNode(GraphNodeDefinition definition)
{
	auto result = m_Registry.Register(std::move(definition));
	if (result.Succeeded()) InvalidateExecution();
	return result;
}

GraphDiagnostic GraphPanel::RegisterReflectionNodes(const reflection::Registry& registry)
{
	auto combined = m_ExecutionReflection;
	auto incoming = registry;
	const auto merged = combined.TryMerge(std::move(incoming));
	if (!merged.Succeeded())
		return { EGraphError::InvalidDefinition, 0, {}, 0,
			LOCAL_FMT("GraphPanel.ReflectionConflict", merged.Owner, merged.Member) };
	auto result = RegisterReflectedGraphNodes(m_Registry, registry);
	m_LastDiagnostic = result;
	if (result.Succeeded())
	{
		InvalidateExecution();
		m_ExecutionReflection = std::move(combined);
		m_Logic.Diagnostics = m_Logic.Data.Validate(m_Registry);
		m_Shader.Diagnostics = m_Shader.Data.Validate(m_Registry);
	}
	return result;
}

bool GraphPanel::AddNode(const GraphNodeCandidate& candidate)
{
	auto& document = GetDocument();
	const auto* definition = m_Registry.Find(candidate.Type);
	const auto count = definition ? definition->Pins.size() + 1 + (m_AddSource.Node != 0 ? 1 : 0) : 0;
	if (!definition || !ReserveEditorIds(count)) return false;
	const auto result = AddGraphNodeCandidate(document.Data, m_Registry, candidate, m_AddSource);
	if (!ApplyResult(result)) return false;
	NodeView view;
	view.Id = static_cast<int>(document.NextId++);
	for (const auto& pin : definition->Pins)
	{
		const int pinId = static_cast<int>(document.NextId++);
		view.Pins.emplace(pin.Key, pinId);
		document.Pins.emplace(pinId, GraphPinReference{ result.Node, pin.Key });
	}
	ed::SetNodePosition(view.Id, m_AddPosition);
	document.Nodes.emplace(result.Node, std::move(view));
	if (result.Link != 0) document.Links.emplace(result.Link, static_cast<int>(document.NextId++));
	document.PendingSelection = result.Node;
	return true;
}

void GraphPanel::DeleteSelection()
{
	auto& document = GetDocument();
	ed::ClearSelection();
	for (auto id : document.SelectedLinks)
	{
		if (ApplyResult(document.Data.Disconnect(id))) document.Links.erase(id);
	}
	for (auto id : document.SelectedNodes)
	{
		if (!ApplyResult(document.Data.RemoveNode(id))) continue;
		const auto found = document.Nodes.find(id);
		if (found == document.Nodes.end()) continue;
		for (const auto& [key, pinId] : found->second.Pins) document.Pins.erase(pinId);
		document.Nodes.erase(found);
	}
	std::erase_if(document.Links, [&](const auto& item)
	{
		const auto links = document.Data.GetLinks();
		return std::none_of(links.begin(), links.end(), [&](const GraphLink& link) { return link.Id == item.first; });
	});
	document.SelectedNodes.clear();
	document.SelectedLinks.clear();
	document.PendingSelection = 0;
	// Rebuild only the UI context before the next frame, retaining node positions and panning in memory.
	document.ResetContext = true;
	m_DragPin = 0;
	m_NodeMenuActive = false;
	m_AddSource = {};
}

void GraphPanel::RefreshValidation()
{
	auto& document = GetDocument();
	document.Diagnostics = document.Data.Validate(m_Registry);
}

void GraphPanel::ToggleBreakpointOnSelected()
{
	auto& document = GetDocument();
	if (!document.SelectedNodes.empty())
		ToggleBreakpoint(document.SelectedNodes.front());
}

bool GraphPanel::ApplyResult(GraphDiagnostic result)
{
	m_LastDiagnostic = std::move(result);
	if (!m_LastDiagnostic.Succeeded()) return false;
	if (m_Domain == EGraphDomain::Logic)
		InvalidateExecution();
	else
		InvalidateShaderBuild();
	RefreshValidation();
	return true;
}
#pragma endregion
