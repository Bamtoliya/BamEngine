#include "MaterialEditor.h"
#include "Material.h"
#include "MaterialInstance.h"
#include "ResourceManager.h"
#include "ImGuiManager.h"
#include "Logger.h"
#include <algorithm>

namespace
{
	template<size_t N>
	bool LocalizedCombo(const char* label, int* selected, const char* (&keys)[N])
	{
		std::array<std::string, N> labels;
		std::array<const char*, N> items;
		for (size_t i = 0; i < N; ++i) labels[i] = LOCAL(keys[i]);
		for (size_t i = 0; i < N; ++i) items[i] = labels[i].c_str();
		return ImGui::Combo(label, selected, items.data(), static_cast<int>(N));
	}

	std::string ToLowerStr(std::string str)
	{
		for (char& c : str) c = static_cast<char>(tolower(c));
		return str;
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
	const std::string name = ToLowerStr(assetPath.filename().string());
	return name.ends_with(".bammat.json") || name.ends_with(".bammatinst.json") ||
		name.ends_with(".bammat") || name.ends_with(".bammatinst");
}

void MaterialEditor::SetTargetResource(const std::filesystem::path& assetPath)
{
	m_TargetResourcePath = assetPath;
	m_Material = nullptr;
	m_MaterialHandle = {};
	m_IsDirty = false;

	const std::wstring relPath = std::filesystem::relative(assetPath).wstring();
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
	// Renderer::Render()가 실행되기 전에 미리 프리뷰 드로우 커맨드와 카메라를 제출
	if (m_Open && m_Material)
	{
		m_PreviewRenderer.Submit(m_Material, dt);
	}
}

// MaterialEditor.cpp -> MaterialEditor::Draw() 전체 교체
void MaterialEditor::Draw()
{
	if (!m_Open) return;

	const std::string title = GetDisplayName() + " - " + m_TargetResourcePath.filename().string() + "###" + WStrToStr(m_Name);
	ImGui::SetNextWindowSize(ImVec2(1000.0f, 650.0f), ImGuiCond_FirstUseEver);

	if (ImGui::Begin(title.c_str(), &m_Open))
	{
		if (!m_Material)
		{
			ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", LOCAL_FMT("MaterialEditor.LoadFailed", m_TargetResourcePath.string()).c_str());
			if (ImGui::Button(LOCAL_CSTR("MaterialEditor.Retry")))
				SetTargetResource(m_TargetResourcePath);
			ImGui::End();
			return;
		}

		// 상단 메인 툴바 (저장 버튼 등)
		DrawToolbar();
		ImGui::Separator();

		// 좌우 2분할 레이아웃 계산 (좌측 뷰포트 55%, 우측 디테일 패널 45%)
		const float availWidth = ImGui::GetContentRegionAvail().x;
		const float viewportWidth = std::max(availWidth * 0.55f, 320.0f);

		// ── [좌측 영역] 3D 머티리얼 뷰포트 (자체 마우스 조작, 부모 스크롤 차단) ──
		ImGuiWindowFlags vpFlags = ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
		if (ImGui::BeginChild("##MaterialViewportPane", ImVec2(viewportWidth, 0.0f), true, vpFlags))
		{
			m_PreviewRenderer.DrawUI(m_Material);
		}
		ImGui::EndChild();

		ImGui::SameLine();

		// ── [우측 영역] 머티리얼 프로퍼티 & 파라미터 디테일 패널 (독립 세로 스크롤) ──
		if (ImGui::BeginChild("##MaterialDetailsPane", ImVec2(0.0f, 0.0f), true))
		{
			DrawPipelineSettings();
			DrawShaderSettings();
			DrawTextureBindings();
			DrawParameters();
		}
		ImGui::EndChild();
	}
	ImGui::End();
}

void MaterialEditor::DrawToolbar()
{
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 4.0f));

	if (ImGui::Button((ICON_FA_FLOPPY_DISK " " + LOCAL("MaterialEditor.Save")).c_str()))
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
			ENGINE_LOG_INFO("Material saved: {}", pathStr);
		}
		else
		{
			ENGINE_LOG_ERROR("Failed to save material: {}", pathStr);
		}
	}

	ImGui::SameLine();
	if (ImGui::Button((ICON_FA_ROTATE_LEFT " " + LOCAL("MaterialEditor.Revert")).c_str()))
	{
		if (m_Material->Reload(m_TargetResourcePath) == EResult::Success)
		{
			m_IsDirty = false;
			ENGINE_LOG_INFO("Material reverted: {}", m_TargetResourcePath.string());
		}
	}

	ImGui::SameLine();
	if (m_IsDirty)
	{
		ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.2f, 1.0f), "[%s]", LOCAL_CSTR("MaterialEditor.Modified"));
		ImGui::SameLine();
	}

	ImGui::TextDisabled("%s", m_TargetResourcePath.string().c_str());
	ImGui::PopStyleVar();
	ImGui::Separator();
}

void MaterialEditor::DrawPipelineSettings()
{
	if (!ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.PipelineSettings"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	auto* inst = dynamic_cast<Engine::MaterialInstance*>(m_Material);
	if (inst)
	{
		std::string baseName = GetResourceDisplayName(inst->GetBaseMaterial());
		ImGui::Text("%s", LOCAL_FMT("MaterialEditor.BaseMaterial", baseName).c_str());
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
	const char* fillModes[] = { "MaterialOption.Solid", "MaterialOption.Wireframe" };
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

	auto DrawShaderSlot = [&](const std::string& label, const Engine::Shader* currentShader, float btnWidth, auto onDrop)
		{
			std::string shaderName = currentShader ? GetResourceDisplayName(currentShader) : (btnWidth < 0.0f ? LOCAL("MaterialEditor.DropShader") : "(None)");
			if (!label.empty())
			{
				ImGui::Text("%s:", label.c_str());
				ImGui::SameLine();
			}
			ImGui::PushID(label.empty() ? "ShaderSlot" : label.c_str());
			ImGui::Button(shaderName.c_str(), ImVec2(btnWidth, 0.0f));
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					std::filesystem::path p((const wchar_t*)payload->Data);
					std::string ext = ToLowerStr(p.extension().string());
					if (ext == ".hlsl" || ext == ".bamshader")
					{
						auto handle = Engine::ResourceManager::Get().LoadFile(p.wstring());
						auto sHandle = Engine::ResourceManager::Get().GetResourceHandle<Engine::Shader>(p.wstring());
						if (!sHandle.IsValid() && handle.IsValid())
							sHandle = Engine::ResourceHandle<Engine::Shader>(handle);
						if (sHandle.IsValid())
						{
							onDrop(sHandle);
							m_IsDirty = true;
						}
					}
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::PopID();
		};

	// 1. 기본 셰이더 (Default Fallback)
	ImGui::TextDisabled("%s", LOCAL_CSTR("MaterialEditor.DefaultShaders"));
	DrawShaderSlot(LOCAL("MaterialEditor.DefaultVS"), m_Material->GetVertexShader(), 240.0f, [&](auto handle) { m_Material->SetVertexShaderHandle(handle); });
	DrawShaderSlot(LOCAL("MaterialEditor.DefaultPS"), m_Material->GetPixelShader(), 240.0f, [&](auto handle) { m_Material->SetPixelShaderHandle(handle); });

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

			// Column 1: Pass Vertex Shader
			ImGui::TableSetColumnIndex(1);
			DrawShaderSlot("", passShader.vertexShader.Get(), -1.0f, [&](auto handle) {
				m_Material->SetPassShader(passShader.passType, handle, passShader.pixelShader);
				});

			// Column 2: Pass Pixel Shader
			ImGui::TableSetColumnIndex(2);
			DrawShaderSlot("", passShader.pixelShader.Get(), -1.0f, [&](auto handle) {
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
			m_Material->SetPassShader(passType, {}, {});
			m_IsDirty = true;
		}
	}
	ImGui::PopID();
}

void MaterialEditor::DrawTextureBindings()
{
	if (!ImGui::CollapsingHeader(LOCAL_CSTR("MaterialEditor.TextureBindings"), ImGuiTreeNodeFlags_DefaultOpen))
		return;

	const auto& bindings = m_Material->GetTextureBindings();
	int slotToRemove = -1;

	if (ImGui::BeginTable("TextureBindingsTable", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
	{
		ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.Slot"), ImGuiTableColumnFlags_WidthFixed, 45.0f);
		ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.Name"), ImGuiTableColumnFlags_WidthFixed, 90.0f);
		ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.Preview"), ImGuiTableColumnFlags_WidthFixed, 36.0f);
		ImGui::TableSetupColumn(LOCAL_CSTR("MaterialEditor.TextureDragDrop"), ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("##Action", ImGuiTableColumnFlags_WidthFixed, 35.0f);
		ImGui::TableHeadersRow();

		for (size_t i = 0; i < bindings.size(); ++i)
		{
			const auto& binding = bindings[i];
			ImGui::PushID(static_cast<int>(i));
			ImGui::TableNextRow();

			// 0: Slot
			ImGui::TableSetColumnIndex(0);
			ImGui::Text("%u", binding.slot);

			// 1: Name
			ImGui::TableSetColumnIndex(1);
			ImGui::TextUnformatted(binding.name.c_str());

			// 2: Preview
			ImGui::TableSetColumnIndex(2);
			ImTextureID texID = 0;
			if (binding.texture.IsValid() && binding.texture.Get() && binding.texture.Get()->GetRHITexture())
				texID = ImGuiManager::Get().GetImGuiTextureID(binding.texture.Get()->GetRHITexture());

			if (texID)
				ImGui::Image(texID, ImVec2(28.0f, 28.0f));
			else
				ImGui::Dummy(ImVec2(28.0f, 28.0f));

			// 3: Texture Handle & DragDrop
			ImGui::TableSetColumnIndex(3);
			std::string texName = binding.texture.IsValid() ? GetResourceDisplayName(binding.texture.Get()) : LOCAL("MaterialEditor.DropTexture");
			ImGui::Button(texName.c_str(), ImVec2(-1.0f, 0.0f));
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM"))
				{
					std::filesystem::path p((const wchar_t*)payload->Data);
					std::string ext = ToLowerStr(p.extension().string());
					if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".dds" || ext == ".bamtex")
					{
						auto texHandle = Engine::ResourceManager::Get().GetResourceHandle<Engine::Texture>(p.wstring());
						if (!texHandle.IsValid())
						{
							auto handle = Engine::ResourceManager::Get().LoadFile(p.wstring());
							if (handle.IsValid())
								texHandle = Engine::ResourceHandle<Engine::Texture>(handle);
						}
						if (texHandle.IsValid())
						{
							m_Material->SetTextureBinding(binding.name, binding.slot, texHandle);
							m_IsDirty = true;
						}
					}
				}
				ImGui::EndDragDropTarget();
			}

			// 4: Delete button
			ImGui::TableSetColumnIndex(4);
			if (ImGui::Button(ICON_FA_TRASH "##RemoveBinding"))
				slotToRemove = static_cast<int>(binding.slot);

			ImGui::PopID();
		}
		ImGui::EndTable();
	}

	if (slotToRemove >= 0)
	{
		m_Material->RemoveTextureBindingBySlot(static_cast<uint32>(slotToRemove));
		m_IsDirty = true;
	}

	// 새 텍스처 바인딩 슬롯 추가
	ImGui::Spacing();
	ImGui::PushID("NewBindingControl");
	ImGui::SetNextItemWidth(50.0f);
	ImGui::InputInt("##SlotNum", &m_NewTextureSlotNumber);
	ImGui::SameLine();
	ImGui::SetNextItemWidth(120.0f);
	ImGui::InputTextWithHint("##SlotName", LOCAL_CSTR("MaterialEditor.SlotNameHint"), m_NewTextureSlotName, sizeof(m_NewTextureSlotName));
	ImGui::SameLine();
	if (ImGui::Button((ICON_FA_PLUS " " + LOCAL("MaterialEditor.AddBinding")).c_str()))
	{
		if (m_NewTextureSlotNumber >= 0 && m_NewTextureSlotName[0] != '\0')
		{
			m_Material->SetTextureBinding(m_NewTextureSlotName, static_cast<uint32>(m_NewTextureSlotNumber), {});
			m_IsDirty = true;
			m_NewTextureSlotNumber++;
			m_NewTextureSlotName[0] = '\0';
		}
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
	const char* typeNames[] = { "Float", "Float2", "Float3 (Color)", "Float4 (Color)", "Int", "Bool" };
	ImGui::SetNextItemWidth(130.0f);
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
