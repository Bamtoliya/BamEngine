#pragma once

#include "BaseViewportPanel.h"
#include "RenderTargetManager.h"
#include "CameraManager.h"
#include "SelectionManager.h"
#include "InputManager.h"
#include "ImViewGuizmo.h"

#pragma region Contructor&Destructor
void BaseViewportPanel::Initialize(void* arg)
{
	if (!arg) return;
	CAST_DESC
	m_Name = desc->Name;

#pragma region Prepare Camera
	tagCameraDesc cameraDesc;
	if (desc->CameraType == EViewportCameraType::Orthographic)
	{
		cameraDesc.IsPerspective = false;
		cameraDesc.OrthoSize = desc->RenderTargetHeight;
	}
	m_EditorCamera = EditorCamera::Create(&cameraDesc);
	m_EditorCamera->SetName(m_Name + L"_Camera");

	//Camera Inspector
	m_InspectorPanel = new InspectorPanel();
	m_InspectorPanel->Close();
	m_InspectorPanel->SetSelectedGameObject(m_EditorCamera);
#pragma endregion

#pragma region Prepare RenderPass And Pipelines
	PreparePipelines();
	PrepareRenderTargetsAndPasses(desc->RenderTargetWidth, desc->RenderTargetHeight);
#pragma endregion
	m_Grid.Initialize(m_Name);
	m_SelectedRTName = m_LightingColorName;

#ifdef _DEBUG
	m_DebugRenderer.Initialize(m_Name);
#endif // _DEBUG

}
void BaseViewportPanel::PrepareRenderTargetsAndPasses(uint32 width, uint32 height)
{
	wstring prefix = m_Name + L"_";

#pragma region RenderTargets
	auto& rtMgr = RenderTargetManager::Get();

	tagRenderTargetDesc gbufferRTDescs[] =
	{
		//GBuffers
		{ETextureFormat::R8G8B8A8_UNORM,		ETextureUsage::RenderTarget | ETextureUsage::Sampler,		ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::GBuffer,		ETextureDimension::Texture2D, width, height, vec4(0.f), L"GBuffer_Diffuse"},
		{ETextureFormat::R16G16B16A16_FLOAT,	ETextureUsage::RenderTarget | ETextureUsage::Sampler,		ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::GBuffer,		ETextureDimension::Texture2D, width, height, vec4(0.f), L"GBuffer_Normal"},
		{ETextureFormat::R8G8B8A8_UNORM,		ETextureUsage::RenderTarget | ETextureUsage::Sampler,		ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::GBuffer,		ETextureDimension::Texture2D, width, height, vec4(0.f), L"GBuffer_PBR"},
		{ETextureFormat::R8G8B8A8_UNORM,		ETextureUsage::RenderTarget | ETextureUsage::Sampler,		ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::GBuffer,		ETextureDimension::Texture2D, width, height, vec4(0.f), L"GBuffer_Emission"},
		{ETextureFormat::R32G32B32A32_FLOAT,	ETextureUsage::RenderTarget | ETextureUsage::Sampler,		ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::GBuffer,		ETextureDimension::Texture2D, width, height, vec4(0.f), L"GBuffer_Position"},
		//ShadowDepth
		{ETextureFormat::D32_FLOAT,				ETextureUsage::DepthStencilTarget | ETextureUsage::Sampler, ERenderTargetBindFlag::RTBF_DepthStencil | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::DepthStencil, ETextureDimension::Texture2D, width, height, vec4(0.f), L"ShadowDepth"},
		//Depth
		{ETextureFormat::D24_UNORM_S8_UINT,		ETextureUsage::DepthStencilTarget | ETextureUsage::Sampler, ERenderTargetBindFlag::RTBF_DepthStencil | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::DepthStencil, ETextureDimension::Texture2D, width, height, vec4(0.f), L"Depth"},
		//LightingColor
		{ETextureFormat::R8G8B8A8_UNORM,		ETextureUsage::RenderTarget | ETextureUsage::Sampler,		ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::Color,		ETextureDimension::Texture2D, width, height, vec4(0.f, 0.f, 0.f, 1.f), L"LightingColor"},
		//Ping-pong RTs
		{ETextureFormat::R8G8B8A8_UNORM,		ETextureUsage::RenderTarget | ETextureUsage::Sampler,		ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::Color,		ETextureDimension::Texture2D, width, height, vec4(0.f, 0.f, 0.f, 1.f), L"PP_A"},
		{ETextureFormat::R8G8B8A8_UNORM,		ETextureUsage::RenderTarget | ETextureUsage::Sampler,		ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource, ERenderTargetType::Color,		ETextureDimension::Texture2D, width, height, vec4(0.f, 0.f, 0.f, 1.f), L"PP_B"},
	};

	for (auto& rtDesc : gbufferRTDescs)
	{
		rtDesc.Name = prefix + rtDesc.Name;
		rtMgr.CreateRenderTarget(&rtDesc);
		m_OwnedRTNames.push_back(rtDesc.Name);
	}
#pragma endregion

#pragma region RenderPasses
	vector<wstring> gBufferNames = { m_OwnedRTNames.begin(), m_OwnedRTNames.begin() + 5 };
	m_ShadowDepthName = m_OwnedRTNames[5];
	wstring depthName = m_OwnedRTNames[6];
	m_LightingColorName = m_OwnedRTNames[7];
	m_PPRTNames[0] = m_OwnedRTNames[8];
	m_PPRTNames[1] = m_OwnedRTNames[9];

	auto& rpMgr = RenderPassManager::Get();
	m_ShadowPassID = rpMgr.RegisterRenderPass(
		prefix + L"ShadowPass", {}, m_ShadowDepthName,
		ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 0, ERenderSortType::FrontToBack, ERenderPassType::Shadow);

	m_GeometryPassID = rpMgr.RegisterRenderPass(
		prefix + L"GeometryPass", gBufferNames, depthName,
		ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.f, 0.f, -1.f), 0, ERenderSortType::FrontToBack, ERenderPassType::Geometry);

	m_LightingPassID = rpMgr.RegisterRenderPass(
		prefix + L"LightingPass", { m_LightingColorName }, L"",
		ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 100, ERenderSortType::None, ERenderPassType::Lighting);

	m_ForwardTransparentPassID = rpMgr.RegisterRenderPass(
		prefix + L"ForwardTransparentPass",
		{ m_LightingColorName }, depthName,
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 110, ERenderSortType::BackToFront, ERenderPassType::ForwardTransparent);

	// ── PostProcess ping-pong passes (priorities 120~135) ──
	for (uint32 pi = 0; pi < m_PPEffects.size() && pi < MAX_PP_PASSES; ++pi)
	{
		RenderPassID rpID = rpMgr.RegisterRenderPass(
			prefix + L"PP_" + std::to_wstring(pi), { m_PPRTNames[pi % 2] }, L"",
			ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
			ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
			vec4(0.0f, 0.f, 0.f, -1.f), 120 + pi, ERenderSortType::None, ERenderPassType::PostProcess);
		m_PPPassIDs.push_back(rpID);
	}

	m_UIOverlayPassID = rpMgr.RegisterRenderPass(
		prefix + L"UIOverlayPass", {}, L"",
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store, 
		vec4(0.f, 0.f, 0.f, -1.f), 200, ERenderSortType::FrontToBack, ERenderPassType::UI);
#pragma endregion

	m_ChannelPreviewPassID = rpMgr.RegisterRenderPass(
		prefix + L"ChannelPreviewPass", { m_PPRTNames[0] }, L"",
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Load, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.0f, 0.0f, 0.0f, -1.0f), 400, ERenderSortType::None, ERenderPassType::Custom);

	m_PassOptions = {
		{ L"Shadow",            true  },
		{ L"Lighting",          true  },
		{ L"ForwardTransparent",true  },
		{ L"UIOverlay",         true  },
	};
}
void BaseViewportPanel::PreparePipelines()
{
	ResourceManager& rm = ResourceManager::Get();
	auto createPipe = [&](const wstring& psName) -> Engine::RHIPipeline* {
		tagRHIPipelineDesc pd = {};
		pd.PipelineType = EPipelineType::Graphics;
		pd.VertexShader = rm.GetResourceHandle<Shader>(L"FullscreenQuadVS")->GetRHIShader();
		pd.PixelShader = rm.GetResourceHandle<Shader>(psName)->GetRHIShader();
		pd.ColorAttachmentCount = 1;
		pd.ColorAttachmentFormats[0] = ETextureFormat::R8G8B8A8_UNORM;
		pd.DepthStencilAttachmentFormat = ETextureFormat::UNKNOWN;
		pd.DepthStencilState.DepthTestEnable = false;
		pd.DepthStencilState.DepthWriteEnable = false;
		pd.Topology = ETopology::TriangleList;
		pd.CullMode = ECullMode::None;
		pd.BlendMode = EBlendMode::Opaque;
		return PipelineManager::Get().GetOrCreatePipeline(pd);
		};

	m_LightingPipeline = createPipe(L"LightingPS");
	m_ChannelPreviewPipeline = createPipe(L"ViewportChannelPS");

	tagPPEffect ppEffects[] = {
		{ L"Tone Mapping (ACES)", true, createPipe(L"PostProcess_ToneMappingPS") },
	};

	for (auto& effect : ppEffects)
	{
		m_PPEffects.push_back(move(effect));
	}
}
void BaseViewportPanel::Free()
{
	Safe_Release(m_EditorCamera);
	Safe_Release(m_InspectorPanel);
	m_Grid.Free();
	m_DebugRenderer.Free();
}
#pragma endregion

void BaseViewportPanel::Update(f32 dt)
{
	CalculateRenderResolution(m_PanelWidth, m_PanelHeight);
	m_EditorCamera->FixedUpdate(dt);
	m_EditorCamera->Update(dt);
	m_EditorCamera->LateUpdate(dt);

	RenderPassManager& rpMgr = RenderPassManager::Get();
	Renderer& renderer = Renderer::Get();
	Camera* camera = m_EditorCamera->GetCamera();
	renderer.RegisterViewportCamera(nullptr, rpMgr.GetRenderPassByID(m_ShadowPassID));
	renderer.RegisterViewportCamera(camera, rpMgr.GetRenderPassByID(m_GeometryPassID));
	renderer.RegisterViewportCamera(camera, rpMgr.GetRenderPassByID(m_ForwardTransparentPassID));
	renderer.RegisterViewportCamera(nullptr, rpMgr.GetRenderPassByID(m_UIOverlayPassID));

	SubmitLightingPass(m_EditorCamera->GetCamera());

	uint32 ppSlot = 0;
	wstring currentRT = m_LightingColorName;
	// 체인 순차 실행 (currentRT와 ppSlot이 함수들을 거치며 갱신됨)
	SubmitPostProcessPass(ppSlot, currentRT);
	SubmitUIOverlayPass(currentRT);

	if (!m_ShowFinalComposed)
	{
		currentRT = m_SelectedRTName;
	}

	SubmitChannelPreviewPass(ppSlot, currentRT);
	// 모든 체인이 끝난 후 최종 디스플레이 타겟 결정
	m_DisplayRTName = currentRT;

	if (*m_Grid.GetVisible())
	{
		m_Grid.SubmitGrid(camera, IsOrthographic(), m_DisplayRTName, m_OwnedRTNames[6]);
	}

#ifdef _DEBUG
	if (*m_DebugRenderer.GetDrawCollidersPtr())
	{
		m_DebugRenderer.SubmitDebugDraw(camera, m_DisplayRTName);
	}
#endif
	

	if(m_Focused && m_Hovered)
		HandleInput(dt);
}

void BaseViewportPanel::HandleInput(f32 dt)
{
	m_EditorCamera->HandleInput(dt);
}

#pragma region Resolution&Ratio
void BaseViewportPanel::CalculateRenderResolution(uint32 panelWidth, uint32 panelHeight)
{
	uint32 newWidth = panelWidth;
	uint32 newHeight = panelHeight;
	switch (m_ResolutionMode)
	{
	case EViewportResolutionMode::Free:
		newWidth = panelWidth;
		newHeight = panelHeight;
		break;
	case EViewportResolutionMode::Preset:
		newWidth = g_ResolutionPresets[m_PresetIndex].Width;
		newHeight = g_ResolutionPresets[m_PresetIndex].Height;
		break;
	case EViewportResolutionMode::Custom:
		newWidth = (uint32)m_CustomWidth;
		newHeight = (uint32)m_CustomHeight;
		break;
	}
	// Aspect Ratio 강제 적용 (Free 모드 + 비율 고정 시)
	if (m_AspectRatioMode != EAspectRatioMode::Free)
	{
		float targetRatio = GetAspectRatio(m_AspectRatioMode);
		float currentRatio = (float)newWidth / (float)newHeight;
		if (currentRatio > targetRatio)
			newWidth = (uint32)(newHeight * targetRatio);
		else
			newHeight = (uint32)(newWidth / targetRatio);
	}
	// 최소 크기 보정
	newWidth = glm::max(newWidth, (uint32)64);
	newHeight = glm::max(newHeight, (uint32)64);
	// 변경 감지 → RT 재생성
	if (newWidth != m_RenderWidth || newHeight != m_RenderHeight)
	{
		m_RenderWidth = newWidth;
		m_RenderHeight = newHeight;
		ResizeRenderTargets(m_RenderWidth, m_RenderHeight);
	}
}

f32 BaseViewportPanel::GetAspectRatio(EAspectRatioMode mode) const
{
	switch (mode)
	{
	case EAspectRatioMode::Ratio_16_9:  return 16.f / 9.f;
	case EAspectRatioMode::Ratio_16_10: return 16.f / 10.f;
	case EAspectRatioMode::Ratio_4_3:   return 4.f / 3.f;
	case EAspectRatioMode::Ratio_21_9:  return 21.f / 9.f;
	case EAspectRatioMode::Ratio_1_1:   return 1.f;
	case EAspectRatioMode::Ratio_9_16:  return 9.f / 16.f;
	default: return 1.f;
	}
}

void BaseViewportPanel::ResizeRenderTargets(uint32 width, uint32 height)
{
	auto& rtMgr = RenderTargetManager::Get();
	// 소유한 모든 RT를 새 크기로 재생성
	for (const auto& rtName : m_OwnedRTNames)
	{
		RenderTarget* rt = rtMgr.GetRenderTarget(rtName);
		if (rt)
			rt->Resize(width, height);   // RenderTarget::Resize()가 필요
	}
	// 카메라 종횡비 갱신
	m_EditorCamera->GetCamera()->SetAspect((f32)width / (f32)height);
	// Ortho 카메라의 경우 OrthoSize도 갱신
	if (IsOrthographic())
		m_EditorCamera->GetCamera()->SetOrthoSize((f32)height);
}

void BaseViewportPanel::DrawResolutionMenu()
{
	if (ImGui::BeginMenu("Resolution"))
	{
		// --- Resolution Mode ---
		if (ImGui::MenuItem("Free (Panel Size)", nullptr,
			m_ResolutionMode == EViewportResolutionMode::Free))
			m_ResolutionMode = EViewportResolutionMode::Free;
		ImGui::Separator();
		// --- Presets ---
		for (uint32 i = 0; i < g_PresetCount; ++i)
		{
			bool selected = (m_ResolutionMode == EViewportResolutionMode::Preset
				&& m_PresetIndex == (int32)i);
			if (ImGui::MenuItem(g_ResolutionPresets[i].Name, nullptr, selected))
			{
				m_ResolutionMode = EViewportResolutionMode::Preset;
				m_PresetIndex = i;
			}
		}
		ImGui::Separator();
		// --- Custom ---
		bool isCustom = (m_ResolutionMode == EViewportResolutionMode::Custom);
		if (ImGui::MenuItem("Custom...", nullptr, isCustom))
			m_ResolutionMode = EViewportResolutionMode::Custom;
		if (isCustom)
		{
			ImGui::InputInt("Width", &m_CustomWidth, 1, 100);
			ImGui::InputInt("Height", &m_CustomHeight, 1, 100);
			m_CustomWidth = glm::clamp(m_CustomWidth, 64, 7680);
			m_CustomHeight = glm::clamp(m_CustomHeight, 64, 4320);
		}
		ImGui::Separator();
		// --- Aspect Ratio ---
		if (ImGui::BeginMenu("Aspect Ratio"))
		{
			auto ratioItem = [&](const char* label, EAspectRatioMode mode)
				{
					if (ImGui::MenuItem(label, nullptr, m_AspectRatioMode == mode))
						m_AspectRatioMode = mode;
				};
			ratioItem("Free", EAspectRatioMode::Free);
			ratioItem("16:9", EAspectRatioMode::Ratio_16_9);
			ratioItem("16:10", EAspectRatioMode::Ratio_16_10);
			ratioItem("4:3", EAspectRatioMode::Ratio_4_3);
			ratioItem("21:9", EAspectRatioMode::Ratio_21_9);
			ratioItem("1:1", EAspectRatioMode::Ratio_1_1);
			ratioItem("9:16", EAspectRatioMode::Ratio_9_16);
			ImGui::EndMenu();
		}
		// --- 현재 해상도 표시 ---
		ImGui::Separator();
		ImGui::Text("Render: %u x %u", m_RenderWidth, m_RenderHeight);
		float scale = (float)m_RenderWidth / (float)m_PanelWidth * 100.f;
		ImGui::Text("Scale: %.0f%%", scale);
		ImGui::EndMenu();
	}
}
#pragma endregion

void BaseViewportPanel::Draw()
{
	if (!m_Open) return;
	ImGui::PushID(this);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	bool opened = true;
	ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar;
	string windowID = WStrToStr(m_Name);
	if (ImGui::Begin(windowID.c_str(), &opened, window_flags))
	{
		DrawOptionsBar();
		ImVec2 panelSize = ImGui::GetContentRegionAvail();
		m_PanelWidth = glm::max((uint32)panelSize.x, (uint32)1);
		m_PanelHeight = glm::max((uint32)panelSize.y, (uint32)1);
		uint32 width = (uint32)panelSize.x;
		uint32 height = (uint32)panelSize.y;
		f32 panelAspectRatio = (f32)width / (f32)height;

		RenderTarget* currentRT = RenderTargetManager::Get().GetRenderTarget(m_DisplayRTName);
		if (currentRT)
		{
			RHITexture* texture = currentRT->GetTexture();
			if (texture)
			{
				f32 imageWidth = (f32)currentRT->GetWidth();
				f32 imageHeight = (f32)currentRT->GetHeight();

				f32 imageRatio = imageWidth / imageHeight;

				ImVec2 finalSize = panelSize;

				if (panelAspectRatio > imageRatio)
				{
					finalSize.y = panelSize.y;
					finalSize.x = finalSize.y * imageRatio;
				}
				else
				{
					finalSize.x = panelSize.x;
					finalSize.y = finalSize.x / imageRatio;
				}

				m_Focused = ImGui::IsWindowFocused();
				m_Hovered = ImGui::IsWindowHovered();

				ImVec2 cursorStart = ImGui::GetCursorPos();
				float offsetX = (panelSize.x - finalSize.x) * 0.5f;
				float offsetY = (panelSize.y - finalSize.y) * 0.5f;
				ImGui::SetCursorPos(ImVec2((float)(int)(cursorStart.x + offsetX), (float)(int)(cursorStart.y + offsetY)));
				ImVec2 imageScreenPos = ImGui::GetCursorScreenPos();

				ImTextureID textureID = (ImTextureID)(size_t)texture->GetNativeHandle();
				ImGui::Image(textureID, finalSize);
			}
		}
	}
	ImGui::End();
	ImGui::PopStyleVar();

	m_InspectorPanel->Draw();

	ImGui::PopID();
}

#pragma region Options Bar
void BaseViewportPanel::DrawOptionsBar()
{
	if (ImGui::BeginMenuBar())
	{
		if (ImGui::BeginMenu("RenderTarget"))
		{
			if (ImGui::MenuItem("FinalRT", nullptr, m_ShowFinalComposed))
			{
				m_ShowFinalComposed = true;
				m_Grid.Show();
			}
				

			ImGui::Separator();

			// ── 1. 콤보박스: 이 뷰포트의 RT (짧은 이름) ──
			wstring prefix = m_Name + L"_";
			int currentIdx = 0;
			vector<string> shortLabels;
			for (int i = 0; i < m_OwnedRTNames.size(); ++i)
			{
				wstring shortW = m_OwnedRTNames[i];
				if (shortW.find(prefix) == 0)
					shortW = shortW.substr(prefix.size());
				shortLabels.push_back(WStrToStr(shortW));
				if (m_OwnedRTNames[i] == m_SelectedRTName)
					currentIdx = i;
			}
			// 현재 선택이 OwnedRT가 아니면 "Other" 표시
			const char* preview = currentIdx < shortLabels.size()
				? shortLabels[currentIdx].c_str() : "Other";
			if (ImGui::BeginCombo("##RTSelect", preview))
			{
				for (int i = 0; i < shortLabels.size(); ++i)
				{
					bool selected = !m_ShowFinalComposed && (m_OwnedRTNames[i] == m_SelectedRTName);
					if (ImGui::Selectable(shortLabels[i].c_str(), selected))
					{
						m_ShowFinalComposed = false;
						m_SelectedRTName = m_OwnedRTNames[i];
						m_Grid.Hide();
					}
					if (selected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			// ── 2. 접힌 메뉴: 전체 RT (풀 이름) ──
			if (ImGui::BeginMenu("All RenderTargets"))
			{
				auto allNames = RenderTargetManager::Get().GetAllRenderTargetNames();
				for (const auto& name : allNames)
				{
					string label = WStrToStr(name);
					bool selected = !m_ShowFinalComposed && (name == m_SelectedRTName);
					if (ImGui::MenuItem(label.c_str(), nullptr, selected))
					{
						m_ShowFinalComposed = false;
						m_SelectedRTName = name;
						m_Grid.Hide();
					}
				}
				ImGui::EndMenu();
			}
			ImGui::EndMenu();
		}

		DrawResolutionMenu();

		if (ImGui::BeginMenu("Debug"))
		{
			ImGui::MenuItem("Show Grid", nullptr, m_Grid.GetVisible());
			ImGui::MenuItem("Show Collider", nullptr, m_DebugRenderer.GetDrawCollidersPtr());
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Post FX"))
		{
			//ImGui::MenuItem("Use Post FX Output as Display", nullptr, &m_UsePostProcessOutputAsDisplay);
			ImGui::SeparatorText("Effects");

			for (int i = 0; i < (int)m_PPEffects.size(); ++i)
			{
				auto& effect = m_PPEffects[i];
				string effectName = WStrToStr(effect.Name);
				string checkboxID = "##PPEnabled_" + std::to_string(i);

				ImGui::Checkbox(checkboxID.c_str(), &effect.Enabled);
				ImGui::SameLine();

				if (effect.Enabled)
				{
					if (ImGui::BeginMenu(effectName.c_str()))
					{
						if (i == 0) // Tone Mapping
						{
							ImGui::SetNextItemWidth(160.f);
							ImGui::SliderFloat("Exposure", &m_ToneMappingParams.exposure, 0.1f, 5.0f, "%.2f");
							ImGui::SetNextItemWidth(160.f);
							ImGui::SliderFloat("Gamma", &m_ToneMappingParams.gamma, 1.0f, 3.0f, "%.2f");
							if (ImGui::Button("Reset", ImVec2(-1, 0)))
							{
								m_ToneMappingParams.exposure = 1.0f;
								m_ToneMappingParams.gamma = 2.2f;
							}
						}

						ImGui::EndMenu();
					}
				}
				else
				{
					ImGui::TextDisabled("%s", effectName.c_str());
				}
			}

			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("Passes"))
		{
			for (auto& pass : m_PassOptions)
			{
				string label = WStrToStr(pass.Name);
				ImGui::MenuItem(label.c_str(), nullptr, &pass.Enabled);
			}
			ImGui::EndMenu();
		}

		if (ImGui::Button("Camera"))
		{
			m_InspectorPanel->ToggleOpen();
		}

		DrawChannelViewButton();
		DrawDimensionToggleButton();
	}
	ImGui::EndMenuBar();
}
void BaseViewportPanel::DrawChannelViewButton()
{
	const ImVec2 buttonSize(28.0f, 0.0f);
	const float buttonSpacing = 0.0f;
	const float groupGapToDimension = 8.0f;
	const float rightPadding = 10.0f;

	const float dimensionGroupWidth = 30.0f * 2.0f;
	const float channelGroupWidth = buttonSize.x * 4.0f + buttonSpacing * 3.0f;

	ImGui::SameLine(
		ImGui::GetWindowWidth() - dimensionGroupWidth - groupGapToDimension - channelGroupWidth - rightPadding);

	auto toggleFlag = [this](EViewportChannelView flag)
		{
			const uint8 current = static_cast<uint8>(m_ChannelView);
			const uint8 bit = static_cast<uint8>(flag);

			if ((current & bit) != 0)
				m_ChannelView = static_cast<EViewportChannelView>(current & ~bit);
			else
				m_ChannelView = static_cast<EViewportChannelView>(current | bit);
		};

	auto drawToggleButton = [&](const char* label, EViewportChannelView flag, const ImVec4& activeColor)
		{
			const bool isActive = HasFlag(m_ChannelView, flag);

			if (isActive)
				ImGui::PushStyleColor(ImGuiCol_Button, activeColor);
			else
				ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);

			if (ImGui::Button(label, buttonSize))
				toggleFlag(flag);

			ImGui::PopStyleColor();
		};

	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(buttonSpacing, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);

	drawToggleButton("R", EViewportChannelView::R, ImVec4(0.75f, 0.20f, 0.20f, 1.0f));
	ImGui::SameLine();
	drawToggleButton("G", EViewportChannelView::G, ImVec4(0.20f, 0.65f, 0.20f, 1.0f));
	ImGui::SameLine();
	drawToggleButton("B", EViewportChannelView::B, ImVec4(0.20f, 0.35f, 0.80f, 1.0f));
	ImGui::SameLine();
	drawToggleButton("A", EViewportChannelView::A, ImVec4(0.55f, 0.55f, 0.55f, 1.0f));

	ImGui::PopStyleVar(2);

}
void BaseViewportPanel::DrawDimensionToggleButton()
{
	// 1. 버튼 크기 및 그룹 전체 너비 계산
	ImVec2 buttonSize(30.0f, 0.0f); // 너비 30px 고정 (텍스트에 맞춰 조절 가능)
	float groupWidth = buttonSize.x * 2.0f;
	float rightPadding = 10.0f; // 우측 여백

	// 2. 커서를 우측 끝으로 이동 (윈도우 너비 - 그룹 너비 - 여백)
	ImGui::SameLine(ImGui::GetWindowWidth() - groupWidth - rightPadding);

	// 3. 스타일 적용: 간격 제거 및 각진 모서리 (토글 느낌)
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f); // 네모난 버튼

	// 4. [2D 버튼] 그리기
	// 활성화 상태(2D)면 'Active' 색상, 아니면 기본 배경보다 어두운 색상 사용
	if (IsOrthographic())
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
	else
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);

	if (ImGui::Button("2D", buttonSize))
	{
		SetOrthographic();
	}
	ImGui::PopStyleColor(); // 색상 복구

	ImGui::SameLine(); // 옆에 바로 붙이기

	// 5. [3D 버튼] 그리기
	// 비활성화 상태(!2D)면 'Active' 색상
	if (IsPerspective())
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_ButtonActive]);
	else
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyle().Colors[ImGuiCol_FrameBg]);

	if (ImGui::Button("3D", buttonSize))
	{
		SetPerspective();
	}
	ImGui::PopStyleColor(); // 색상 복구

	// 6. 스타일 복구
	ImGui::PopStyleVar(2); // ItemSpacing, FrameRounding
}
#pragma endregion

#pragma region Rendering
void BaseViewportPanel::SubmitLightingPass(Camera* camera)
{
	Renderer::Get().SubmitCustomCommand(
		[this, camera](f32 dt, RenderPass* pass) -> EResult
		{
			auto* rhi = Renderer::Get().GetRHI();
			auto& rtMgr = RenderTargetManager::Get();
			auto& smMgr = SamplerManager::Get();
			RHISampler* sampler = smMgr.GetDefaultSampler();
			wstring prefix = m_Name + L"_";

			tagCameraBuffer camBuf = camera->GetCameraBuffer();
			camBuf.time = dt;
			rhi->BindConstantBuffer(&camBuf, sizeof(tagCameraBuffer), 0);
			rhi->BindConstantBuffer(&camBuf, sizeof(tagCameraBuffer), 3);
			rhi->BindTextureSampler(
				rtMgr.GetRenderTarget(prefix + L"GBuffer_Diffuse")->GetTexture(),
				sampler, 0);
			rhi->BindTextureSampler(
				rtMgr.GetRenderTarget(prefix + L"GBuffer_Normal")->GetTexture(),
				sampler, 1);
			rhi->BindTextureSampler(
				rtMgr.GetRenderTarget(prefix + L"GBuffer_PBR")->GetTexture(),
				sampler, 2);
			rhi->BindTextureSampler(
				rtMgr.GetRenderTarget(prefix + L"GBuffer_Emission")->GetTexture(),
				sampler, 3);
			rhi->BindTextureSampler(
				rtMgr.GetRenderTarget(prefix + L"GBuffer_Position")->GetTexture(),
				sampler, 4);
			rhi->BindTextureSampler(
				rtMgr.GetRenderTarget(m_ShadowDepthName)->GetTexture(),
				sampler, 5);
			// Lighting 파이프라인 바인딩
			{
				
				tagCameraBuffer shadowCam = LightManager::Get().GetShadowCameraBuffer(0u);
				tagLightShadowData shadowCamData = LightManager::Get().GetShadowData(0u);
				rhi->BindConstantBuffer(&shadowCamData, sizeof(shadowCamData), 1);
			}
			rhi->BindPipeline(m_LightingPipeline);

			if (IsFailure(LightManager::Get().Bind(0)))
			{
				fmt::print(stderr, "LightManager::Bind failed\n");
				return EResult::Fail;
			}

			if (IsFailure(rhi->Draw(3)))
			{
				fmt::print(stderr, "Lighting draw failed\n");
				return EResult::Fail;
			}
			return EResult::Success;
		},
		m_LightingPassID);
}

void BaseViewportPanel::SubmitPostProcessPass(uint32& slot, std::wstring& currentRT)
{
	for (auto& effect : m_PPEffects)
	{
		if (!effect.Enabled || !effect.Pipeline) continue;
		if (slot >= MAX_PP_PASSES) break;
		const wstring writeRT = m_PPRTNames[slot % 2];
		Engine::Renderer::Get().SubmitCustomCommand(
			[this, &effect, capturedRead = currentRT](f32 dt, Engine::RenderPass* pass) -> EResult
			{
				auto* src = Engine::RenderTargetManager::Get().GetRenderTarget(capturedRead);
				if (!src || !src->GetTexture()) return EResult::Fail;
				auto* rhi = Engine::Renderer::Get().GetRHI();
				rhi->BindTextureSampler(src->GetTexture(), Engine::SamplerManager::Get().GetDefaultSampler(), 0);
				rhi->BindConstantBuffer(&m_ToneMappingParams, sizeof(m_ToneMappingParams), 0);
				rhi->BindPipeline(effect.Pipeline);
				return rhi->Draw(3);
			}, m_PPPassIDs[slot]);
		currentRT = writeRT; // 다음 패스를 위해 읽을 타겟 갱신
		++slot;
	}
}
void BaseViewportPanel::SubmitUIOverlayPass(const wstring& currentRT)
{
	RenderPassManager::Get().GetRenderPassByID(m_UIOverlayPassID)->SetColorAttachments({ currentRT });
}
void BaseViewportPanel::SubmitChannelPreviewPass(uint32& slot, wstring& currentRT)
{
	if (m_ChannelView == EViewportChannelView::RGBA || !m_ChannelPreviewPipeline)
		return;

	const wstring writeRT = m_PPRTNames[slot % 2];
	const wstring sourceRTName = currentRT;

	RenderPass* channelPass = RenderPassManager::Get().GetRenderPassByID(m_ChannelPreviewPassID);
	if(channelPass)
		channelPass->SetColorAttachments({ m_PPRTNames[slot % 2] }); // 현재 슬롯의 쓰기 RT로 채널 프리뷰 패스 설정
	
	Engine::Renderer::Get().SubmitCustomCommand(
		[this, capturedRead = sourceRTName](f32 dt, Engine::RenderPass* pass) -> EResult
		{
			auto* src = Engine::RenderTargetManager::Get().GetRenderTarget(capturedRead);
			if (!src || !src->GetTexture()) return EResult::Fail;
			auto* rhi = Engine::Renderer::Get().GetRHI();
			ChannelViewData channelData = { static_cast<uint32>(m_ChannelView) };

			rhi->BindConstantBuffer(&channelData, sizeof(ChannelViewData), 0);
			rhi->BindTextureSampler(src->GetTexture(), Engine::SamplerManager::Get().GetDefaultSampler(), 0);
			rhi->BindPipeline(m_ChannelPreviewPipeline);
			return rhi->Draw(3);
		}, m_ChannelPreviewPassID);

	currentRT = writeRT;
	++slot;
}

#pragma endregion