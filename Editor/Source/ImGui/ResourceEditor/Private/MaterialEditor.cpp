#include "MaterialEditor.h"
#include "ShortcutManager.h"
#include "Material.h"
#include "MaterialInstance.h"
#include "ResourceManager.h"
#include "PlatformPath.h"
#include "Functions.h"
#include "ImGuiManager.h"
#include "Logger.h"
#include "LocalizationManager.h"
#include "ResourcePicker.h"
#include "AssetCache.h"

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
	m_TargetResourcePath = assetPath;
	m_Material = nullptr;
	m_MaterialHandle = {};
	m_IsDirty = false;

	const std::wstring relPath = Engine::PlatformPath::ToProjectRelative(assetPath).wstring();
	auto& rm = Engine::ResourceManager::Get();

	m_MaterialHandle = rm.GetResourceHandle<Engine::MaterialInterface>(relPath);
	if (!m_MaterialHandle.IsValid())
	{
		auto handle = rm.LoadFile(relPath);
		if (handle.IsValid())
			m_MaterialHandle = Engine::ResourceHandle<Engine::MaterialInterface>(handle);
	}

	m_Material = m_MaterialHandle.Get();
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

void MaterialEditor::Draw()
{
	if (!m_Open)
	{
		m_IsVisibleOnScreen = false;
		return;
	}
	const auto title = BuildResourceWindowTitle();
	ImGui::SetNextWindowSize(ImVec2(1000.0f, 650.0f), ImGuiCond_FirstUseEver);
	// 가려진 탭이면 Begin이 false를 반환합니다.
	if (BeginPanelWindow(title.c_str(), m_Open, ImGuiWindowFlags_MenuBar))
	{
		m_IsVisibleOnScreen = true;
		// 마우스가 에디터 위에 있거나 포커스된 경우 프리뷰 갱신 타이머 연장 (0.5초 동안 활성 렌더링)
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows) || ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
		{
			m_PreviewDirtyTimer = 0.5f;
		}
		if (!m_Material)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", LOCAL_FMT("MaterialEditor.LoadFailed", m_TargetResourcePath.string()).c_str());
			if (ImGui::Button(LOCAL_CSTR("MaterialEditor.Retry")))
				SetTargetResource(m_TargetResourcePath);
			ImGui::End();
			return;
		}

		DrawMenuBar();
		DrawToolbar();
		ImGui::Separator();
		const float availWidth = ImGui::GetContentRegionAvail().x;
		const float splitterThickness = 4.0f;
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
	}
	else
	{
		m_IsVisibleOnScreen = false;
	}
	ImGui::End();
}

void MaterialEditor::DrawMenuBar()
{
	if (!ImGui::BeginMenuBar()) return;

	if (ImGui::BeginMenu(LOCAL_LABEL("EditorMenu.File").c_str()))
	{
		const bool canSave = m_Material != nullptr;
		ImGui::BeginDisabled(!canSave);
		auto& sm = ShortcutManager::Get();
		if (ImGui::MenuItem((ICON_FA_FLOPPY_DISK " " + LOCAL_LABEL("EditorCommon.Save")).c_str(), sm.GetShortcutHint("Animator.Save")))
		{
			const std::string pathStr = m_TargetResourcePath.string();
			EResult result = EResult::Fail;
			if (pathStr.ends_with(".json"))
				result = Engine::ResourceManager::Get().SaveToJsonFile(m_Material, m_TargetResourcePath.wstring());
			else
				result = Engine::ResourceManager::Get().SaveToBinaryFile(m_Material, m_TargetResourcePath.wstring());

			if (result == EResult::Success)
			{
				m_IsDirty = false;
				if (AssetCache::IsCreated())
					AssetCache::Get().RequestInvalidate(m_TargetResourcePath);
				EDITOR_LOG_INFO("Material saved: {}", pathStr);
			}
			else
			{
				EDITOR_LOG_ERROR("Failed to save material: {}", pathStr);
			}
		}

		if (ImGui::MenuItem((ICON_FA_ROTATE_LEFT " " + LOCAL_LABEL("MaterialEditor.Revert")).c_str()))
		{
			if (m_Material->Reload(m_TargetResourcePath) == EResult::Success)
			{
				m_IsDirty = false;
				if (AssetCache::IsCreated())
					AssetCache::Get().RequestInvalidate(m_TargetResourcePath);
				EDITOR_LOG_INFO("Material reverted: {}", m_TargetResourcePath.string());
			}
		}
		ImGui::EndDisabled();

		ImGui::EndMenu();
	}

	if (ImGui::BeginMenu(LOCAL_LABEL("MaterialEditor.ViewMenu").c_str()))
	{
		auto& sm = ShortcutManager::Get();
		if (ImGui::MenuItem((ICON_FA_CROSSHAIRS " " + LOCAL_LABEL("MaterialEditor.ResetCamera")).c_str(), sm.GetShortcutHint("Viewport.Focus")))
		{
			m_PreviewRenderer.GetWidget().FocusTarget(Engine::vec3(0.0f, 0.0f, 0.0f), 3.0f);
		}

		if (ImGui::MenuItem((ICON_FA_BORDER_ALL " " + LOCAL_LABEL("MaterialEditor.ToggleGrid")).c_str(), sm.GetShortcutHint("Viewport.ToggleGrid"), m_PreviewRenderer.GetWidget().IsGridVisible()))
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

	ImGui::EndMenuBar();
}

void MaterialEditor::DrawToolbar()
{
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));

	if (ImGui::Button(ICON_FA_FLOPPY_DISK "##SaveMaterial"))
	{
		const std::string pathStr = m_TargetResourcePath.string();
		EResult result = EResult::Fail;
		if (pathStr.ends_with(".json"))
			result = Engine::ResourceManager::Get().SaveToJsonFile(m_Material, m_TargetResourcePath.wstring());
		else
			result = Engine::ResourceManager::Get().SaveToBinaryFile(m_Material, m_TargetResourcePath.wstring());

		if (result == EResult::Success)
		{
			m_IsDirty = false;
			if (AssetCache::IsCreated())
				AssetCache::Get().RequestInvalidate(m_TargetResourcePath);
			EDITOR_LOG_INFO("Material saved: {}", pathStr);
		}
		else
		{
			EDITOR_LOG_ERROR("Failed to save material: {}", pathStr);
		}
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("EditorCommon.Save"));

	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_ROTATE_LEFT "##RevertMaterial"))
	{
		if (m_Material && m_Material->Reload(m_TargetResourcePath) == EResult::Success)
		{
			m_IsDirty = false;
			if (AssetCache::IsCreated())
				AssetCache::Get().RequestInvalidate(m_TargetResourcePath);
			EDITOR_LOG_INFO("Material reverted: {}", m_TargetResourcePath.string());
		}
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("MaterialEditor.Revert"));

	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	if (ImGui::Button(ICON_FA_CROSSHAIRS "##FocusMaterial"))
	{
		m_PreviewRenderer.GetWidget().FocusTarget(Engine::vec3(0.0f, 0.0f, 0.0f), 3.0f);
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("MaterialEditor.ResetCamera"));

	ImGui::SameLine();
	if (ImGui::Button(ICON_FA_BORDER_ALL "##ToggleGridMaterial"))
	{
		m_PreviewRenderer.GetWidget().ToggleGrid();
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", LOCAL_CSTR("MaterialEditor.ToggleGrid"));

	ImGui::SameLine();
	ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
	ImGui::SameLine();

	// 파일 상태 배지
	const std::string fileName = m_TargetResourcePath.empty()
		? LOCAL("AnimatorGraph.Unsaved") : m_TargetResourcePath.filename().string();
	ImGui::TextDisabled("[%s%s]", fileName.c_str(), m_IsDirty ? " *" : "");

	ImGui::PopStyleVar();
}

void MaterialEditor::DrawPipelineSettings()
{
	if (!ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.PipelineSettings"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

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
		if (Editor::DrawInspectorResourcePicker(baseHandle, true))
		{
			inst->SetBaseMaterial(baseHandle);
			m_IsDirty = true;
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
		m_IsDirty = true;
	}

	// Cull Mode
	int cullIdx = static_cast<int>(m_Material->GetCullMode());
	const char* cullModes[] = { "MaterialOption.None", "MaterialOption.Front", "MaterialOption.Back" };
	if (LocalizedCombo(LOCAL_LABEL("MaterialEditor.CullMode").c_str(), &cullIdx, cullModes))
	{
		m_Material->SetCullMode(static_cast<Engine::ECullMode>(cullIdx));
		m_IsDirty = true;
	}

	// Fill Mode
	int fillIdx = static_cast<int>(m_Material->GetFillMode());
	const char* fillModes[] = { "MaterialOption.Wireframe", "MaterialOption.Solid" };
	if (LocalizedCombo(LOCAL_LABEL("MaterialEditor.FillMode").c_str(), &fillIdx, fillModes))
	{
		m_Material->SetFillMode(static_cast<Engine::EFillMode>(fillIdx));
		m_IsDirty = true;
	}

	// Depth Mode
	int depthIdx = static_cast<int>(m_Material->GetDepthMode());
	const char* depthModes[] = { "MaterialOption.None", "MaterialOption.ReadOnly", "MaterialOption.ReadWrite" };
	if (LocalizedCombo(LOCAL_LABEL("MaterialEditor.DepthMode").c_str(), &depthIdx, depthModes))
	{
		m_Material->SetDepthMode(static_cast<Engine::EDepthMode>(depthIdx));
		m_IsDirty = true;
	}

	// Depth Compare Op
	int opIdx = static_cast<int>(m_Material->GetDepthCompareOp());
	const char* compareOps[] = { "MaterialOption.Never", "MaterialOption.Less", "MaterialOption.Equal", "MaterialOption.LessEqual", "MaterialOption.Greater", "MaterialOption.NotEqual", "MaterialOption.GreaterEqual", "MaterialOption.Always" };
	if (LocalizedCombo(LOCAL_LABEL("MaterialEditor.DepthCompareOp").c_str(), &opIdx, compareOps))
	{
		m_Material->SetDepthCompareOp(static_cast<Engine::ECompareOp>(opIdx));
		m_IsDirty = true;
	}
}

void MaterialEditor::DrawShaderSettings()
{
	if (!ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.ShaderSettings"), ImGuiTreeNodeFlags_DefaultOpen))
		return;
	auto DrawShaderSlot = [&](const std::string& slotId, const std::string& label, Engine::ResourceHandle<Engine::Shader> currentShader, float btnWidth, auto onAssign)
		{
			if (!label.empty())
			{
				ImGui::Text("%s:", label.c_str());
				ImGui::SameLine();
			}
			ImGui::PushID(slotId.c_str());
			if (btnWidth > 0.0f) ImGui::SetNextItemWidth(btnWidth);
			if (Editor::DrawInspectorResourcePicker(currentShader, true))
			{
				onAssign(currentShader);
				m_IsDirty = true;
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
		ImGui::TableSetupColumn("##Action", ImGuiTableColumnFlags_WidthFixed, 35.0f);
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
			if (ImGui::Button(ICON_FA_TRASH "##RemovePass"))
				passToRemove = typeIdx;

			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	if (passToRemove >= 0)
	{
		m_Material->RemovePassShader(static_cast<Engine::ERenderPassType>(passToRemove));
		m_IsDirty = true;
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
			m_IsDirty = true;
		}
	}
	ImGui::PopID();
}

void MaterialEditor::DrawTextureBindings()
{
	if (!ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.TextureBindings"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

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
		ImGui::TableSetupColumn("##Action", ImGuiTableColumnFlags_WidthFixed, 35.0f);
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
						m_IsDirty = true;
					}
					ImGui::SameLine();
					if (ImGui::SmallButton("+"))
					{
						m_Material->SetTextureBinding(row.name, row.slot + 1, row.texture);
						m_IsDirty = true;
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
			if (Editor::DrawInspectorResourcePicker(texHandle, true, false))
			{
				m_Material->SetTextureBinding(row.name, row.slot, texHandle);
				m_IsDirty = true;
			}

			// Column 4: Action (인스턴스 오버라이드 해제 또는 삭제)
			ImGui::TableSetColumnIndex(4);
			if (baseMat && row.isOverridden && row.hasBase)
			{
				// 부모 상속 슬롯을 오버라이드한 경우 -> 되돌리기 버튼 (Revert to Base)
				if (ImGui::Button(ICON_FA_ROTATE_LEFT "##RevertOverride"))
					slotToRemove = static_cast<int>(row.slot);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip("%s", LOCAL_CSTR("MaterialEditor.Revert"));
			}
			else if (row.isOverridden)
			{
				// 일반 머티리얼의 등록 슬롯이거나 인스턴스의 추가 커스텀 슬롯 -> 삭제 버튼
				if (ImGui::Button(ICON_FA_TRASH "##RemoveBinding"))
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
		m_IsDirty = true;
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
		m_IsDirty = true;
	}
	ImGui::PopID();
}

void MaterialEditor::DrawParameters()
{
	if (!ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.Parameters"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	const auto& params = m_Material->GetParameters();
	std::string paramToRemove = "";

	if (ImGui::BeginTable("ParametersTable", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.Name"), ImGuiTableColumnFlags_WidthFixed, 140.0f);
		ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.Value"), ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("##Action", ImGuiTableColumnFlags_WidthFixed, 35.0f);
		ImGui::TableHeadersRow();

		for (const auto& [name, param] : params)
		{
			ImGui::PushID(name.c_str());
			ImGui::TableNextRow();

			ImGui::TableSetColumnIndex(0);
			ImGui::TextUnformatted(name.c_str());

			ImGui::TableSetColumnIndex(1);
			switch (param.type)
			{
			case Engine::EMaterialParameterType::Float:
			{
				float val = param.data.size() >= sizeof(float) ? *reinterpret_cast<const float*>(param.data.data()) : 0.0f;
				if (ImGui::DragFloat("##val", &val, 0.01f))
				{
					m_Material->SetFloat(name, val);
					m_IsDirty = true;
				}
				break;
			}
			case Engine::EMaterialParameterType::Float2:
			{
				vec2 val = param.data.size() >= sizeof(vec2) ? *reinterpret_cast<const vec2*>(param.data.data()) : vec2(0.0f, 0.0f);
				if (ImGui::DragFloat2("##val", &val.x, 0.01f))
				{
					m_Material->SetFloat2(name, val);
					m_IsDirty = true;
				}
				break;
			}
			case Engine::EMaterialParameterType::Float3:
			{
				vec3 val = param.data.size() >= sizeof(vec3) ? *reinterpret_cast<const vec3*>(param.data.data()) : vec3(0.0f, 0.0f, 0.0f);
				if (ImGui::ColorEdit3("##val", &val.x))
				{
					m_Material->SetFloat3(name, val);
					m_IsDirty = true;
				}
				break;
			}
			case Engine::EMaterialParameterType::Float4:
			{
				vec4 val = param.data.size() >= sizeof(vec4) ? *reinterpret_cast<const vec4*>(param.data.data()) : vec4(0.0f, 0.0f, 0.0f, 0.0f);
				if (ImGui::ColorEdit4("##val", &val.x))
				{
					m_Material->SetFloat4(name, val);
					m_IsDirty = true;
				}
				break;
			}
			case Engine::EMaterialParameterType::Int:
			{
				int val = param.data.size() >= sizeof(int) ? *reinterpret_cast<const int*>(param.data.data()) : 0;
				if (ImGui::DragInt("##val", &val))
				{
					m_Material->SetInt(name, val);
					m_IsDirty = true;
				}
				break;
			}
			case Engine::EMaterialParameterType::Bool:
			{
				bool val = param.data.size() >= sizeof(bool) ? *reinterpret_cast<const bool*>(param.data.data()) : false;
				if (ImGui::Checkbox("##val", &val))
				{
					m_Material->SetBool(name, val);
					m_IsDirty = true;
				}
				break;
			}
			default:
				ImGui::TextDisabled("%s", LOCAL_CSTR("MaterialEditor.UnsupportedType"));
				break;
			}

			ImGui::TableSetColumnIndex(2);
			if (ImGui::Button(ICON_FA_TRASH "##DelParam"))
				paramToRemove = name;

			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	if (!paramToRemove.empty())
	{
		m_Material->RemoveParameter(paramToRemove);
		m_IsDirty = true;
	}

	// 새 파라미터 추가
	ImGui::Spacing();
	ImGui::PushID("NewParamControl");
	ImGui::SetNextItemWidth(140.0f);
	ImGui::InputTextWithHint("##ParamName", LOCAL_CSTR("MaterialEditor.ParamNameHint"), m_NewParamName, sizeof(m_NewParamName));
	ImGui::SameLine();
	const string colorFloat3 = LOCAL_FMT("MaterialEditor.ColorType", "Float3");
	const string colorFloat4 = LOCAL_FMT("MaterialEditor.ColorType", "Float4");
	const char* typeNames[] = { "Float", "Float2", colorFloat3.c_str(), colorFloat4.c_str(), "Int", "Bool" };
	ImGui::SetNextItemWidth(155.0f);
	ImGui::Combo("##ParamType", &m_NewParamTypeIndex, typeNames, IM_ARRAYSIZE(typeNames));
	ImGui::SameLine();
	if (ImGui::Button((ICON_FA_PLUS " " + LOCAL("MaterialEditor.AddParam")).c_str()))
	{
		if (m_NewParamName[0] != '\0' && !m_Material->HasParameter(m_NewParamName))
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
			m_IsDirty = true;
			m_NewParamName[0] = '\0';
		}
	}
	ImGui::PopID();
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