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
#include <archive/JsonArchive.h>

namespace
{
	template<size_t N>
	bool LocalizedCombo(const char* label, int* selected, const char* (&keys)[N])
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
}

bool MaterialEditor::IsSupported(const std::filesystem::path& assetPath) const
{
	const std::string name = Engine::ToLowerStr(Engine::WStrToStr(assetPath.filename().wstring()));
	return name.ends_with(".bammat.json") || name.ends_with(".bammatinst.json") ||
		name.ends_with(".bammat") || name.ends_with(".bammatinst") ||
		name.ends_with(".mat");
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
}

bool MaterialEditor::CanSaveDocument() const
{
	return m_Open && m_Material && !m_IsReadOnly;
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
	else m_History.Clear();
	if (AssetCache::IsCreated()) AssetCache::Get().RequestInvalidate(m_TargetResourcePath);
	return true;
}

void MaterialEditor::ReleaseResourceDocument()
{
	m_PreviewRenderer.ReleaseMaterial();
	m_Material = nullptr;
	m_MaterialHandle.Reset();
	m_History.Clear();
	m_IsReadOnly = false;
}

void MaterialEditor::Update(f32 dt)
{
	if (!m_Open || !m_IsVisibleOnScreen || !m_Material)
		return;
	// [안전 방어]: 베이스 머티리얼이 없는 인스턴스는 렌더러 크래시 방지를 위해 프리뷰 제출 차단
	auto* inst = dynamic_cast<Engine::MaterialInstance*>(m_Material);
	if (inst && !inst->GetBaseMaterial())
		return;
	if (m_PreviewDirtyTimer > 0.0f)
	{
		m_PreviewDirtyTimer -= dt;
		m_PreviewRenderer.Submit(m_Material, dt);
	}
	else
	{
		static float s_IdleTimer = 0.0f;
		s_IdleTimer += dt;
		if (s_IdleTimer >= 0.2f)
		{
			s_IdleTimer = 0.0f;
			m_PreviewRenderer.Submit(m_Material, dt);
		}
	}
}

void MaterialEditor::OnResourceWindowVisibility(bool visible)
{
	m_IsVisibleOnScreen = visible;
	if (visible && (m_Focused || m_Hovered)) m_PreviewDirtyTimer = 0.5f;
}

void MaterialEditor::DrawCustomContent()
{
	if (!m_Material)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", LOCAL_FMT("MaterialEditor.LoadFailed", m_TargetResourcePath.string()).c_str());
		if (ImGui::Button(LOCAL_CSTR("MaterialEditor.Retry")))
			SetTargetResource(m_TargetResourcePath);
		return;
	}

	const float availWidth = ImGui::GetContentRegionAvail().x;
	const float splitterThickness = Editor::SplitterThickness;
	const float spacingX = ImGui::GetStyle().ItemSpacing.x;
	// 우측 디테일 창 가용 너비 계산 (최소 320px 보장)
	const float minLeft = 250.0f;
	const float minRight = 320.0f;
	float detailsWidth = (std::max)(minRight, availWidth - m_ViewportWidth - splitterThickness - spacingX);
	// ── [좌측] 3D 머티리얼 뷰포트 ──
	ImGuiWindowFlags vpFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
	if (ImGui::BeginChild("##MaterialViewportPane", ImVec2(m_ViewportWidth, 0.0f), true, vpFlags))
	{
		m_PreviewRenderer.DrawUI(m_Material);
	}
	ImGui::EndChild();
	ImGui::SameLine();
	// ── [스플리터] 뷰포트 ↔ 디테일 패널 ──
	Editor::SplitterV("##MaterialSplitter", &m_ViewportWidth, &detailsWidth, minLeft, minRight, 0.0f, splitterThickness);
	ImGui::SameLine();
	// ── [우측] 프로퍼티 & 파라미터 패널 (남은 가용 폭 전체 채움) ──
	if (ImGui::BeginChild("##MaterialDetailsPane", ImVec2(0.0f, 0.0f), true))
	{
		DrawPipelineSettings();
		DrawShaderSettings();
		DrawTextureBindings();
		DrawParameters();
	}
	ImGui::EndChild();
	if (!IsHistoryGestureActive()) CommitDocumentEdit();
}

void MaterialEditor::SaveTarget()
{
	if (!CommitDocumentEdit()) return;
	if (!m_Material || m_IsReadOnly)
		return;

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
		// Captured material archives contain resource paths. Establish a new baseline after those paths can change.
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

bool MaterialEditor::CanUndoDocument() const
{
	return CanSaveDocument() && m_History.CanUndo() && (!m_History.HasPendingEdit() || !IsHistoryGestureActive());
}

bool MaterialEditor::CanRedoDocument() const
{
	return CanSaveDocument() && m_History.CanRedo();
}

void MaterialEditor::UndoDocument()
{
	if (CanUndoDocument() && CommitDocumentEdit()) m_History.Undo();
}

void MaterialEditor::RedoDocument()
{
	if (CanRedoDocument()) m_History.Redo();
}

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
		if (ImGui::MenuItem((ICON_FA_CROSSHAIRS " " + LOCAL_LABEL("MaterialEditor.ResetCamera")).c_str(),
			shortcuts.GetShortcutHint("Viewport.Focus")))
		{
			m_PreviewRenderer.GetWidget().FocusTarget(Engine::vec3(0.0f, 0.0f, 0.0f), 3.0f);
		}

		if (ImGui::MenuItem((ICON_FA_BORDER_ALL " " + LOCAL_LABEL("MaterialEditor.ToggleGrid")).c_str(),
			shortcuts.GetShortcutHint("Viewport.ToggleGrid"), m_PreviewRenderer.GetWidget().IsGridVisible()))
		{
			m_PreviewRenderer.GetWidget().ToggleGrid();
		}

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

void MaterialEditor::DrawCustomQuickAccess()
{
	if (!m_Material) return;

	EditorUI::ToolbarLayout toolbar;

	DrawSaveButton(toolbar);
	DrawHistoryButtons(toolbar, "MaterialEditor");
	if (toolbar.Icon("RevertMaterial", ICON_FA_ROTATE_LEFT, "MaterialEditor.Revert", { .Enabled = CanSaveDocument() && HasUnsavedResourceChanges() }))
		RunWithDiscardConfirmation([this] { RestoreSavedChanges(); }, false);

	if (toolbar.Icon("FocusMaterial", ICON_FA_CROSSHAIRS, "MaterialEditor.ResetCamera"))
	{
		m_PreviewRenderer.GetWidget().FocusTarget(Engine::vec3(0.0f, 0.0f, 0.0f), 3.0f);
	}

	EditorUI::IconButtonOptions gridOptions;
	gridOptions.Active = m_PreviewRenderer.GetWidget().IsGridVisible();
	if (toolbar.Icon("ToggleGridMaterial", ICON_FA_BORDER_ALL, "MaterialEditor.ToggleGrid", gridOptions))
	{
		m_PreviewRenderer.GetWidget().ToggleGrid();
	}

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

void MaterialEditor::DrawPipelineSettings()
{
	if (!ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.PipelineSettings"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	ImGui::BeginDisabled(m_IsReadOnly);

	auto* inst = dynamic_cast<Engine::MaterialInstance*>(m_Material);
	if (inst)
	{
		auto baseHandle = inst->GetBaseMaterialHandle();
		Engine::Material* baseMat = inst->GetBaseMaterial();
		if (!baseMat)
		{
			// 베이스 머티리얼 누락 시 경고 배너 표시
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.2f, 1.0f));
			ImGui::TextWrapped(ICON_FA_TRIANGLE_EXCLAMATION " %s", LOCAL_CSTR("MaterialEditor.BaseMaterialMissing"));
			ImGui::PopStyleColor();
		}
		ImGui::AlignTextToFramePadding();
		ImGui::Text("%s:", LOCAL_CSTR("MaterialEditor.BaseMaterialSlot"));
		ImGui::SameLine();
		// 베이스 머티리얼을 다시 연결할 수 있는 피커 슬롯 제공
		ImGui::PushID("BaseMaterialSlot");
		if (Editor::DrawInspectorResourcePicker(baseHandle, !m_IsReadOnly) && !m_IsReadOnly)
		{
			inst->SetBaseMaterial(baseHandle);
			m_History.MarkEdited();
		}
		ImGui::PopID();
		ImGui::Separator();
	}

	// Blend Mode
	constexpr Engine::EBlendMode blendValues[]{ Engine::EBlendMode::Opaque, Engine::EBlendMode::Masked,
		Engine::EBlendMode::AlphaBlend, Engine::EBlendMode::Additive, Engine::EBlendMode::None,
		Engine::EBlendMode::NonPremultiplied, Engine::EBlendMode::Forward, Engine::EBlendMode::Decal };
	const char* blendModes[]{ "MaterialOption.Opaque", "MaterialOption.Masked", "MaterialOption.Translucent",
		"MaterialOption.Additive", "MaterialOption.None", "MaterialOption.NonPremultiplied",
		"MaterialOption.Forward", "MaterialOption.Decal" };
	int blendIdx = -1;
	for (int i = 0; i < IM_ARRAYSIZE(blendValues); ++i)
		if (blendValues[i] == m_Material->GetBlendMode()) blendIdx = i;
	if (LocalizedCombo(LOCAL_LABEL("MaterialEditor.BlendMode").c_str(), &blendIdx, blendModes))
	{
		m_Material->SetBlendMode(blendValues[blendIdx]);
		m_History.MarkEdited();
	}

	// Cull Mode
	int cullIdx = static_cast<int>(m_Material->GetCullMode());
	const char* cullModes[] = { "MaterialOption.None", "MaterialOption.Front", "MaterialOption.Back" };
	if (LocalizedCombo(LOCAL_LABEL("MaterialEditor.CullMode").c_str(), &cullIdx, cullModes))
	{
		m_Material->SetCullMode(static_cast<Engine::ECullMode>(cullIdx));
		m_History.MarkEdited();
	}

	// Fill Mode
	int fillIdx = static_cast<int>(m_Material->GetFillMode());
	const char* fillModes[] = { "MaterialOption.Wireframe", "MaterialOption.Solid" };
	if (LocalizedCombo(LOCAL_LABEL("MaterialEditor.FillMode").c_str(), &fillIdx, fillModes))
	{
		m_Material->SetFillMode(static_cast<Engine::EFillMode>(fillIdx));
		m_History.MarkEdited();
	}

	// Depth Mode
	int depthIdx = static_cast<int>(m_Material->GetDepthMode());
	const char* depthModes[] = { "MaterialOption.None", "MaterialOption.ReadOnly", "MaterialOption.ReadWrite" };
	if (LocalizedCombo(LOCAL_LABEL("MaterialEditor.DepthMode").c_str(), &depthIdx, depthModes))
	{
		m_Material->SetDepthMode(static_cast<Engine::EDepthMode>(depthIdx));
		m_History.MarkEdited();
	}

	// Depth Compare Op
	int opIdx = static_cast<int>(m_Material->GetDepthCompareOp());
	const char* compareOps[] = { "MaterialOption.Never", "MaterialOption.Less", "MaterialOption.Equal", "MaterialOption.LessEqual", "MaterialOption.Greater", "MaterialOption.NotEqual", "MaterialOption.GreaterEqual", "MaterialOption.Always" };
	if (LocalizedCombo(LOCAL_LABEL("MaterialEditor.DepthCompareOp").c_str(), &opIdx, compareOps))
	{
		m_Material->SetDepthCompareOp(static_cast<Engine::ECompareOp>(opIdx));
		m_History.MarkEdited();
	}

	ImGui::EndDisabled();
}

void MaterialEditor::DrawShaderSettings()
{
	if (!ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.ShaderSettings"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	ImGui::BeginDisabled(m_IsReadOnly);

	auto DrawShaderSlot = [&](const std::string& slotId, const std::string& label, Engine::ResourceHandle<Engine::Shader> currentShader, float btnWidth, auto onAssign)
		{
			if (!label.empty())
			{
				ImGui::Text("%s:", label.c_str());
				ImGui::SameLine();
			}
			ImGui::PushID(slotId.c_str());
			if (btnWidth > 0.0f) ImGui::SetNextItemWidth(btnWidth);
			if (Editor::DrawInspectorResourcePicker(currentShader, !m_IsReadOnly) && !m_IsReadOnly)
			{
				onAssign(currentShader);
				m_History.MarkEdited();
			}
			ImGui::PopID();
		};
	// 1. 기본 셰이더 (Default Fallback)
	ImGui::TextDisabled("%s", LOCAL_CSTR("MaterialEditor.DefaultShaders"));
	DrawShaderSlot("DefaultVS", LOCAL("MaterialEditor.DefaultVS"), m_Material->GetVertexShaderHandle(), 240.0f, [&](auto handle) { m_Material->SetVertexShaderHandle(handle); });
	DrawShaderSlot("DefaultPS", LOCAL("MaterialEditor.DefaultPS"), m_Material->GetPixelShaderHandle(), 240.0f, [&](auto handle) { m_Material->SetPixelShaderHandle(handle); });
	ImGui::Spacing();
	ImGui::Separator();
	// 2. 패스별 셰이더 오버라이드
	ImGui::TextDisabled("%s", LOCAL_CSTR("MaterialEditor.PassSpecificShaders"));

	const char* passTypeNames[] = {
		"MaterialOption.Custom", "MaterialOption.Geometry", "MaterialOption.Lighting", "MaterialOption.Shadow", "MaterialOption.Forward", "MaterialOption.ForwardTransparent", "MaterialOption.PostProcess", "MaterialOption.UI"
	};

	const auto& passShaders = m_Material->GetPassShaders();
	int passToRemove = -1;

	if (ImGui::BeginTable("PassShadersTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.PassType"), ImGuiTableColumnFlags_WidthFixed, 110.0f);
		ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.VertexShader"), ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.PixelShader"), ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("##Action", ImGuiTableColumnFlags_WidthFixed, EditorUI::GetIconButtonSize(ICON_FA_TRASH, true).x);
		ImGui::TableHeadersRow();

		for (size_t i = 0; i < passShaders.size(); ++i)
		{
			const auto& passShader = passShaders[i];
			ImGui::PushID(static_cast<int>(i));
			ImGui::TableNextRow();

			// Column 0: Pass Type
			ImGui::TableSetColumnIndex(0);
			int typeIdx = static_cast<int>(passShader.passType);
			const std::string typeName = (typeIdx >= 0 && typeIdx < IM_ARRAYSIZE(passTypeNames))
				? LOCAL(passTypeNames[typeIdx]) : LOCAL("MaterialOption.Unknown");
			ImGui::Text("%s", typeName.c_str());

			// Column 1: Pass Vertex Shader (고유 ID "VS" 전달)
			ImGui::TableSetColumnIndex(1);
			DrawShaderSlot("VS", "", passShader.vertexShader, -1.0f, [&](auto handle) {
				m_Material->SetPassShader(passShader.passType, handle, passShader.pixelShader);
				});

			// Column 2: Pass Pixel Shader (고유 ID "PS" 전달)
			ImGui::TableSetColumnIndex(2);
			DrawShaderSlot("PS", "", passShader.pixelShader, -1.0f, [&](auto handle) {
				m_Material->SetPassShader(passShader.passType, passShader.vertexShader, handle);
				});

			// Column 3: 삭제
			ImGui::TableSetColumnIndex(3);
			if (EditorUI::SmallIconButton("RemovePass", ICON_FA_TRASH, "EditorCommon.Delete"))
				passToRemove = typeIdx;

			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	if (passToRemove >= 0)
	{
		m_Material->RemovePassShader(static_cast<Engine::ERenderPassType>(passToRemove));
		m_History.MarkEdited();
	}

	// 새 패스 셰이더 추가
	ImGui::Spacing();
	ImGui::PushID("NewPassControl");
	ImGui::SetNextItemWidth(140.0f);
	LocalizedCombo("##NewPassType", &m_NewPassTypeIndex, passTypeNames);
	ImGui::SameLine();
	if (ImGui::Button((ICON_FA_PLUS " " + LOCAL("MaterialEditor.AddPassShader")).c_str()))
	{
		auto passType = static_cast<Engine::ERenderPassType>(m_NewPassTypeIndex);
		if (!m_Material->HasPassShader(passType))
		{
			// 빈 핸들 대신 기존 기본 셰이더를 초기값으로 복사하여 크래시 방지
			m_Material->SetPassShader(passType, m_Material->GetVertexShaderHandle(), m_Material->GetPixelShaderHandle());
			m_History.MarkEdited();
		}
	}
	ImGui::PopID();

	ImGui::EndDisabled();
}

void MaterialEditor::DrawTextureBindings()
{
	if (!ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.TextureBindings"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	ImGui::BeginDisabled(m_IsReadOnly);

	auto* inst = dynamic_cast<Engine::MaterialInstance*>(m_Material);
	Engine::Material* baseMat = inst ? inst->GetBaseMaterial() : nullptr;

	// 1. 셰이더 요구 슬롯 파악 (머티리얼 또는 베이스 머티리얼의 픽셀 셰이더 기준)
	struct ExpectedSlot
	{
		uint32 slot;
		std::string name;
	};

	std::vector<ExpectedSlot> expectedSlots = { { 0, "Diffuse" } };
	const Engine::Shader* ps = m_Material->GetPixelShader();
	if (!ps && baseMat) ps = baseMat->GetPixelShader();

	if (ps)
	{
		const std::string psKey = Engine::ToLowerStr(WStrToStr(ps->GetKey()));
		const std::string psPath = Engine::ToLowerStr(WStrToStr(ps->GetSpirvPath()));
		const std::string combined = psKey + " " + psPath;

		if (combined.find("gbuffer") != std::string::npos)
		{
			expectedSlots = {
				{ 0, "Diffuse" },
				{ 1, "Normal" },
				{ 2, "PBR" },
				{ 3, "Emissive" }
			};
		}
		else if (combined.find("terrain") != std::string::npos)
		{
			expectedSlots = {
				{ 0, "Diffuse" },
				{ 1, "HeightMap" },
				{ 2, "RockMap" },
				{ 3, "DirtMap" },
				{ 4, "SplatMap" }
			};
		}
		else if (combined.find("sprite") != std::string::npos)
		{
			expectedSlots = { { 0, "Sprite" } };
		}
	}

	// 2. 머티리얼 인스턴스 상속 및 바인딩 행 병합
	struct BindingRow
	{
		uint32 slot = 0;
		std::string name;
		Engine::ResourceHandle<Engine::Texture> texture;
		Engine::ResourceHandle<Engine::Texture> baseTexture;
		bool isOverridden = false;
		bool hasBase = false;
	};

	std::vector<BindingRow> rows;

	// 2-1. 베이스 머티리얼의 슬롯 먼저 수집
	if (baseMat)
	{
		for (const auto& b : baseMat->GetTextureBindings())
			rows.push_back({ b.slot, b.name, b.texture, b.texture, false, true });
	}

	// 2-2. 인스턴스(또는 일반 머티리얼) 본인의 바인딩 병합
	const auto& currentBindings = m_Material->GetTextureBindings();
	for (const auto& b : currentBindings)
	{
		auto it = std::find_if(rows.begin(), rows.end(), [&](const BindingRow& r) { return r.slot == b.slot; });
		if (it != rows.end())
		{
			it->name = b.name;
			it->texture = b.texture;
			it->isOverridden = true;
		}
		else
		{
			rows.push_back({ b.slot, b.name, b.texture, {}, true, false });
		}
	}

	// 2-3. 셰이더 요구 슬롯 중 아직 없는 슬롯 추가 (Placeholder)
	for (const auto& exp : expectedSlots)
	{
		auto it = std::find_if(rows.begin(), rows.end(), [&](const BindingRow& r) { return r.slot == exp.slot; });
		if (it == rows.end())
			rows.push_back({ exp.slot, exp.name, {}, {}, false, false });
	}

	// 2-4. 슬롯 번호 순서(t0, t1, t2...)로 정렬
	std::sort(rows.begin(), rows.end(), [](const BindingRow& a, const BindingRow& b) { return a.slot < b.slot; });

	int slotToRemove = -1;

	// 3. 텍스처 바인딩 테이블 렌더링
	if (ImGui::BeginTable("TextureBindingsTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.Slot"), ImGuiTableColumnFlags_WidthFixed, 80.0f);
		ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.Name"), ImGuiTableColumnFlags_WidthFixed, 130.0f);
		ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.Preview"), ImGuiTableColumnFlags_WidthFixed, 75.0f);
		ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.TextureDragDrop"), ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("##Action", ImGuiTableColumnFlags_WidthFixed, EditorUI::GetIconButtonSize(ICON_FA_TRASH, true).x);
		ImGui::TableHeadersRow();

		for (size_t i = 0; i < rows.size(); ++i)
		{
			const auto& row = rows[i];
			ImGui::PushID(static_cast<int>(i));
			ImGui::TableNextRow();

			// Column 0: Slot 번호 및 +- 버튼 (베이스가 없는 독립 슬롯일 때만 +- 지원)
			ImGui::TableSetColumnIndex(0);
			if (row.isOverridden || row.hasBase)
			{
				ImGui::Text(LOCAL_CSTR("MaterialEditor.TextureRegister"), row.slot);
				if (!baseMat && row.isOverridden)
				{
					ImGui::SameLine();
					if (row.slot > 0 && ImGui::SmallButton("-"))
					{
						m_Material->SetTextureBinding(row.name, row.slot - 1, row.texture);
						m_History.MarkEdited();
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("+"))
					{
						m_Material->SetTextureBinding(row.name, row.slot + 1, row.texture);
						m_History.MarkEdited();
					}
				}
			}
			else
			{
				ImGui::TextDisabled(LOCAL_CSTR("MaterialEditor.TextureRegister"), row.slot);
			}

			// Column 1: Name (상태 표시)
			ImGui::TableSetColumnIndex(1);
			if (baseMat)
			{
				if (row.isOverridden)
				{
					ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "%s*", row.name.c_str());
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("MaterialEditor.OverriddenInInstance"));
				}
				else if (row.hasBase)
				{
					ImGui::TextDisabled("%s", LOCAL_FMT("MaterialEditor.BaseBinding", row.name).c_str());
					if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("MaterialEditor.InheritedFromBase"));
				}
				else
				{
					ImGui::TextDisabled("%s", row.name.c_str());
				}
			}
			else
			{
				if (row.isOverridden) ImGui::TextUnformatted(row.name.c_str());
				else ImGui::TextDisabled("%s", row.name.c_str());
			}

			// Column 2: Preview
			ImGui::TableSetColumnIndex(2);
			ImTextureID texID = 0;
			if (row.texture.IsValid() && row.texture.Get() && row.texture.Get()->GetRHITexture())
				texID = ImGuiManager::Get().GetImGuiTextureID(row.texture.Get()->GetRHITexture());

			if (texID) ImGui::Image(texID, ImVec2(28.0f, 28.0f));
			else ImGui::Dummy(ImVec2(28.0f, 28.0f));

			// Column 3: Texture Resource Picker Slot (클릭 시 피커 팝업 + 드래그 앤 드롭 동시 지원)
			ImGui::TableSetColumnIndex(3);
			auto texHandle = row.texture;
			if (Editor::DrawInspectorResourcePicker(texHandle, !m_IsReadOnly, false) && !m_IsReadOnly)
			{
				m_Material->SetTextureBinding(row.name, row.slot, texHandle);
				m_History.MarkEdited();
			}

			// Column 4: Action (인스턴스 오버라이드 해제 또는 삭제)
			ImGui::TableSetColumnIndex(4);
			if (baseMat && row.isOverridden && row.hasBase)
			{
				// 부모 상속 슬롯을 오버라이드한 경우 -> 되돌리기 버튼 (Revert to Base)
				if (EditorUI::SmallIconButton("RevertOverride", ICON_FA_ROTATE_LEFT, "EditorCommon.Reset"))
					slotToRemove = static_cast<int>(row.slot);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", LOCAL_CSTR("MaterialEditor.Revert"));
			}
			else if (row.isOverridden)
			{
				// 일반 머티리얼의 등록 슬롯이거나 인스턴스의 추가 커스텀 슬롯 -> 삭제 버튼
				if (EditorUI::SmallIconButton("RemoveBinding", ICON_FA_TRASH, "EditorCommon.Delete"))
					slotToRemove = static_cast<int>(row.slot);
			}
			else
			{
				ImGui::Dummy(ImVec2(20.0f, 0.0f));
			}

			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	if (slotToRemove >= 0)
	{
		m_Material->RemoveTextureBindingBySlot(static_cast<uint32>(slotToRemove));
		m_History.MarkEdited();
	}

	// 4. 새 슬롯 원클릭 추가 (다음 빈 슬롯 자동 계산)
	uint32 nextAvailableSlot = 0;
	for (const auto& r : rows)
		if (r.slot >= nextAvailableSlot) nextAvailableSlot = r.slot + 1;

	ImGui::Spacing();
	ImGui::PushID("NewBindingControl");
	const std::string addBtnText = ICON_FA_PLUS " " +
		LOCAL_FMT("MaterialEditor.AddBindingAtSlot", LOCAL("MaterialEditor.AddBinding"), nextAvailableSlot) + "###AddTextureBinding";
	if (ImGui::Button(addBtnText.c_str()))
	{
		std::string finalName = "Texture_" + std::to_string(nextAvailableSlot);
		m_Material->SetTextureBinding(finalName, nextAvailableSlot, {});
		m_History.MarkEdited();
	}
	ImGui::PopID();

	ImGui::EndDisabled();
}

void MaterialEditor::DrawParameters()
{
	// CollapsingHeader를 복원하여 타 섹션과의 접기/펼치기 일관성 유지
	if (!ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.Parameters"), ImGuiTreeNodeFlags_DefaultOpen))
		return;
	if (!m_Material) return;
	ImGui::BeginDisabled(m_IsReadOnly);

	const auto& params = m_Material->GetParameters();
	// 1. 표준 검색 바 (EditorUI::DrawSearchBar) 도입
	EditorUI::DrawSearchBar("MaterialParamSearch", m_ParameterSearch, sizeof(m_ParameterSearch), "MaterialEditor.SearchParameters");
	ImGui::Spacing();
	const std::string filterLower = Engine::ToLowerStr(m_ParameterSearch);
	std::string paramToRemove;
	if (ImGui::BeginTable("ParamsTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.Name"), ImGuiTableColumnFlags_WidthStretch, 0.4f);
		ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.Value"), ImGuiTableColumnFlags_WidthStretch, 0.5f);
		ImGui::TableSetupColumn("##Action", ImGuiTableColumnFlags_WidthFixed, 32.0f);
		ImGui::TableHeadersRow();
		for (const auto& [name, param] : params)
		{
			// 검색어 필터링
			if (!filterLower.empty() && Engine::ToLowerStr(name).find(filterLower) == std::string::npos)
				continue;
			ImGui::PushID(name.c_str());
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(name.c_str());
			ImGui::TableSetColumnIndex(1);
			ImGui::SetNextItemWidth(-FLT_MIN);
			switch (param.type)
			{
			case Engine::EMaterialParameterType::Float:
			{
				float val = param.data.size() >= sizeof(float) ? *reinterpret_cast<const float*>(param.data.data()) : 0.0f;
				if (ImGui::DragFloat("##val", &val, 0.01f))
				{
					m_Material->SetFloat(name, val);
					m_History.MarkEdited();
				}
				break;
			}
			case Engine::EMaterialParameterType::Float2:
			{
				vec2 val = param.data.size() >= sizeof(vec2) ? *reinterpret_cast<const vec2*>(param.data.data()) : vec2(0.0f, 0.0f);
				if (ImGui::DragFloat2("##val", &val.x, 0.01f))
				{
					m_Material->SetFloat2(name, val);
					m_History.MarkEdited();
				}
				break;
			}
			case Engine::EMaterialParameterType::Float3:
			{
				vec3 val = param.data.size() >= sizeof(vec3) ? *reinterpret_cast<const vec3*>(param.data.data()) : vec3(0.0f, 0.0f, 0.0f);
				if (ImGui::ColorEdit3("##val", &val.x))
				{
					m_Material->SetFloat3(name, val);
					m_History.MarkEdited();
				}
				break;
			}
			case Engine::EMaterialParameterType::Float4:
			{
				vec4 val = param.data.size() >= sizeof(vec4) ? *reinterpret_cast<const vec4*>(param.data.data()) : vec4(0.0f, 0.0f, 0.0f, 0.0f);
				if (ImGui::ColorEdit4("##val", &val.x))
				{
					m_Material->SetFloat4(name, val);
					m_History.MarkEdited();
				}
				break;
			}
			case Engine::EMaterialParameterType::Int:
			{
				int val = param.data.size() >= sizeof(int) ? *reinterpret_cast<const int*>(param.data.data()) : 0;
				if (ImGui::DragInt("##val", &val))
				{
					m_Material->SetInt(name, val);
					m_History.MarkEdited();
				}
				break;
			}
			case Engine::EMaterialParameterType::Bool:
			{
				bool val = param.data.size() >= sizeof(bool) ? *reinterpret_cast<const bool*>(param.data.data()) : false;
				if (ImGui::Checkbox("##val", &val))
				{
					m_Material->SetBool(name, val);
					m_History.MarkEdited();
				}
				break;
			}
			default:
				ImGui::TextDisabled("%s", LOCAL_CSTR("MaterialEditor.UnsupportedType"));
				break;
			}
			ImGui::TableSetColumnIndex(2);
			if (EditorUI::SmallIconButton("DelParam", ICON_FA_TRASH, "EditorCommon.Delete"))
				paramToRemove = name;
			ImGui::PopID();
		}
		ImGui::EndTable();
	}
	if (!paramToRemove.empty())
	{
		m_Material->RemoveParameter(paramToRemove);
		m_History.MarkEdited();
	}
	// 2. 새 파라미터 추가 바 반응형 배치 (도킹 폭 260px 잘림 방지)
	ImGui::Spacing();
	ImGui::PushID("NewParamControl");
	const float availWidth = ImGui::GetContentRegionAvail().x;
	const float nameWidth = (availWidth > 320.0f) ? 140.0f : (availWidth * 0.45f);
	const float typeWidth = (availWidth > 320.0f) ? 140.0f : (availWidth * 0.45f);
	ImGui::SetNextItemWidth(nameWidth);
	ImGui::InputTextWithHint("##ParamName", LOCAL_CSTR("MaterialEditor.ParamNameHint"), m_NewParamName, sizeof(m_NewParamName));
	if (availWidth > 320.0f)
		ImGui::SameLine();
	const string colorFloat3 = LOCAL_FMT("MaterialEditor.ColorType", "Float3");
	const string colorFloat4 = LOCAL_FMT("MaterialEditor.ColorType", "Float4");
	const char* typeNames[] = { "Float", "Float2", colorFloat3.c_str(), colorFloat4.c_str(), "Int", "Bool" };
	ImGui::SetNextItemWidth(typeWidth);
	ImGui::Combo("##ParamType", &m_NewParamTypeIndex, typeNames, IM_ARRAYSIZE(typeNames));
	if (availWidth > 400.0f)
		ImGui::SameLine();
	const bool canAdd = (m_NewParamName[0] != '\0' && !m_Material->HasParameter(m_NewParamName));
	ImGui::BeginDisabled(!canAdd);
	if (ImGui::Button((ICON_FA_PLUS " " + LOCAL("MaterialEditor.AddParam")).c_str()))
	{
		switch (m_NewParamTypeIndex)
		{
		case 0: m_Material->SetFloat(m_NewParamName, 0.0f); break;
		case 1: m_Material->SetFloat2(m_NewParamName, vec2(0.0f, 0.0f)); break;
		case 2: m_Material->SetFloat3(m_NewParamName, vec3(1.0f, 1.0f, 1.0f)); break;
		case 3: m_Material->SetFloat4(m_NewParamName, vec4(1.0f, 1.0f, 1.0f, 1.0f)); break;
		case 4: m_Material->SetInt(m_NewParamName, 0); break;
		case 5: m_Material->SetBool(m_NewParamName, false); break;
		}
		m_History.MarkEdited();
		m_NewParamName[0] = '\0';
	}
	ImGui::EndDisabled();
	ImGui::PopID();
	ImGui::EndDisabled();
}

void MaterialEditor::CaptureState(EditorWorkspace& workspace) const
{
	IImGui::CaptureState(workspace);
	workspace.SetSplitter("Material.ViewportWidth", m_ViewportWidth);
}

void MaterialEditor::RestoreState(const EditorWorkspace& workspace)
{
	IImGui::RestoreState(workspace);
	m_ViewportWidth = workspace.GetSplitter("Material.ViewportWidth", 480.0f);
}
