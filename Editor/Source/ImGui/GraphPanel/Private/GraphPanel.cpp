#include "GraphPanel.h"
#include "GraphMessageLocalization.h"
#include "LocalizationManager.h"
#include "ReflectionRegistry.h"
#include <imnodes.h>
#include <algorithm>
#include <utility>
#include "FileDialogs.h"
#include <limits>
#include <memory>

#pragma region Constructor&Destructor
GraphPanel::GraphPanel()
{
	m_Name = L"Graph"; m_DisplayNameKey = "GraphPanel.Title";
	m_LastDiagnostic = m_Registry.RegisterBuiltins();
	m_Ready = m_LastDiagnostic.Succeeded();
	if (m_Ready && ReflectionRegistry::IsCreated())
	{
		m_ExecutionReflection = ReflectionRegistry::Get().GetRegistry();
		m_LastDiagnostic = RegisterReflectedGraphNodes(m_Registry, m_ExecutionReflection);
	}
}

GraphPanel::~GraphPanel()
{
	Free();
}

void GraphPanel::Free()
{
	InvalidateExecution();
	m_ExecutionReflection = {};
	for (auto* document : { &m_Logic, &m_Shader })
	{
		if (document->Context) ImNodes::DestroyContext(document->Context);
		document->Context = nullptr;
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

	auto* previousContext = ImNodes::GetCurrentContext();
	GraphDocument* previousDocument = nullptr;
	for (auto* document : { &m_Logic, &m_Shader })
	{
		if (previousContext && previousContext == document->Context)
			previousDocument = document;
	}
	const string windowTitle = LOCAL("GraphPanel.Title") + "###GraphPanel";
	ImGui::SetNextWindowSize(ImVec2(1000.0f, 650.0f), ImGuiCond_FirstUseEver);
	if (ImGui::Begin(windowTitle.c_str(), &m_Open))
	{
		m_Focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		m_Hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
		if (m_Ready)
		{
			ActivateDocument();
			DrawToolbar();
			DrawFileToolbar();
			DrawShaderToolbar();
			DrawExecutionToolbar();
			UpdateExecution();
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
	ImNodes::SetCurrentContext(previousDocument ? previousDocument->Context : previousContext);
}
#pragma endregion

#pragma region DrawUI
void GraphPanel::DrawToolbar()
{
	const char* domainKey = m_Domain == EGraphDomain::Logic ? "GraphPanel.Logic" : "GraphPanel.Shader";
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
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
	if (ImGui::Button(LOCAL_CSTR("GraphPanel.AddNode")))
	{
		const auto pan = ImNodes::EditorContextGetPanning();
		const float offset = 24.0f * static_cast<float>(GetDocument().Data.GetNodes().size() % 6);
		m_AddPosition = ImVec2(40.0f + offset - pan.x, 40.0f + offset - pan.y);
		PrepareNodeMenu();
		m_OpenNodePopup = true;
	}
	ImGui::SameLine();
	if (ImGui::Button(LOCAL_CSTR("GraphPanel.Validate"))) RefreshValidation();
	ImGui::SameLine();
	if (ImGui::Button(LOCAL_CSTR("GraphPanel.ResetView"))) ImNodes::EditorContextResetPanning(ImVec2(0.0f, 0.0f));
	ImGui::SameLine();
	ImGui::BeginDisabled(GetDocument().SelectedNodes.empty() && GetDocument().SelectedLinks.empty());
	if (ImGui::Button(LOCAL_CSTR("GraphPanel.Delete"))) DeleteSelection();
	ImGui::EndDisabled();
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

		if (!FileDialogs::SaveFileDialog(selectedPath, filters, defaultName, L"json")) return;
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

	if (!FileDialogs::OpenFileDialog(selectedPath, filters)) return;

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

	std::unique_ptr<ImNodesContext, decltype(&ImNodes::DestroyContext)> freshContext(
		ImNodes::CreateContext(), &ImNodes::DestroyContext);
	ImNodes::SetCurrentContext(freshContext.get());

	std::size_t index = 0;
	for (const auto& node : candidate.Data.GetNodes())
	{
		const float x = 40.0f + static_cast<float>(index % 3) * 420.0f;
		const float y = 40.0f + static_cast<float>(index / 3) * 360.0f;
		ImNodes::SetNodeGridSpacePos(candidate.Nodes.at(node.Id).Id, ImVec2(x, y));
		++index;
	}

	if (m_Domain == EGraphDomain::Logic)
	{
		InvalidateExecution();
		m_EntryNode = 0;
	}

	std::swap(document, candidate);
	document.Context = freshContext.release();
	if (candidate.Context) ImNodes::DestroyContext(candidate.Context);

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
	if (ImGui::Button(LOCAL_LABEL("GraphPanel.Save").c_str())) SaveDocument(false);
	ImGui::SameLine();
	if (ImGui::Button(LOCAL_LABEL("GraphPanel.SaveAs").c_str())) SaveDocument(true);

	const auto& document = GetDocument();

	ImGui::SameLine();
	if (ImGui::Button(LOCAL_LABEL("GraphPanel.Open").c_str()))
	{
		if (GetDocument().Data.GetNodes().empty())
			OpenDocument();
		else
			ImGui::OpenPopup(LOCAL_LABEL("GraphPanel.ConfirmOpen").c_str());
	}

	bool openRequested = false;
	if (ImGui::BeginPopupModal(LOCAL_LABEL("GraphPanel.ConfirmOpen").c_str(),
		nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		ImGui::TextWrapped("%s", LOCAL_CSTR("GraphPanel.ReplaceWarning"));

		if (ImGui::Button(LOCAL_LABEL("GraphPanel.ReplaceCurrent").c_str()))
		{
			ImGui::CloseCurrentPopup();
			openRequested = true;
		}

		ImGui::SameLine();
		if (ImGui::Button(LOCAL_LABEL("EditorCommon.Cancel").c_str()))
			ImGui::CloseCurrentPopup();

		ImGui::EndPopup();
	}

	if (openRequested) OpenDocument();

	ImGui::SameLine();

	const std::string pathText = document.FilePath.empty()
		? LOCAL("GraphPanel.NoFile")
		: LOCAL_FMT("GraphPanel.FilePath", WStrToStr(document.FilePath.wstring()));

	ImGui::TextDisabled("%s", pathText.c_str());
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", pathText.c_str());

	const auto& result = document.FileResult;
	if (result.Succeeded()) return;

	const char* stageKey = "GraphPanel.FileStageUnknown";
	switch (result.Stage)
	{
	case EGraphFileStage::Validate: stageKey = "GraphPanel.FileStageValidate"; break;
	case EGraphFileStage::Serialize: stageKey = "GraphPanel.FileStageSerialize"; break;
	case EGraphFileStage::Read: stageKey = "GraphPanel.FileStageRead"; break;
	case EGraphFileStage::Deserialize: stageKey = "GraphPanel.FileStageDeserialize"; break;
	case EGraphFileStage::Write: stageKey = "GraphPanel.FileStageWrite"; break;
	case EGraphFileStage::Replace: stageKey = "GraphPanel.FileStageReplace"; break;
	default: break;
	}

	const auto message = LOCAL_FMT("GraphPanel.FileFailed", LOCAL(stageKey));
	ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", message.c_str());

	if (ImGui::IsItemHovered())
	{
		ImGui::BeginTooltip();

		if (!result.ArchiveError)
		{
			const auto detail = LOCAL_FMT("GraphPanel.ArchiveErrorDetail",
				static_cast<int>(result.ArchiveError.Code), result.ArchiveError.Field);
			ImGui::TextWrapped("%s", detail.c_str());
		}

		if (!result.GraphError.Succeeded())
		{
			const auto detail = LOCAL_FMT("GraphPanel.GraphErrorDetail",
				static_cast<int>(result.GraphError.Code), result.GraphError.Node,
				result.GraphError.Pin, LocalizeGraphMessage(result.GraphError.Message));
			ImGui::TextWrapped("%s", detail.c_str());
		}

		if (result.SystemError)
		{
			const auto detail = LOCAL_FMT("GraphPanel.SystemErrorDetail",
				result.SystemError.value(), result.SystemError.message());
			ImGui::TextWrapped("%s", detail.c_str());
		}

		ImGui::EndTooltip();
	}
}

void GraphPanel::PrepareNodeMenu(GraphPinReference source)
{
	m_AddSource = std::move(source);
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
	bool any = false;
	ImGui::BeginChild("##NodeResults", ImVec2(ImGui::GetFontSize() * 28.0f, ImGui::GetTextLineHeightWithSpacing() * 17));
	for (const auto& candidate : m_NodeCandidates)
	{
		const auto* definition = m_Registry.Find(candidate.Type);
		if (!definition) continue;
		const string name = definition->DisplayName.empty() ? candidate.Type : LOCAL(definition->DisplayName);
		string label = definition->Category.empty() ? name : LOCAL(definition->Category) + " / " + name;
		if (!candidate.Pin.empty())
		{
			const auto* pin = m_Registry.FindPin(candidate.Type, candidate.Pin);
			if (!pin) continue;
			label += " / " + (pin->DisplayName.empty() ? pin->Key : LOCAL(pin->DisplayName));
		}
		const string searchText = label + " " + LOCAL(definition->Tooltip);
		if (!MatchesGraphNodeSearch(*definition, candidate, m_NodeSearch, searchText)) continue;
		any = true;
		label += "###Candidate";
		ImGui::PushID(candidate.Type.c_str());
		ImGui::PushID(candidate.Pin.c_str());
		if (ImGui::Selectable(label.c_str(), false, ImGuiSelectableFlags_NoAutoClosePopups) && AddNode(candidate))
			ImGui::CloseCurrentPopup();
		if (ImGui::IsItemHovered())
		{
			ImGui::BeginTooltip();
			ImGui::TextUnformatted(candidate.Type.c_str());
			if (!candidate.Pin.empty()) ImGui::TextUnformatted(candidate.Pin.c_str());
			if (!definition->Tooltip.empty()) ImGui::TextUnformatted(LOCAL_CSTR(definition->Tooltip));
			ImGui::EndTooltip();
		}
		ImGui::PopID();
		ImGui::PopID();
	}
	if (!any) ImGui::TextDisabled("%s", LOCAL_CSTR("GraphPanel.NoMatchingNodes"));
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

	ImGui::PushID(FindPinId(reference));
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
	if (missing && ImGui::SmallButton(LOCAL_CSTR("GraphPanel.UseValue")))
		ApplyResult(graph.SetInputValue(m_Registry, reference, value));
	if (overridden && ImGui::SmallButton(LOCAL_CSTR("GraphPanel.ResetValue")))
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
	ImNodes::SetNodeGridSpacePos(view.Id, m_AddPosition);
	document.Nodes.emplace(result.Node, std::move(view));
	if (result.Link != 0) document.Links.emplace(result.Link, static_cast<int>(document.NextId++));
	document.PendingSelection = result.Node;
	return true;
}

void GraphPanel::DeleteSelection()
{
	auto& document = GetDocument();
	ImNodes::ClearNodeSelection();
	ImNodes::ClearLinkSelection();
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
