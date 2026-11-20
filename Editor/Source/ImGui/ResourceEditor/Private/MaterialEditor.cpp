#include "MaterialEditor.h"
#include "EditorUI.h"
#include "ShortcutManager.h"
#include "Material.h"
#include "MaterialInstance.h"
#include "ResourceManager.h"
#include "AssetPathRelocation.h"
#include "PlatformPath.h"
#include "Functions.h"
#include "ImGuiManager.h"
#include "Logger.h"
#include "LocalizationManager.h"
#include "ResourcePicker.h"
#include "AssetCache.h"
#include "NotificationManager.h"
#include "Graph.h"
#include "GraphFile.h"
#include "GraphRegistry.h"
#include "ReflectionRegistry.h"
#include "IconsFontAwesome7.h"
#include "AssetPathPicker.h"
#include "ReflectionPropertyDrawer.h"
#include <reflection/Metadata.h>
#include <archive/JsonArchive.h>
#include <functional>
#include <imgui_stdlib.h>
#include <algorithm>

namespace
{
template<size_t N>
bool LocalizedCombo(const char* label, int* selected, const char* const (&keys)[N])
{
	std::array<std::optional<Engine::LocalizedText>, N> labels;
	std::array<const char*, N> items{};
	auto& localization = Engine::LocalizationManager::Get();

	for (size_t i = 0; i < N; ++i)
	{
		labels[i].emplace(localization.GetTextForDisplay(keys[i]));
		items[i] = labels[i]->c_str();
	}

	return ImGui::Combo(label, selected, items.data(), static_cast<int>(N));
}

bool IsShaderGraphFile(const std::filesystem::path& path)
{
	return Engine::ToLowerStr(Engine::WStrToStr(path.filename().wstring())).ends_with(".bamgraph.json");
}

// 셰이더 그래프 연결 피커. 그래프 패널·콘텐츠 브라우저의 그래프 표시와 같은 아이콘을 쓴다.
const Editor::AssetPathPickerDesc ShaderGraphPicker{ { ICON_FA_DIAGRAM_PROJECT, ImVec4(0.45f, 0.75f, 1.0f, 1.0f), "GRAPH" }, IsShaderGraphFile };

std::string GetResourceDisplayName(const Engine::Resource* res)
{
	if (!res) return LOCAL("MaterialEditor.NoResource");
	const std::wstring& key = res->GetKey();
	if (key.empty()) return LOCAL("MaterialEditor.EmptyResourceKey");
	return std::filesystem::path(key).filename().string();
}
}

MaterialEditor::MaterialEditor()
{
	m_Name = L"Material Editor";
	m_DisplayNameKey = "ResourceEditor.MaterialTitle";
	m_PreviewRenderer.Initialize(512, 512);
	(void)m_GraphRegistry.RegisterBuiltins();
}

MaterialEditor::~MaterialEditor()
{
	ReleaseResourceDocument();
}

bool MaterialEditor::IsSupported(const std::filesystem::path& assetPath) const
{
	const std::string name = Engine::ToLowerStr(Engine::WStrToStr(assetPath.filename().wstring()));
	return name.ends_with(".bammat.json") || name.ends_with(".bammatinst.json") ||
		name.ends_with(".bammat") || name.ends_with(".bammatinst") ||
		name.ends_with(".mat");
}

bool MaterialEditor::HasUnsavedResourceChanges() const
{
	return m_History.IsDirty() || m_ShaderGraphHistory.IsDirty();
}

bool MaterialEditor::CanSaveDocument() const
{
	return m_Open && m_Material && !m_IsReadOnly;
}

void MaterialEditor::SetTargetResource(const std::filesystem::path& assetPath)
{
	const auto path = Engine::PlatformPath::ToProjectRelative(assetPath).wstring();
	auto& manager = Engine::ResourceManager::Get();
	auto candidate = manager.GetResourceHandle<Engine::MaterialInterface>(path);
	if (!candidate.IsValid())
		candidate = Engine::ResourceHandle<Engine::MaterialInterface>(manager.LoadFile(path));
	if (!candidate.Get() || !RestoreSavedChanges()) return;

	m_TargetResourcePath = assetPath;
	m_IsReadOnly = Engine::PlatformPath::IsReadOnlyTarget(assetPath);
	m_PreviewRenderer.ReleaseMaterial();
	m_MaterialHandle = std::move(candidate);
	m_Material = m_MaterialHandle.Get();

	if (auto initial = CaptureDocument()) m_History.Reset(std::move(*initial));
	else m_History.Clear();

	// 이전 문서의 그래프 상태를 비우고, 연결된 그래프가 있을 때만 읽는다.
	// 여는 것만으로 그래프 파일을 만들거나 머티리얼의 셰이더를 바꾸지 않는다.
	m_ShaderGraph = {};
	m_ShaderGraphHistory.Clear();
	m_GraphReady = false;
	m_LiveCompileCooldown = 0.0f;
	TryLoadLinkedShaderGraph();
}

void MaterialEditor::Update(f32 dt)
{
	if (!m_Material || !m_IsVisibleOnScreen) return;

	// 1. 프리뷰 렌더러 프레임 갱신
	m_PreviewRenderer.Submit(m_Material, dt);

	// 2. 실시간 자동 컴파일 디바운스 틱
	if (m_AutoCompileEnabled && m_LiveCompileCooldown > 0.0f)
	{
		m_LiveCompileCooldown -= dt;
		if (m_LiveCompileCooldown <= 0.0f)
		{
			m_LiveCompileCooldown = 0.0f;
			LiveHotReloadShader();
		}
	}
}

void MaterialEditor::OnResourceWindowVisibility(bool visible)
{
	m_IsVisibleOnScreen = visible;
}

void MaterialEditor::ReleaseResourceDocument()
{
	// 프리뷰 위젯은 패널 수명 동안 유지한다(생성자에서 한 번 초기화). 문서를 닫을 때는 머티리얼 참조만 놓는다.
	m_PreviewRenderer.ReleaseMaterial();
	m_Material = nullptr;
	m_MaterialHandle.Reset();
	m_LivePixelShader.Reset();
	m_History.Clear();
	m_ShaderGraph = {};
	m_ShaderGraphHistory.Clear();
	m_GraphReady = false;
	m_LiveCompileCooldown = 0.0f;
	m_SelectedNode = 0;
	m_SelectedLink = 0;
	m_SelectedNodes.clear();
	m_SelectedLinks.clear();
}

bool MaterialEditor::RestoreSavedResource()
{
	if (!m_Material) return false;
	archive::JsonArchive draft;
	if (!m_Material->WriteArchive(draft)) return false;
	if (m_Material->Reload(m_TargetResourcePath) != EResult::Success)
	{
		if (const auto restored = m_Material->ReadArchive(draft); !restored)
			EDITOR_LOG_ERROR("Could not restore the material draft: {}", restored.Field);
		return false;
	}
	if (auto initial = CaptureDocument()) m_History.Reset(std::move(*initial));
	// 저장된 상태로 되돌렸으므로 그래프도 저장된 연결 기준으로 다시 읽는다.
	m_ShaderGraph = {};
	m_ShaderGraphHistory.Clear();
	m_GraphReady = false;
	m_LiveCompileCooldown = 0.0f;
	TryLoadLinkedShaderGraph();
	return true;
}

// 공통 3줄 헤더의 메뉴 줄(파일·보기·편집). 다른 리소스 편집기와 같은 구성을 유지한다.
void MaterialEditor::DrawCustomToolbar()
{
	if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.File").c_str()))
	{
		DrawSaveMenuItem();
		if (ImGui::MenuItem((ICON_FA_ROTATE_LEFT " " + LOCAL_LABEL("MaterialEditor.Revert")).c_str(), nullptr, false, CanSaveDocument() && HasUnsavedResourceChanges()))
			RunWithDiscardConfirmation([this] { RestoreSavedChanges(); }, false);
		DrawCloseMenuItem();
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu(LOCAL_LABEL("MaterialEditor.ViewMenu").c_str()))
	{
		const auto& shortcuts = ShortcutManager::Get();
		if (ImGui::MenuItem((ICON_FA_CROSSHAIRS " " + LOCAL_LABEL("MaterialEditor.ResetCamera")).c_str(), shortcuts.GetShortcutHint("Viewport.Focus")))
			m_PreviewRenderer.GetWidget().FocusTarget(Engine::vec3(0.0f, 0.0f, 0.0f), 3.0f);
		if (ImGui::MenuItem((ICON_FA_BORDER_ALL " " + LOCAL_LABEL("MaterialEditor.ToggleGrid")).c_str(),
			shortcuts.GetShortcutHint("Viewport.ToggleGrid"), m_PreviewRenderer.GetWidget().IsGridVisible()))
			m_PreviewRenderer.GetWidget().ToggleGrid();

		ImGui::Separator();
		if (ImGui::BeginMenu(LOCAL_LABEL("MaterialEditor.PreviewMesh").c_str()))
		{
			const auto currentType = m_PreviewRenderer.GetPreviewMeshType();
			if (ImGui::MenuItem(LOCAL_LABEL("MaterialEditor.Sphere").c_str(), nullptr, currentType == EPreviewMeshType::Sphere))
				m_PreviewRenderer.SetPreviewMeshType(EPreviewMeshType::Sphere);
			if (ImGui::MenuItem(LOCAL_LABEL("MaterialEditor.Cube").c_str(), nullptr, currentType == EPreviewMeshType::Cube))
				m_PreviewRenderer.SetPreviewMeshType(EPreviewMeshType::Cube);
			if (ImGui::MenuItem(LOCAL_LABEL("MaterialEditor.Plane").c_str(), nullptr, currentType == EPreviewMeshType::Plane))
				m_PreviewRenderer.SetPreviewMeshType(EPreviewMeshType::Plane);
			ImGui::EndMenu();
		}
		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.Edit").c_str()))
	{
		DrawHistoryMenuItems("MaterialEditor");
		ImGui::EndMenu();
	}
}

// 공통 3줄 헤더의 빠른 실행 줄. 셰이더 그래프 액션은 등록된 단축키 액션을 그대로 실행한다.
void MaterialEditor::DrawCustomQuickAccess()
{
	if (!m_Material) return;

	EditorUI::ToolbarLayout toolbar;
	DrawSaveButton(toolbar);
	DrawHistoryButtons(toolbar, "MaterialEditor");
	if (toolbar.Icon("RevertMaterial", ICON_FA_ROTATE_LEFT, "MaterialEditor.Revert", { .Enabled = CanSaveDocument() && HasUnsavedResourceChanges() }))
		RunWithDiscardConfirmation([this] { RestoreSavedChanges(); }, false);

	auto& shortcuts = ShortcutManager::Get();
	const auto drawAction = [&](const char* id, const char* icon, const char* labelKey)
	{
		EditorUI::IconButtonOptions options;
		options.Enabled = shortcuts.CanExecuteAction(id);
		options.ShortcutHint = shortcuts.GetShortcutHint(id);
		if (toolbar.Icon(id, icon, labelKey, options))
			shortcuts.ExecuteAction(id);
	};
	drawAction("MaterialEditor.CompileHotReload", ICON_FA_BOLT, "MaterialEditor.CompileHotReload");
	drawAction("MaterialEditor.ResetView", ICON_FA_DIAGRAM_PROJECT, "MaterialEditor.ResetView");

	if (toolbar.Icon("FocusMaterial", ICON_FA_CROSSHAIRS, "MaterialEditor.ResetCamera"))
		m_PreviewRenderer.GetWidget().FocusTarget(Engine::vec3(0.0f, 0.0f, 0.0f), 3.0f);

	EditorUI::IconButtonOptions gridOptions;
	gridOptions.Active = m_PreviewRenderer.GetWidget().IsGridVisible();
	if (toolbar.Icon("ToggleGridMaterial", ICON_FA_BORDER_ALL, "MaterialEditor.ToggleGrid", gridOptions))
		m_PreviewRenderer.GetWidget().ToggleGrid();

	// 파일 상태 배지
	const std::string fileName = m_TargetResourcePath.empty()
		? LOCAL("AnimatorGraph.Unsaved") : m_TargetResourcePath.filename().string();
	toolbar.Text(("[" + fileName + (HasUnsavedResourceChanges() ? " *]" : "]")).c_str());

	if (m_IsReadOnly)
	{
		const std::string label = ICON_FA_LOCK " " + LOCAL("ContentBrowser.EngineReadOnlyBadge");
		toolbar.Text(label.c_str());
	}
}

void MaterialEditor::DrawCustomContent()
{
	if (!m_Material)
	{
		EditorUI::EmptyState("MaterialEditor.NoResource");
		return;
	}

	const ImVec2 avail = ImGui::GetContentRegionAvail();
	const float availHeight = avail.y;
	const float spacing = ImGui::GetStyle().ItemSpacing.x;
	constexpr float splitterThickness = Editor::SplitterThickness;
	constexpr float minSideWidth = 200.0f;
	constexpr float minCenterWidth = 260.0f;

	// 세 영역 + 스플리터 두 개를 SameLine으로 잇으면 항목 간격이 네 번 들어간다. 그만큼 빼야 우측 패널이 창 밖으로 밀리지 않는다.
	const float paneWidth = (std::max)(1.0f, avail.x - splitterThickness * 2.0f - spacing * 4.0f);
	float leftWidth = m_LeftPaneWidth;
	float rightWidth = m_RightPaneWidth;
	if (paneWidth >= minSideWidth * 2.0f + minCenterWidth)
	{
		const float maxSideSum = paneWidth - minCenterWidth;
		if (leftWidth + rightWidth > maxSideSum)
		{
			const float scale = maxSideSum / (leftWidth + rightWidth);
			leftWidth *= scale;
			rightWidth *= scale;
		}
		leftWidth = (std::max)(leftWidth, minSideWidth);
		rightWidth = (std::max)(rightWidth, minSideWidth);
	}
	else
	{
		// 창이 좁으면 저장한 선호 폭은 유지하고 이번 프레임만 비율로 나눈다.
		leftWidth = paneWidth * 0.3f;
		rightWidth = paneWidth * 0.3f;
	}
	float centerWidth = (std::max)(1.0f, paneWidth - leftWidth - rightWidth);

	// 1. [좌측 패널]: 3D 프리뷰 + 머티리얼 설정
	DrawLeftPane(leftWidth, availHeight);

	// 수직 스플리터 1: 좌측 ↔ 중앙 (드래그했을 때만 선호 폭을 갱신)
	ImGui::SameLine();
	if (Editor::SplitterV("##SplitterLeftCenter", &leftWidth, &centerWidth, minSideWidth, minCenterWidth, availHeight, splitterThickness))
		m_LeftPaneWidth = leftWidth;
	ImGui::SameLine();

	// 2. [중앙 패널]: 셰이더 그래프 캔버스. 그래프가 없는 머티리얼은 안내와 만들기 버튼을 보여 준다.
	if (m_GraphReady)
		DrawCenterCanvasPane(centerWidth, availHeight);
	else
		DrawNoShaderGraphState(centerWidth, availHeight);

	// 수직 스플리터 2: 중앙 ↔ 우측
	ImGui::SameLine();
	if (Editor::SplitterV("##SplitterCenterRight", &centerWidth, &rightWidth, minCenterWidth, minSideWidth, availHeight, splitterThickness))
		m_RightPaneWidth = rightWidth;
	ImGui::SameLine();

	// 3. [우측 패널]: 선택한 노드 속성 + 셰이더 빌드 정보
	DrawRightInspectorPane(rightWidth, availHeight);

	if (!IsHistoryGestureActive()) CommitDocumentEdit();
}

void MaterialEditor::DrawLeftPane(float width, float height)
{
	// 1. 바깥 컨테이너 패딩 0 및 마우스 휠 스크롤/스크롤바 완전 차단 (가로 1칸 밀림 원천 방지)
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	if (ImGui::BeginChild("##MaterialLeftPane", ImVec2(width, height), false, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		const float minPreviewH = 120.0f;
		const float minSettingsH = 150.0f;
		constexpr float splitterH = Editor::SplitterThickness;

		const float usableHeight = (std::max)(1.0f, height - splitterH - ImGui::GetStyle().ItemSpacing.y * 2.0f);
		m_PreviewHeight = std::clamp(m_PreviewHeight, minPreviewH, (std::max)(minPreviewH, usableHeight - minSettingsH));
		const float settingsHeight = (std::max)(1.0f, usableHeight - m_PreviewHeight);

		// ── 1-A. 상단: 3D 머티리얼 프리뷰 위젯 ──
		if (ImGui::BeginChild("##MaterialPreviewContainer", ImVec2(0.0f, m_PreviewHeight), true))
		{
			m_PreviewRenderer.DrawUI(m_Material);
		}
		ImGui::EndChild();

		// ── 수평 스플리터: 프리뷰 ↕ 파이프라인/셰이더 설정 ──
		float curSettingsH = settingsHeight;
		Editor::SplitterH("##SplitterPreviewSettings", &m_PreviewHeight, &curSettingsH, minPreviewH, minSettingsH, 0.0f, splitterH);

		// ── 1-B. 하단: 파이프라인 / 셰이더 / 바인딩 / 파라미터 상세 설정 ──
		if (ImGui::BeginChild("##MaterialDetailsScroll", ImVec2(0.0f, curSettingsH), true))
		{
			ImGui::SetScrollX(0.0f); // 수평 1칸 밀림 방지: 가로 스크롤 위치 강제 0 고정
			DrawPipelineSettings();
			DrawShaderSettings();
			DrawTextureBindings();
			DrawParameters();
		}
		ImGui::EndChild();
	}
	ImGui::EndChild();
	ImGui::PopStyleVar();
}

void MaterialEditor::DrawPipelineSettings()
{
	if (!ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.PipelineSettings"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	const bool readOnly = m_IsReadOnly || !m_Material;
	ImGui::BeginDisabled(readOnly);

	// ── 라벨과 값의 비율 조정: 가용 폭의 42%를 라벨로 할당 (110px ~ 145px 범위 제한) ──
	auto tableStyle = EditorUI::InspectorPropertyTableStyle();
	const float availWidth = ImGui::GetContentRegionAvail().x;
	tableStyle.LabelWidth = std::clamp(availWidth * 0.42f, 110.0f, 145.0f);

	if (EditorUI::BeginPropertyTable("##PipelineSettingsTable", tableStyle))
	{
		// 1. 블렌드 모드 (EBlendMode: 비트마스크 열거형 매핑)
		constexpr Engine::EBlendMode blendValues[] = {
			Engine::EBlendMode::Opaque,
			Engine::EBlendMode::Masked,
			Engine::EBlendMode::AlphaBlend,
			Engine::EBlendMode::Additive,
			Engine::EBlendMode::NonPremultiplied
		};
		constexpr const char* const blendKeys[] = {
			"MaterialOption.Opaque",
			"MaterialOption.Masked",
			"MaterialOption.Translucent",
			"MaterialOption.Additive",
			"MaterialOption.NonPremultiplied"
		};
		int blendIdx = -1;
		for (int i = 0; i < static_cast<int>(IM_ARRAYSIZE(blendValues)); ++i)
		{
			if (blendValues[i] == m_Material->GetBlendMode())
			{
				blendIdx = i;
				break;
			}
		}
		EditorUI::PropertyRow("MaterialEditor.BlendMode");
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (LocalizedCombo("###BlendMode", &blendIdx, blendKeys) && blendIdx >= 0)
		{
			m_Material->SetBlendMode(blendValues[blendIdx]);
			m_History.MarkEdited();
		}

		// 2. 컬링 모드 (ECullMode)
		constexpr Engine::ECullMode cullValues[] = {
			Engine::ECullMode::None,
			Engine::ECullMode::Front,
			Engine::ECullMode::Back
		};
		constexpr const char* const cullKeys[] = {
			"MaterialOption.None",
			"MaterialOption.Front",
			"MaterialOption.Back"
		};
		int cullIdx = -1;
		for (int i = 0; i < static_cast<int>(IM_ARRAYSIZE(cullValues)); ++i)
		{
			if (cullValues[i] == m_Material->GetCullMode())
			{
				cullIdx = i;
				break;
			}
		}
		EditorUI::PropertyRow("MaterialEditor.CullMode");
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (LocalizedCombo("###CullMode", &cullIdx, cullKeys) && cullIdx >= 0)
		{
			m_Material->SetCullMode(cullValues[cullIdx]);
			m_History.MarkEdited();
		}

		// 3. 채우기 모드 (EFillMode: Solid=1, Wireframe=0)
		constexpr Engine::EFillMode fillValues[] = {
			Engine::EFillMode::Solid,
			Engine::EFillMode::Wireframe
		};
		constexpr const char* const fillKeys[] = {
			"MaterialOption.Solid",
			"MaterialOption.Wireframe"
		};
		int fillIdx = -1;
		for (int i = 0; i < static_cast<int>(IM_ARRAYSIZE(fillValues)); ++i)
		{
			if (fillValues[i] == m_Material->GetFillMode())
			{
				fillIdx = i;
				break;
			}
		}
		EditorUI::PropertyRow("MaterialEditor.FillMode");
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (LocalizedCombo("###FillMode", &fillIdx, fillKeys) && fillIdx >= 0)
		{
			m_Material->SetFillMode(fillValues[fillIdx]);
			m_History.MarkEdited();
		}

		// 4. 깊이 모드 (EDepthMode)
		constexpr Engine::EDepthMode depthValues[] = {
			Engine::EDepthMode::None,
			Engine::EDepthMode::ReadOnly,
			Engine::EDepthMode::ReadWrite
		};
		constexpr const char* const depthKeys[] = {
			"MaterialOption.None",
			"MaterialOption.ReadOnly",
			"MaterialOption.ReadWrite"
		};
		int depthIdx = -1;
		for (int i = 0; i < static_cast<int>(IM_ARRAYSIZE(depthValues)); ++i)
		{
			if (depthValues[i] == m_Material->GetDepthMode())
			{
				depthIdx = i;
				break;
			}
		}
		EditorUI::PropertyRow("MaterialEditor.DepthMode");
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (LocalizedCombo("###DepthMode", &depthIdx, depthKeys) && depthIdx >= 0)
		{
			m_Material->SetDepthMode(depthValues[depthIdx]);
			m_History.MarkEdited();
		}

		// 5. 깊이 비교 연산 (ECompareOp)
		constexpr Engine::ECompareOp compValues[] = {
			Engine::ECompareOp::Never,
			Engine::ECompareOp::Less,
			Engine::ECompareOp::Equal,
			Engine::ECompareOp::LessOrEqual,
			Engine::ECompareOp::Greater,
			Engine::ECompareOp::NotEqual,
			Engine::ECompareOp::GreaterOrEqual,
			Engine::ECompareOp::Always
		};
		constexpr const char* const compKeys[] = {
			"MaterialOption.Never",
			"MaterialOption.Less",
			"MaterialOption.Equal",
			"MaterialOption.LessEqual",
			"MaterialOption.Greater",
			"MaterialOption.NotEqual",
			"MaterialOption.GreaterEqual",
			"MaterialOption.Always"
		};
		int compIdx = -1;
		for (int i = 0; i < static_cast<int>(IM_ARRAYSIZE(compValues)); ++i)
		{
			if (compValues[i] == m_Material->GetDepthCompareOp())
			{
				compIdx = i;
				break;
			}
		}
		EditorUI::PropertyRow("MaterialEditor.DepthCompareOp");
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (LocalizedCombo("###DepthCompareOp", &compIdx, compKeys) && compIdx >= 0)
		{
			m_Material->SetDepthCompareOp(compValues[compIdx]);
			m_History.MarkEdited();
		}

		EditorUI::EndPropertyTable();
	}

	ImGui::EndDisabled();
}

void MaterialEditor::DrawShaderSettings()
{
	if (!ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.ShaderSettings"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	const bool readOnly = m_IsReadOnly || !m_Material;
	ImGui::BeginDisabled(readOnly);

	// ── 셰이더 그래프 연결: 인스펙터와 같은 경로 피커로 지정·해제하고, 없으면 새로 만든다 ──
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(LOCAL_CSTR("MaterialEditor.ShaderGraphPath"));
	DrawShaderGraphLinkControls();

	const float availWidth = ImGui::GetContentRegionAvail().x;

	// ── 기본 셰이더 (2열 프로퍼티 테이블: 좌측 라벨, 우측 리소스 피커) ──
	ImGui::Separator();
	ImGui::TextDisabled("%s", LOCAL_CSTR("MaterialEditor.DefaultShaders"));

	auto defaultPropsStyle = EditorUI::InspectorPropertyTableStyle();
	defaultPropsStyle.LabelWidth = std::clamp(availWidth * 0.40f, 100.0f, 135.0f);

	if (EditorUI::BeginPropertyTable("##DefaultShadersProps", defaultPropsStyle))
	{
		EditorUI::PropertyRow("MaterialEditor.DefaultVS");
		auto vsHandle = m_Material->GetVertexShaderHandle();
		if (Editor::DrawInspectorResourcePicker(vsHandle, !readOnly) && !readOnly)
		{
			m_Material->SetVertexShaderHandle(vsHandle);
			m_Material->IncreaseVersion();
			m_History.MarkEdited();
			m_PreviewDirtyTimer = 0.2f;
		}

		EditorUI::PropertyRow("MaterialEditor.DefaultPS");
		auto psHandle = m_Material->GetPixelShaderHandle();
		if (Editor::DrawInspectorResourcePicker(psHandle, !readOnly) && !readOnly)
		{
			m_Material->SetPixelShaderHandle(psHandle);
			m_Material->IncreaseVersion();
			m_History.MarkEdited();
			m_PreviewDirtyTimer = 0.2f;
		}

		EditorUI::EndPropertyTable();
	}

	// ── 패스별 셰이더 목록 & 컨트롤 ──
	ImGui::Separator();
	ImGui::TextDisabled("%s", LOCAL_CSTR("MaterialEditor.PassSpecificShaders"));

	// 상단 컨트롤: 패스 종류 선택 및 [+ 추가] 버튼
	constexpr const char* const passNames[] = { "Custom", "Geometry", "Lighting", "Shadow", "Forward", "ForwardTransparent", "PostProcess", "UI" };
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 8.0f);
	ImGui::Combo("##NewPassTypeCombo", &m_NewPassTypeIndex, passNames, IM_ARRAYSIZE(passNames));
	ImGui::SameLine();
	if (ImGui::Button(("+ " + LOCAL("MaterialEditor.AddPassShader")).c_str()))
	{
		auto passType = static_cast<Engine::ERenderPassType>(m_NewPassTypeIndex);
		if (!m_Material->HasPassShader(passType))
		{
			m_Material->SetPassShader(passType, m_Material->GetVertexShaderHandle(), m_Material->GetPixelShaderHandle());
			m_Material->IncreaseVersion();
			m_History.MarkEdited();
			m_PreviewDirtyTimer = 0.2f;
		}
	}

	// 패스 셰이더 목록: 항목별 카드 테이블로 묶어서 표시
	// Row 0: | [PassName] (좌측)               | (공백)            [휴지통] |
	// Row 1: | 정점 셰이더                     | 리소스 피커                |
	// Row 2: | 픽셀 셰이더                     | 리소스 피커                |
	const auto& passes = m_Material->GetPassShaders();
	int passToRemove = -1;

	if (!passes.empty())
	{
		auto passPropsStyle = EditorUI::InspectorPropertyTableStyle();
		passPropsStyle.LabelWidth = std::clamp(availWidth * 0.40f, 100.0f, 135.0f);

		for (size_t i = 0; i < passes.size(); ++i)
		{
			const auto& entry = passes[i];
			ImGui::PushID(static_cast<int>(entry.passType));

			int pIdx = static_cast<int>(entry.passType);
			const char* pName = (pIdx >= 0 && pIdx < IM_ARRAYSIZE(passNames)) ? passNames[pIdx] : "Unknown";

			ImGui::Spacing();

			// ── 패스 카드 헤더: 좌측 [PassName] + 우측 끝 [휴지통] ──
			ImGui::AlignTextToFramePadding();
			ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "[%s]", pName);

			const float btnSize = 24.0f;
			ImGui::SameLine();
			const float remaining = ImGui::GetContentRegionAvail().x;
			if (remaining > btnSize)
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + remaining - btnSize);

			EditorUI::IconButtonOptions delOpt;
			delOpt.Compact = true;
			delOpt.NoWrap = true;
			if (EditorUI::IconButton("DelPass", ICON_FA_TRASH, "MaterialEditor.DeletePassShader", delOpt))
			{
				passToRemove = static_cast<int>(entry.passType);
			}

			// ── 패스 셰이더 프로퍼티 테이블 ──
			if (EditorUI::BeginPropertyTable("##PassShaderProps", passPropsStyle))
			{

				// 정점 셰이더 행
				EditorUI::PropertyRow("MaterialEditor.VertexShader");
				auto vs = entry.vertexShader;
				if (Editor::DrawInspectorResourcePicker(vs, !readOnly) && !readOnly)
				{
					m_Material->SetPassShader(entry.passType, vs, entry.pixelShader);
					m_Material->IncreaseVersion();
					m_History.MarkEdited();
					m_PreviewDirtyTimer = 0.2f;
				}

				// 픽셀 셰이더 행
				EditorUI::PropertyRow("MaterialEditor.PixelShader");
				auto ps = entry.pixelShader;
				if (Editor::DrawInspectorResourcePicker(ps, !readOnly) && !readOnly)
				{
					m_Material->SetPassShader(entry.passType, entry.vertexShader, ps);
					m_Material->IncreaseVersion();
					m_History.MarkEdited();
					m_PreviewDirtyTimer = 0.2f;
				}

				EditorUI::EndPropertyTable();
			}

			ImGui::Spacing();
			ImGui::PopID();
		}

		if (passToRemove >= 0)
		{
			m_Material->RemovePassShader(static_cast<Engine::ERenderPassType>(passToRemove));
			m_Material->IncreaseVersion();
			m_History.MarkEdited();
			m_PreviewDirtyTimer = 0.2f;
		}
	}

	ImGui::EndDisabled();
}

void MaterialEditor::DrawTextureBindings()
{
	if (!ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.TextureBindings"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	const bool readOnly = m_IsReadOnly || !m_Material;
	ImGui::BeginDisabled(readOnly);

	// 상단 컨트롤: 슬롯 번호 + 바인딩 이름 + [+ 바인딩 추가]
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.5f);
	ImGui::InputInt("##NewSlotNum", &m_NewTextureSlotNumber, 0, 0);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7.0f);
	ImGui::InputTextWithHint("##NewSlotName", LOCAL_CSTR("MaterialEditor.SlotNameHint"), m_NewTextureSlotName, sizeof(m_NewTextureSlotName));
	ImGui::SameLine();
	if (ImGui::Button(("+ " + LOCAL("MaterialEditor.AddBinding")).c_str()))
	{
		if (m_NewTextureSlotName[0] != '\0')
		{
			m_Material->SetTextureBinding(m_NewTextureSlotName, static_cast<uint32>(std::max(0, m_NewTextureSlotNumber)), {});
			m_NewTextureSlotName[0] = '\0';
			m_NewTextureSlotNumber++;
			m_Material->IncreaseVersion();
			m_History.MarkEdited();
			m_PreviewDirtyTimer = 0.2f;
		}
	}

	const auto& bindings = m_Material->GetTextureBindings();
	int slotToRemove = -1;

	if (!bindings.empty())
	{
		// 미리보기 칸 제거, 텍스처 리소스피커 버튼(썸네일 포함) 및 삭제 버튼을 갖춘 4열 테이블
		if (ImGui::BeginTable("##TextureBindingsTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.Slot"), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 2.8f);
			ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.Name"), ImGuiTableColumnFlags_WidthStretch, 0.32f);
			ImGui::TableSetupColumn(LOCAL_CSTR("ResourceType.Texture"), ImGuiTableColumnFlags_WidthStretch, 0.68f);
			ImGui::TableSetupColumn("##DelBinding", ImGuiTableColumnFlags_WidthFixed, 28.0f);
			ImGui::TableHeadersRow();

			for (const auto& binding : bindings)
			{
				ImGui::PushID(static_cast<int>(binding.slot));
				ImGui::TableNextRow();

				const float rowH = ImGui::GetTextLineHeight() * 2.0f + ImGui::GetStyle().ItemInnerSpacing.y + ImGui::GetStyle().FramePadding.y * 2.0f;
				const float textOffsetY = (rowH - ImGui::GetTextLineHeight()) * 0.5f;

				// 0. 슬롯 번호 (썸네일 피커 높이에 맞춰 수직 중앙 정렬)
				ImGui::TableSetColumnIndex(0);
				if (textOffsetY > 0.0f) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textOffsetY);
				ImGui::Text("t%u", binding.slot);

				// 1. 바인딩 이름 (수직 중앙 정렬)
				ImGui::TableSetColumnIndex(1);
				if (textOffsetY > 0.0f) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + textOffsetY);
				ImGui::TextUnformatted(binding.name.c_str());

				// 2. 텍스처 리소스 피커 (클릭 시 에셋 선택 팝업 오픈, 썸네일 보이는 버전)
				ImGui::TableSetColumnIndex(2);
				ImGui::SetNextItemWidth(-FLT_MIN);
				auto texHandle = binding.texture;
				if (Editor::DrawInspectorResourcePicker(texHandle, !readOnly, true) && !readOnly)
				{
					m_Material->SetTextureBinding(binding.name, binding.slot, texHandle);
					m_Material->IncreaseVersion();
					m_History.MarkEdited();
					m_PreviewDirtyTimer = 0.2f;
				}

				// 3. 삭제 버튼 (휴지통 아이콘, NoWrap = true로 상시 노출 및 수직 중앙 정렬)
				ImGui::TableSetColumnIndex(3);
				const float btnOffsetY = (rowH - ImGui::GetFrameHeight()) * 0.5f;
				if (btnOffsetY > 0.0f) ImGui::SetCursorPosY(ImGui::GetCursorPosY() + btnOffsetY);
				EditorUI::IconButtonOptions delOpt;
				delOpt.Compact = true;
				delOpt.NoWrap = true;
				if (EditorUI::IconButton("DelBinding", ICON_FA_TRASH, "MaterialEditor.DeleteBinding", delOpt))
				{
					slotToRemove = static_cast<int>(binding.slot);
				}

				ImGui::PopID();
			}
			ImGui::EndTable();

			if (slotToRemove >= 0)
			{
				m_Material->RemoveTextureBindingBySlot(static_cast<uint32>(slotToRemove));
				m_Material->IncreaseVersion();
				m_History.MarkEdited();
				m_PreviewDirtyTimer = 0.2f;
			}
		}
	}

	ImGui::EndDisabled();
}

void MaterialEditor::DrawParameters()
{
	if (!ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.Parameters"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	const bool readOnly = m_IsReadOnly || !m_Material;
	ImGui::BeginDisabled(readOnly);

	// 상단 컨트롤 행 1: 이름 + 타입 + [+ 추가]
	constexpr const char* const typeNames[] = { "Float", "Float2", "Float3", "Float4", "Int", "Bool" };
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7.0f);
	ImGui::InputTextWithHint("##NewParamName", LOCAL_CSTR("MaterialEditor.ParamNameHint"), m_NewParamName, sizeof(m_NewParamName));
	ImGui::SameLine();
	ImGui::SetNextItemWidth(ImGui::GetFontSize() * 5.5f);
	ImGui::Combo("##NewParamType", &m_NewParamTypeIndex, typeNames, IM_ARRAYSIZE(typeNames));
	ImGui::SameLine();
	if (ImGui::Button(("+ " + LOCAL("MaterialEditor.AddParam")).c_str()))
	{
		if (m_NewParamName[0] != '\0')
		{
			switch (m_NewParamTypeIndex)
			{
			case 0: m_Material->SetFloat(m_NewParamName, 0.0f); break;
			case 1: m_Material->SetFloat2(m_NewParamName, Engine::vec2(0.0f)); break;
			case 2: m_Material->SetFloat3(m_NewParamName, Engine::vec3(0.0f)); break;
			case 3: m_Material->SetFloat4(m_NewParamName, Engine::vec4(0.0f)); break;
			case 4: m_Material->SetInt(m_NewParamName, 0); break;
			case 5: m_Material->SetBool(m_NewParamName, false); break;
			default: break;
			}
			m_NewParamName[0] = '\0';
			m_Material->IncreaseVersion();
			m_History.MarkEdited();
			m_PreviewDirtyTimer = 0.2f;
		}
	}

	// 상단 컨트롤 행 2: 표준 검색 바
	EditorUI::DrawSearchBar("MaterialParamSearch", m_ParameterSearch, sizeof(m_ParameterSearch), "MaterialEditor.SearchParameters");
	ImGui::Spacing();

	const auto& params = m_Material->GetParameters();
	const std::string filterLower = Engine::ToLowerStr(m_ParameterSearch);

	if (!params.empty())
	{
		if (ImGui::BeginTable("##ParamsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.Name"), ImGuiTableColumnFlags_WidthStretch, 0.40f);
			ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.Value"), ImGuiTableColumnFlags_WidthStretch, 0.60f);
			ImGui::TableSetupColumn("##DelParam", ImGuiTableColumnFlags_WidthFixed, 28.0f);
			ImGui::TableHeadersRow();

			std::string paramToRemove;

			for (const auto& [name, param] : params)
			{
				if (!filterLower.empty() && Engine::ToLowerStr(name).find(filterLower) == std::string::npos)
					continue;

				ImGui::PushID(name.c_str());
				ImGui::TableNextRow();

				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(name.c_str());

				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-FLT_MIN);

				const std::string lower = Engine::ToLowerStr(name);
				const bool isColorParam = param.isColor || lower.find("color") != std::string::npos || lower.find("tint") != std::string::npos;

				switch (param.type)
				{
				case Engine::EMaterialParameterType::Float:
				{
					float val = param.data.size() >= sizeof(float) ? *reinterpret_cast<const float*>(param.data.data()) : 0.0f;
					// PBR 주요 파라미터(roughness, metallic, ao 등)는 0~1 슬라이더로 인스펙터 스타일 렌더링
					bool changed = false;
					if (lower == "roughness" || lower == "metallic" || lower == "ao" || lower == "cavity" || (param.minValue < param.maxValue))
					{
						const float minV = (param.minValue < param.maxValue) ? param.minValue : 0.0f;
						const float maxV = (param.minValue < param.maxValue) ? param.maxValue : 1.0f;
						changed = ImGui::SliderFloat("##val", &val, minV, maxV, "%.2f");
					}
					else
					{
						changed = ImGui::DragFloat("##val", &val, 0.01f);
					}

					if (changed)
					{
						m_Material->SetFloat(name, val);
						SyncGraphFromParameter(name, &val, Engine::EMaterialParameterType::Float);
						m_Material->IncreaseVersion();
						m_History.MarkEdited();
						m_PreviewDirtyTimer = 0.2f;
					}
					break;
				}
				case Engine::EMaterialParameterType::Float3:
				{
					Engine::vec3 val = param.data.size() >= sizeof(Engine::vec3) ? *reinterpret_cast<const Engine::vec3*>(param.data.data()) : Engine::vec3(0.0f);
					if (isColorParam)
					{
						if (ImGui::ColorEdit3("##val", &val.x, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_Float))
						{
							m_Material->SetFloat3(name, val);
							SyncGraphFromParameter(name, &val, Engine::EMaterialParameterType::Float3);
							m_Material->IncreaseVersion();
							m_History.MarkEdited();
							m_PreviewDirtyTimer = 0.2f;
						}
					}
					else
					{
						if (ImGui::DragFloat3("##val", &val.x, 0.01f))
						{
							m_Material->SetFloat3(name, val);
							SyncGraphFromParameter(name, &val, Engine::EMaterialParameterType::Float3);
							m_Material->IncreaseVersion();
							m_History.MarkEdited();
							m_PreviewDirtyTimer = 0.2f;
						}
					}
					break;
				}
				case Engine::EMaterialParameterType::Float4:
				{
					Engine::vec4 val = param.data.size() >= sizeof(Engine::vec4) ? *reinterpret_cast<const Engine::vec4*>(param.data.data()) : Engine::vec4(0.0f);
					if (isColorParam)
					{
						if (ImGui::ColorEdit4("##val", &val.x, ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_Float))
						{
							m_Material->SetFloat4(name, val);
							SyncGraphFromParameter(name, &val, Engine::EMaterialParameterType::Float4);
							m_Material->IncreaseVersion();
							m_History.MarkEdited();
							m_PreviewDirtyTimer = 0.2f;
						}
					}
					else
					{
						if (ImGui::DragFloat4("##val", &val.x, 0.01f))
						{
							m_Material->SetFloat4(name, val);
							SyncGraphFromParameter(name, &val, Engine::EMaterialParameterType::Float4);
							m_Material->IncreaseVersion();
							m_History.MarkEdited();
							m_PreviewDirtyTimer = 0.2f;
						}
					}
					break;
				}
				case Engine::EMaterialParameterType::Float2:
				{
					Engine::vec2 val = param.data.size() >= sizeof(Engine::vec2) ? *reinterpret_cast<const Engine::vec2*>(param.data.data()) : Engine::vec2(0.0f);
					if (ImGui::DragFloat2("##val", &val.x, 0.01f))
					{
						m_Material->SetFloat2(name, val);
						m_Material->IncreaseVersion();
						m_History.MarkEdited();
						m_PreviewDirtyTimer = 0.2f;
					}
					break;
				}
				case Engine::EMaterialParameterType::Int:
				{
					int val = param.data.size() >= sizeof(int) ? *reinterpret_cast<const int*>(param.data.data()) : 0;
					if (ImGui::DragInt("##val", &val))
					{
						m_Material->SetInt(name, val);
						m_Material->IncreaseVersion();
						m_History.MarkEdited();
						m_PreviewDirtyTimer = 0.2f;
					}
					break;
				}
				case Engine::EMaterialParameterType::Bool:
				{
					bool val = param.data.size() >= sizeof(bool) ? *reinterpret_cast<const bool*>(param.data.data()) : false;
					if (ImGui::Checkbox("##val", &val))
					{
						m_Material->SetBool(name, val);
						m_Material->IncreaseVersion();
						m_History.MarkEdited();
						m_PreviewDirtyTimer = 0.2f;
					}
					break;
				}
				default:
					ImGui::TextDisabled("%s", LOCAL_CSTR("MaterialEditor.UnsupportedType"));
					break;
				}

				ImGui::TableSetColumnIndex(2);
				EditorUI::IconButtonOptions delOpt;
				delOpt.Compact = true;
				if (EditorUI::IconButton("DelParam", ICON_FA_TRASH, "MaterialEditor.DeleteParam", delOpt))
				{
					paramToRemove = name;
				}

				ImGui::PopID();
			}

			if (!paramToRemove.empty())
			{
				m_Material->RemoveParameter(paramToRemove);
				m_Material->IncreaseVersion();
				m_History.MarkEdited();
				m_PreviewDirtyTimer = 0.2f;
			}

			ImGui::EndTable();
		}
	}

	ImGui::EndDisabled();
}

void MaterialEditor::DrawRightInspectorPane(float width, float height)
{
	if (ImGui::BeginChild("##MaterialRightInspectorArea", ImVec2(width, height), false, ImGuiWindowFlags_NoScrollbar))
	{
		constexpr float minNodePropsH = 100.0f;
		constexpr float minDiagnosticsH = 80.0f;
		constexpr float minHlslH = 100.0f;
		constexpr float splitterH = Editor::SplitterThickness;
		const float spacingY = ImGui::GetStyle().ItemSpacing.y;

		// 상단, 스플리터1, 중단, 스플리터2, 하단 총 5개 요소 간 세로 간격(spacing * 4) 및 스플리터 두께 제외
		const float usableHeight = (std::max)(1.0f, height - splitterH * 2.0f - spacingY * 4.0f);
		if (usableHeight < minNodePropsH + minDiagnosticsH + minHlslH)
		{
			m_RightNodePropsHeight = usableHeight * 0.4f;
			m_RightDiagnosticsHeight = usableHeight * 0.25f;
		}
		else
		{
			m_RightNodePropsHeight = std::clamp(m_RightNodePropsHeight, minNodePropsH, usableHeight - (minDiagnosticsH + minHlslH));
			m_RightDiagnosticsHeight = std::clamp(m_RightDiagnosticsHeight, minDiagnosticsH, usableHeight - m_RightNodePropsHeight - minHlslH);
		}
		float hlslHeight = (std::max)(1.0f, usableHeight - m_RightNodePropsHeight - m_RightDiagnosticsHeight);

		// ── 1. [상단]: 노드 속성 (Node Properties) ──
		if (ImGui::BeginChild("##MaterialRightNodePropsArea", ImVec2(width, m_RightNodePropsHeight), true))
		{
			if (ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.NodeProperties"), ImGuiTreeNodeFlags_DefaultOpen))
				DrawSelectedNodeProperties();
		}
		ImGui::EndChild();

		// 수평 스플리터 1: 상단(노드 속성) ↔ 중단(셰이더 로그)
		float topH = m_RightNodePropsHeight;
		float midH = m_RightDiagnosticsHeight;
		if (Editor::SplitterH("##SplitterNodePropsDiag", &topH, &midH, minNodePropsH, minDiagnosticsH, width, splitterH))
		{
			m_RightNodePropsHeight = topH;
			m_RightDiagnosticsHeight = midH;
		}

		// ── 2. [중단]: 셰이더 진단 및 로그 (Shader Diagnostics & Log) ──
		if (ImGui::BeginChild("##MaterialRightDiagnosticsArea", ImVec2(width, m_RightDiagnosticsHeight), true))
		{
			DrawShaderDiagnostics();
		}
		ImGui::EndChild();

		// 수평 스플리터 2: 중단(셰이더 로그) ↔ 하단(생성된 HLSL 파일)
		midH = m_RightDiagnosticsHeight;
		float botH = hlslHeight;
		if (Editor::SplitterH("##SplitterDiagHlsl", &midH, &botH, minDiagnosticsH, minHlslH, width, splitterH))
		{
			m_RightDiagnosticsHeight = midH;
		}

		// ── 3. [하단]: 생성된 HLSL 소스 파일 (Generated HLSL Source) ──
		hlslHeight = (std::max)(1.0f, usableHeight - m_RightNodePropsHeight - m_RightDiagnosticsHeight);
		if (ImGui::BeginChild("##MaterialRightHlslArea", ImVec2(width, hlslHeight), true))
		{
			DrawGeneratedHlsl();
		}
		ImGui::EndChild();
	}
	ImGui::EndChild();
}

void MaterialEditor::DrawSelectedNodeProperties()
{
	// 이 영역은 우측 패널의 한 섹션이다. 패널 전체 기준 빈 상태(EmptyState)는 커서를 세로 중앙으로 옮겨 아래 섹션을 밀어내므로 쓰지 않는다.
	const auto* node = (m_GraphReady && m_SelectedNode != 0) ? m_ShaderGraph.Data.FindNode(m_SelectedNode) : nullptr;
	if (!node)
	{
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextDisabled("%s", LOCAL_CSTR("MaterialEditor.NoNodeSelected"));
		ImGui::PopTextWrapPos();
		return;
	}

	const auto* schema = m_GraphRegistry.Find(node->Type);
	const std::string title = schema && !schema->DisplayName.empty() ? schema->DisplayName : node->Type;
	ImGui::TextUnformatted(LOCAL_FMT("MaterialEditor.NodeTitle", node->Id, title).c_str());

	// 코멘트/설명 편집
	std::string comment = node->Comment;
	ImGui::SetNextItemWidth(-FLT_MIN);
	if (ImGui::InputTextWithHint("##NodeComment", LOCAL_CSTR("EditorCommon.Comment"), &comment))
	{
		(void)m_ShaderGraph.Data.SetNodeComment(node->Id, comment);
		CommitShaderGraphEdit();
	}

	if (!schema) return;

	ImGui::SeparatorText(LOCAL_CSTR("MaterialEditor.InputPins"));
	// 인스펙터·리소스 편집기와 같은 공용 속성 표(라벨 폭·여백·잘린 라벨 툴팁)를 쓴다.
	auto tableStyle = EditorUI::InspectorPropertyTableStyle();
	tableStyle.LabelWidth = (std::min)(tableStyle.LabelWidth, ImGui::GetContentRegionAvail().x * 0.4f);
	if (EditorUI::BeginPropertyTable("##NodeInputPins", tableStyle))
	{
		const auto& links = m_ShaderGraph.Data.GetLinks();
		for (const auto& pin : schema->Pins)
		{
			if (pin.Direction != Engine::EGraphPinDirection::Input) continue;

			ImGui::PushID(pin.Key.c_str());
			EditorUI::PropertyLabel(pin.DisplayName.empty() ? pin.Key.c_str() : pin.DisplayName.c_str(), PinTypeName(pin.Type));
			EditorUI::PropertyValue();
			const bool isConnected = std::any_of(links.begin(), links.end(),
				[&](const auto& link) { return link.To.Node == node->Id && link.To.Pin == pin.Key; });
			if (isConnected)
			{
				ImGui::AlignTextToFramePadding();
				ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", LOCAL_CSTR("MaterialEditor.PinConnected"));
			}
			else
			{
				DrawPinInspectorValue(*node, pin);
			}
			ImGui::PopID();
		}
		EditorUI::EndPropertyTable();
	}
}

void MaterialEditor::DrawPinInspectorValue(const Engine::GraphNode& node, const Engine::GraphPin& pin)
{
	// 우측 속성 표용 값 위젯: 값 열 폭을 채우고, 벡터는 인스펙터와 같은 축별 입력(좁으면 세로 배치)을 쓴다.
	// 노드 안의 작은 입력칸(DrawPinValue)은 캔버스 전용으로 그대로 둔다.
	const auto it = node.InputValues.find(pin.Key);
	const Engine::GraphValue& value = it != node.InputValues.end() ? it->second : pin.DefaultValue;
	const auto apply = [&](Engine::GraphValue next)
	{
		if (m_ShaderGraph.Data.SetInputValue(m_GraphRegistry, { node.Id, pin.Key }, std::move(next)).Succeeded())
			CommitShaderGraphEdit();
	};

	reflection::Metadata metadata;
	(void)metadata.Add("Step", 0.01);

	switch (pin.Type)
	{
	case Engine::EGraphPinType::Bool:
	{
		bool current = std::holds_alternative<bool>(value) && std::get<bool>(value);
		if (ImGui::Checkbox("##PinValue", &current)) apply(current);
		break;
	}
	case Engine::EGraphPinType::Int:
	{
		// GraphValue의 정수는 int64_t다.
		std::int64_t current = std::holds_alternative<std::int64_t>(value) ? std::get<std::int64_t>(value) : 0;
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::DragScalar("##PinValue", ImGuiDataType_S64, &current)) apply(current);
		break;
	}
	case Engine::EGraphPinType::Float:
	{
		float current = std::holds_alternative<float>(value) ? std::get<float>(value) : 0.0f;
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::DragFloat("##PinValue", &current, 0.01f, 0.0f, 0.0f, "%.3f")) apply(current);
		break;
	}
	case Engine::EGraphPinType::Float2:
	{
		auto current = std::holds_alternative<std::array<float, 2>>(value) ? std::get<std::array<float, 2>>(value) : std::array<float, 2>{};
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::DragFloat2("##PinValue", current.data(), 0.01f, 0.0f, 0.0f, "%.3f")) apply(current);
		break;
	}
	case Engine::EGraphPinType::Float3:
	{
		const auto stored = std::holds_alternative<std::array<float, 3>>(value) ? std::get<std::array<float, 3>>(value) : std::array<float, 3>{};
		glm::vec3 current(stored[0], stored[1], stored[2]);
		if (ReflectionPropertyDrawer::DrawVector3(metadata, current))
			apply(std::array<float, 3>{ current.x, current.y, current.z });
		break;
	}
	case Engine::EGraphPinType::Float4:
	{
		const auto stored = std::holds_alternative<std::array<float, 4>>(value) ? std::get<std::array<float, 4>>(value) : std::array<float, 4>{};
		glm::vec4 current(stored[0], stored[1], stored[2], stored[3]);
		if (ReflectionPropertyDrawer::DrawVector4(metadata, current))
			apply(std::array<float, 4>{ current.x, current.y, current.z, current.w });
		break;
	}
	case Engine::EGraphPinType::String:
	{
		std::string current = std::holds_alternative<std::string>(value) ? std::get<std::string>(value) : std::string{};
		ImGui::SetNextItemWidth(-FLT_MIN);
		if (ImGui::InputText("##PinValue", &current)) apply(std::move(current));
		break;
	}
	default:
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%s", PinTypeName(pin.Type));
		break;
	}
}

void MaterialEditor::DrawShaderBuildDetails()
{
	auto& build = m_ShaderGraph.ShaderBuild;

	if (ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.ShaderDiagnostics"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (!build.Details.empty())
		{
			// 컴파일러 메시지는 공백 없는 긴 절대 경로를 포함한다. 패널 폭에서 줄바꿈하고 복사 버튼과 전체 내용 툴팁을 둔다.
			if (EditorUI::SmallIconButton("CopyDiagnostics", ICON_FA_COPY, "MaterialEditor.CopyDiagnostics"))
				ImGui::SetClipboardText(build.Details.c_str());

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextUnformatted(build.Details.c_str());
			ImGui::PopTextWrapPos();
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 45.0f);
				ImGui::TextUnformatted(build.Details.c_str());
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
		}
		else
		{
			ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "%s", LOCAL_CSTR("GraphPanel.Valid"));
		}
	}

	if (ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.GeneratedHlsl"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Button((ICON_FA_COPY " " + LOCAL("MaterialEditor.CopyHlsl")).c_str()))
		{
			ImGui::SetClipboardText(build.Source.c_str());
		}
		ImGui::SameLine();
		if (ImGui::Button((ICON_FA_ARROWS_ROTATE " " + LOCAL("MaterialEditor.SourceGenerated")).c_str()))
		{
			GenerateShaderSource();
		}

		// 읽기 전용 여러 줄 입력창: 일부를 선택해 Ctrl+C로 복사할 수 있고, 긴 줄은 가로 스크롤한다.
		ImGui::InputTextMultiline("##HlslView", &build.Source, ImVec2(-FLT_MIN, ImGui::GetTextLineHeight() * 16.0f),
			ImGuiInputTextFlags_ReadOnly);
	}
}

void MaterialEditor::DrawShaderDiagnostics()
{
	auto& build = m_ShaderGraph.ShaderBuild;

	if (ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.ShaderDiagnostics"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (!build.Details.empty())
		{
			if (EditorUI::SmallIconButton("CopyDiagnostics", ICON_FA_COPY, "MaterialEditor.CopyDiagnostics"))
				ImGui::SetClipboardText(build.Details.c_str());

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextUnformatted(build.Details.c_str());
			ImGui::PopTextWrapPos();
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered())
			{
				ImGui::BeginTooltip();
				ImGui::PushTextWrapPos(ImGui::GetFontSize() * 45.0f);
				ImGui::TextUnformatted(build.Details.c_str());
				ImGui::PopTextWrapPos();
				ImGui::EndTooltip();
			}
		}
		else
		{
			ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "%s", LOCAL_CSTR("GraphPanel.Valid"));
		}
	}
}

void MaterialEditor::DrawGeneratedHlsl()
{
	auto& build = m_ShaderGraph.ShaderBuild;

	if (ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.GeneratedHlsl"), ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (ImGui::Button((ICON_FA_COPY " " + LOCAL("MaterialEditor.CopyHlsl")).c_str()))
		{
			ImGui::SetClipboardText(build.Source.c_str());
		}
		ImGui::SameLine();
		if (ImGui::Button((ICON_FA_ARROWS_ROTATE " " + LOCAL("MaterialEditor.SourceGenerated")).c_str()))
		{
			GenerateShaderSource();
		}

		// 남은 하단 영역 전체를 채우도록 -FLT_MIN 지정
		ImGui::InputTextMultiline("##HlslView", &build.Source, ImVec2(-FLT_MIN, -FLT_MIN), ImGuiInputTextFlags_ReadOnly);
	}
}

bool MaterialEditor::HasLinkedShaderGraph() const
{
	return m_Material && !m_Material->GetShaderGraphPath().empty();
}

std::filesystem::path MaterialEditor::ResolveShaderGraphPath(const std::string& storedPath) const
{
	// 피커가 돌려주는 가상 URI(Project:// 등), 프로젝트 기준 상대 경로, 이전 파일의 머티리얼 폴더 기준 상대 경로를 차례로 확인한다.
	if (storedPath.empty()) return {};
	if (PlatformPath::IsVirtualUri(storedPath))
		return PlatformPath::ResolveUri(storedPath);

	const auto projectRelative = PlatformPath::ResolveProjectRelative(storedPath);
	if (std::filesystem::exists(projectRelative)) return projectRelative;

	const auto besideMaterial = m_TargetResourcePath.parent_path() / Engine::Utf8ToPath(storedPath);
	return std::filesystem::exists(besideMaterial) ? besideMaterial : projectRelative;
}

void MaterialEditor::TryLoadLinkedShaderGraph()
{
	// 연결된 그래프를 읽기만 한다. 없는 파일을 만들거나 머티리얼 경로·셰이더를 바꾸지 않는다.
	if (!HasLinkedShaderGraph()) return;

	const std::string storedPath = m_Material->GetShaderGraphPath();
	const auto targetPath = ResolveShaderGraphPath(storedPath);
	if (!std::filesystem::exists(targetPath))
	{
		m_ShaderGraph.ShaderBuild.StatusKey = "MaterialEditor.ShaderGraphMissing";
		m_ShaderGraph.ShaderBuild.Details = storedPath;
		return;
	}

	if (!LoadShaderGraph(targetPath))
	{
		m_ShaderGraph.ShaderBuild.StatusKey = "MaterialEditor.ShaderGraphLoadFailed";
		m_ShaderGraph.ShaderBuild.Details = m_LastGraphDiagnostic.Message;
	}
}

bool MaterialEditor::LinkShaderGraph(const std::string& graphUri)
{
	// 고른 그래프를 먼저 불러와 보고, 셰이더 그래프로 읽혔을 때만 머티리얼에 연결한다.
	if (!m_Material || m_IsReadOnly || graphUri.empty()) return false;

	const auto targetPath = ResolveShaderGraphPath(graphUri);
	m_ShaderGraph = {};
	m_ShaderGraphHistory.Clear();
	m_GraphReady = false;
	if (!std::filesystem::exists(targetPath) || !LoadShaderGraph(targetPath))
	{
		NotificationManager::Get().AddNotification(LOCAL("ResourceEditor.MaterialTitle"),
			LOCAL("MaterialEditor.ShaderGraphLoadFailed"), ELogLevel::Warn, 4.0f);
		// 연결은 바꾸지 않았으므로 기존 그래프를 다시 읽는다.
		TryLoadLinkedShaderGraph();
		return false;
	}

	m_Material->SetShaderGraphPath(graphUri);
	m_History.MarkEdited();
	return true;
}

void MaterialEditor::UnlinkShaderGraph()
{
	if (!m_Material || m_IsReadOnly || !HasLinkedShaderGraph()) return;
	m_Material->SetShaderGraphPath("");
	m_History.MarkEdited();
	m_ShaderGraph = {};
	m_ShaderGraphHistory.Clear();
	m_GraphReady = false;
	m_LiveCompileCooldown = 0.0f;
	m_SelectedNode = 0;
	m_SelectedLink = 0;
	m_SelectedNodes.clear();
	m_SelectedLinks.clear();
}

void MaterialEditor::DrawShaderGraphLinkControls()
{
	// 인스펙터의 Animator·Timeline 지정과 같은 경로 피커: 목록 선택·드래그 앤 드롭·해제를 지원한다.
	const bool canEdit = m_Material && !m_IsReadOnly;
	const std::string current = m_Material ? m_Material->GetShaderGraphPath() : std::string{};
	std::string picked;
	ImGui::PushID("ShaderGraphLink");
	if (DrawInspectorAssetPathPicker(current, picked, ShaderGraphPicker, canEdit) && canEdit && picked != current)
	{
		if (picked.empty())
			UnlinkShaderGraph();
		else
			(void)LinkShaderGraph(picked);
	}

	if (!HasLinkedShaderGraph())
	{
		ImGui::BeginDisabled(!canEdit);
		if (ImGui::Button((ICON_FA_PLUS " " + LOCAL("MaterialEditor.CreateShaderGraph")).c_str(), ImVec2(-FLT_MIN, 0.0f)))
			CreateShaderGraphForMaterial();
		ImGui::EndDisabled();
	}
	ImGui::PopID();
}

void MaterialEditor::CreateShaderGraphForMaterial()
{
	// 사용자가 요청했을 때만 기본 그래프를 만들고 머티리얼에 연결한다.
	if (!m_Material || m_IsReadOnly || HasLinkedShaderGraph()) return;

	const auto targetPath = GenerateDefaultShaderGraphPath();
	if (!std::filesystem::exists(targetPath) && !CreateDefaultMaterialShaderGraph(targetPath))
	{
		NotificationManager::Get().AddNotification(LOCAL("ResourceEditor.MaterialTitle"),
			LOCAL("MaterialEditor.ShaderGraphCreateFailed"), ELogLevel::Error, 4.0f);
		return;
	}

	m_Material->SetShaderGraphPath(PlatformPath::ToProjectRelative(targetPath).string());
	m_History.MarkEdited();
	m_ShaderGraph = {};
	m_ShaderGraphHistory.Clear();
	m_GraphReady = false;
	TryLoadLinkedShaderGraph();
}

void MaterialEditor::DrawNoShaderGraphState(float width, float height)
{
	// 그래프가 없거나 읽지 못한 머티리얼도 중앙 영역을 유지하고, 안내와 함께 기존 그래프 지정·새로 만들기를 보여 준다.
	if (ImGui::BeginChild("##MaterialCenterCanvasArea", ImVec2(width, height), ImGuiChildFlags_Borders))
	{
		const bool missing = HasLinkedShaderGraph();
		const auto message = LOCAL(missing ? m_ShaderGraph.ShaderBuild.StatusKey.c_str() : "MaterialEditor.NoShaderGraph");
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		const float boxWidth = (std::min)(avail.x, ImGui::GetFontSize() * 24.0f);
		const float textHeight = ImGui::CalcTextSize(message.c_str(), nullptr, false, boxWidth).y;
		// 안내 1줄 + 피커 + 만들기 버튼 높이로 세로 중앙을 잡는다.
		const float blockHeight = textHeight + (ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y) * 2.0f;
		ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + (std::max)(0.0f, (avail.x - boxWidth) * 0.5f),
			ImGui::GetCursorPosY() + (std::max)(0.0f, (avail.y - blockHeight) * 0.5f)));

		if (ImGui::BeginChild("##ShaderGraphLinkBox", ImVec2(boxWidth, 0.0f), ImGuiChildFlags_AutoResizeY))
		{
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextDisabled("%s", message.c_str());
			ImGui::PopTextWrapPos();
			if (missing && !m_ShaderGraph.ShaderBuild.Details.empty() && ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", m_ShaderGraph.ShaderBuild.Details.c_str());
			ImGui::Spacing();
			DrawShaderGraphLinkControls();
		}
		ImGui::EndChild();
	}
	ImGui::EndChild();
}

bool MaterialEditor::LoadShaderGraph(const std::filesystem::path& path)
{
	if (m_ShaderGraph.LoadedPath == path && m_GraphReady) return true;

	if (!ReflectionRegistry::IsCreated()) return false;
	const auto& reflection = ReflectionRegistry::Get();

	Engine::Graph candidate(Engine::EGraphDomain::Shader);
	const auto result = Engine::LoadGraphJson(path, candidate, m_GraphRegistry,
		reflection.GetRegistry(), reflection.GetPropertyCodecs());

	if (!result.Succeeded())
	{
		m_LastGraphDiagnostic = result.GraphError;
		EDITOR_LOG_ERROR("Failed to load shader graph {}: {}", path.string(), result.GraphError.Message);
		return false;
	}

	m_ShaderGraph.Data = std::move(candidate);
	m_ShaderGraph.FilePath = path;
	m_ShaderGraph.LoadedPath = path;
	m_ShaderGraphHistory.Reset(m_ShaderGraph.Data);
	m_GraphReady = true;
	m_NeedsNavigateToContent = true;
	// ── 그래프 로드 성공 시 노드 핀 값들을 머티리얼 파라미터로 자동 동기화 ──
	SyncParametersFromGraph();
	InvalidateShaderBuild();
	GenerateShaderSource();
	return true;
}

bool MaterialEditor::SaveShaderGraph()
{
	if (!m_GraphReady || m_ShaderGraph.FilePath.empty()) return true;

	if (!ReflectionRegistry::IsCreated()) return false;
	const auto& reflection = ReflectionRegistry::Get();

	const auto result = Engine::SaveGraphJson(m_ShaderGraph.FilePath, m_ShaderGraph.Data, m_GraphRegistry,
		reflection.GetRegistry(), reflection.GetPropertyCodecs());

	if (result.Succeeded())
	{
		m_ShaderGraphHistory.MarkSaved();
		return true;
	}
	return false;
}

std::filesystem::path MaterialEditor::GenerateDefaultShaderGraphPath() const
{
	std::string baseName = "NewMaterial";
	if (m_Material && !m_TargetResourcePath.empty())
	{
		baseName = m_TargetResourcePath.stem().string();
		const std::string bammat = ".bammat";
		if (baseName.size() > bammat.size() && baseName.ends_with(bammat))
			baseName = baseName.substr(0, baseName.size() - bammat.size());
	}
	const std::string filename = baseName + ".bamgraph.json";
	return PlatformPath::ResolveProjectRelative(std::filesystem::path("Assets/Shaders/Graphs") / filename);
}

bool MaterialEditor::CreateDefaultMaterialShaderGraph(const std::filesystem::path& path) const
{
	try
	{
		std::filesystem::create_directories(path.parent_path());

		Engine::Graph graph(Engine::EGraphDomain::Shader);
		Engine::GraphRegistry registry;
		(void)registry.RegisterBuiltins();

		(void)graph.AddNode(registry, "Shader.MaterialOutput");
		(void)graph.SetNodePosition(1, 400.0f, 250.0f);

		if (!ReflectionRegistry::IsCreated()) return false;
		const auto& reflection = ReflectionRegistry::Get();
		const auto result = Engine::SaveGraphJson(path, graph, registry,
			reflection.GetRegistry(), reflection.GetPropertyCodecs());

		return result.Succeeded();
	}
	catch (const std::exception& e)
	{
		EDITOR_LOG_ERROR("Failed to create default shader graph: {}", e.what());
		return false;
	}
}

void MaterialEditor::SaveTarget()
{
	if (!CommitDocumentEdit()) return;
	if (!m_Material || m_IsReadOnly) return;

	auto& manager = Engine::ResourceManager::Get();
	const std::string path = m_TargetResourcePath.string();
	const EResult result = path.ends_with(".json")
		? manager.SaveToJsonFile(m_Material, m_TargetResourcePath.wstring())
		: manager.SaveToBinaryFile(m_Material, m_TargetResourcePath.wstring());

	if (result != EResult::Success)
	{
		EDITOR_LOG_ERROR("Failed to save material: {}", path);
		NotificationManager::Get().AddNotification(GetDisplayName(), LOCAL_FMT("Notification.SaveFailed", path), ELogLevel::Error);
		return;
	}

	SaveShaderGraph();
	m_History.MarkSaved();
	if (AssetCache::IsCreated())
		AssetCache::Get().RequestInvalidate(m_TargetResourcePath);
	EDITOR_LOG_INFO("Material saved: {}", path);
}

std::optional<std::string> MaterialEditor::CaptureDocument() const
{
	archive::JsonArchive ar;
	std::string snapshot;
	if (!m_Material || !m_Material->WriteArchive(ar) || !ar.ToJson(snapshot)) return std::nullopt;
	return snapshot;
}

bool MaterialEditor::PrepareAssetPathRelocation(AssetPathRelocation& relocation)
{
	if (!PrepareTargetPathRelocation(relocation)) return false;
	if (m_Material)
	{
		relocation.AfterCommit([this] {
			if (auto snapshot = CaptureDocument()) m_History.Reset(std::move(*snapshot));
			else m_History.Clear();
		});
	}
	return true;
}

bool MaterialEditor::CommitDocumentEdit()
{
	if (!m_History.HasPendingEdit()) return true;
	auto snapshot = CaptureDocument();
	return snapshot && m_History.Record(std::move(*snapshot),
		[this](const auto& value) { return ApplyDocumentSnapshot(value); }, LOCAL("History.Command.EditProperty"));
}

bool MaterialEditor::ApplyDocumentSnapshot(const std::string& snapshot)
{
	if (!m_Material) return false;
	archive::JsonArchive ar;
	archive::JsonArchive previous;
	if (!ar.Parse(snapshot) || !m_Material->WriteArchive(previous)) return false;
	if (!m_Material->ReadArchive(ar))
	{
		(void)m_Material->ReadArchive(previous);
		return false;
	}
	m_Material->IncreaseVersion();
	m_PreviewDirtyTimer = 0.5f;
	return true;
}

bool MaterialEditor::CommitShaderGraphEdit()
{
	InvalidateShaderBuild();
	m_ShaderGraphHistory.Record(m_ShaderGraph.Data,
		[this](const auto& graph) {
			m_ShaderGraph.Data = graph;
			InvalidateShaderBuild();
			return true;
		}, LOCAL("History.Command.EditProperty"));

	// 자동 컴파일 활성화 시 0.25초 쿨다운 시작
	if (m_AutoCompileEnabled)
	{
		m_LiveCompileCooldown = 0.25f;
	}

	// ── 그래프 노드 핀 값 변경 시 머티리얼 파라미터 및 3D 프리뷰 즉시 동기화 ──
	SyncParametersFromGraph();
	if (m_Material)
	{
		m_Material->IncreaseVersion();
		m_PreviewDirtyTimer = 0.2f;
	}
	return true;
}

bool MaterialEditor::CanUndoDocument() const
{
	return CanSaveDocument() && (m_History.CanUndo() || m_ShaderGraphHistory.CanUndo());
}

bool MaterialEditor::CanRedoDocument() const
{
	return CanSaveDocument() && (m_History.CanRedo() || m_ShaderGraphHistory.CanRedo());
}

void MaterialEditor::UndoDocument()
{
	if (m_ShaderGraphHistory.CanUndo()) m_ShaderGraphHistory.Undo();
	else if (m_History.CanUndo()) m_History.Undo();
}

void MaterialEditor::RedoDocument()
{
	if (m_ShaderGraphHistory.CanRedo()) m_ShaderGraphHistory.Redo();
	else if (m_History.CanRedo()) m_History.Redo();
}

void MaterialEditor::CaptureState(EditorWorkspace& workspace) const
{
	IImGui::CaptureState(workspace);
	workspace.SetSplitter("Material.LeftPaneWidth", m_LeftPaneWidth);
	workspace.SetSplitter("Material.PreviewHeight", m_PreviewHeight);
	workspace.SetSplitter("Material.RightPaneWidth", m_RightPaneWidth);
}

void MaterialEditor::RestoreState(const EditorWorkspace& workspace)
{
	IImGui::RestoreState(workspace);
	m_LeftPaneWidth = workspace.GetSplitter("Material.LeftPaneWidth", 320.0f);
	m_PreviewHeight = workspace.GetSplitter("Material.PreviewHeight", 280.0f);
	m_RightPaneWidth = workspace.GetSplitter("Material.RightPaneWidth", 300.0f);
}

void MaterialEditor::AlignNodes(int alignment) { AlignSelectedNodes(alignment); }
void MaterialEditor::DuplicateSelectedNodes() { DuplicateSelection(); }
void MaterialEditor::CreateCommentBox() { CreateCommentBoxFromSelection(); }

void MaterialEditor::SyncParametersFromGraph()
{
	if (!m_Material || m_IsReadOnly || !m_GraphReady) return;

	// 마스터 노드(Shader.MaterialOutput) 탐색
	const Engine::GraphNode* masterNode = nullptr;
	for (const auto& node : m_ShaderGraph.Data.GetNodes())
	{
		if (node.Type == "Shader.MaterialOutput")
		{
			masterNode = &node;
			break;
		}
	}
	if (!masterNode) return;

	const auto* schema = m_GraphRegistry.Find(masterNode->Type);
	if (!schema) return;

	const auto links = m_ShaderGraph.Data.GetLinks();
	const auto hasLink = [&](const std::string& pin) {
		return std::any_of(links.begin(), links.end(), [&](const Engine::GraphLink& link) {
			return link.To.Node == masterNode->Id && link.To.Pin == pin;
			});
		};

	// 마스터 노드의 입력 핀 순회 (링크가 연결되지 않은 핀의 값을 머티리얼 파라미터로 동기화)
	for (const auto& pin : schema->Pins)
	{
		if (pin.Direction != Engine::EGraphPinDirection::Input) continue;
		if (hasLink(pin.Key)) continue; // 노드 링크 연결이 있으면 그래프 연산식이 우선

		const auto it = masterNode->InputValues.find(pin.Key);
		const Engine::GraphValue& graphVal = (it != masterNode->InputValues.end()) ? it->second : pin.DefaultValue;

		if (std::holds_alternative<float>(graphVal))
		{
			m_Material->SetFloat(pin.Key, std::get<float>(graphVal));
		}
		else if (std::holds_alternative<std::array<float, 3>>(graphVal))
		{
			const auto arr = std::get<std::array<float, 3>>(graphVal);
			m_Material->SetFloat3(pin.Key, Engine::vec3(arr[0], arr[1], arr[2]));
		}
		else if (std::holds_alternative<std::array<float, 4>>(graphVal))
		{
			const auto arr = std::get<std::array<float, 4>>(graphVal);
			m_Material->SetFloat4(pin.Key, Engine::vec4(arr[0], arr[1], arr[2], arr[3]));
		}
	}

	// 기본 PBR 핀이 누락되어 있다면 표준 기본값 채움
	const auto& curParams = m_Material->GetParameters();
	const auto hasParam = [&](const std::string& name) {
		const std::string lower = Engine::ToLowerStr(name);
		return std::any_of(curParams.begin(), curParams.end(), [&](const auto& pair) {
			return Engine::ToLowerStr(pair.first) == lower;
			});
		};
	if (!hasParam("baseColor")) m_Material->SetFloat4("baseColor", Engine::vec4(1.0f));
	if (!hasParam("roughness")) m_Material->SetFloat("roughness", 0.5f);
	if (!hasParam("metallic")) m_Material->SetFloat("metallic", 0.0f);
	if (!hasParam("emissive")) m_Material->SetFloat3("emissive", Engine::vec3(0.0f));
}

void MaterialEditor::SyncGraphFromParameter(const std::string& name, const void* data, Engine::EMaterialParameterType type)
{
	if (!m_GraphReady || !data) return;

	for (const auto& node : m_ShaderGraph.Data.GetNodes())
	{
		if (node.Type == "Shader.MaterialOutput")
		{
			const std::string lower = Engine::ToLowerStr(name);
			const auto* schema = m_GraphRegistry.Find(node.Type);
			if (!schema) break;

			for (const auto& pin : schema->Pins)
			{
				if (pin.Direction != Engine::EGraphPinDirection::Input) continue;
				if (Engine::ToLowerStr(pin.Key) == lower)
				{
					Engine::GraphValue nextVal;
					if (type == Engine::EMaterialParameterType::Float)
					{
						nextVal = *reinterpret_cast<const float*>(data);
					}
					else if (type == Engine::EMaterialParameterType::Float3)
					{
						const auto v = *reinterpret_cast<const Engine::vec3*>(data);
						nextVal = std::array<float, 3>{ v.x, v.y, v.z };
					}
					else if (type == Engine::EMaterialParameterType::Float4)
					{
						const auto v = *reinterpret_cast<const Engine::vec4*>(data);
						nextVal = std::array<float, 4>{ v.x, v.y, v.z, v.w };
					}

					// 공식 API인 SetInputValue를 통해 안전하게 핀 값 수정
					(void)m_ShaderGraph.Data.SetInputValue(m_GraphRegistry, { node.Id, pin.Key }, std::move(nextVal));
					break;
				}
			}
			break;
		}
	}
}