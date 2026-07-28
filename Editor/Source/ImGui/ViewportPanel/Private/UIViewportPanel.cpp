#pragma once

#include "UIViewportPanel.h"
#include "RectTransform.h"
#include "SelectionManager.h"
#include "UICanvas.h"

void UIViewportPanel::Initialize(void* arg)
{
	__super::Initialize(arg);
	CAST_DESC;
	SetOrthographic();
	PrepareRenderTargetsAndPasses(desc->RenderTargetWidth, desc->RenderTargetHeight);
	m_Grid.Initialize(m_Name);

    m_SelectedRTName = m_UIColorRTName;
    m_DisplayRTName = m_UIColorRTName;

    m_LayoutOwnerToken = static_cast<uint64>(reinterpret_cast<uintptr_t>(this));
}

void UIViewportPanel::Free()
{
    __super::Free();
    ClearCanvasLayoutOverride();
	m_Grid.Free();
}

void UIViewportPanel::PrepareRenderTargetsAndPasses(uint32 width, uint32 height)
{
    wstring prefix = m_Name + L"_";
    m_UIColorRTName = prefix + L"UIColor";
	m_VirtualCanvasRTName = prefix + L"VirtualCanvas";
    tagRenderTargetDesc colorDesc = {
        ETextureFormat::R8G8B8A8_UNORM,
        ETextureUsage::RenderTarget | ETextureUsage::Sampler,
        ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource,
        ERenderTargetType::Color,
        ETextureDimension::Texture2D,
        width, height,
        vec4(0.f, 0.f, 0.f, 1.f),
        m_UIColorRTName
    };
    RenderTargetManager::Get().CreateRenderTarget(&colorDesc);

    tagRenderTargetDesc virtualCanvasDesc = {
        ETextureFormat::R8G8B8A8_UNORM, ETextureUsage::RenderTarget | ETextureUsage::Sampler,
        ERenderTargetBindFlag::RTBF_RenderTarget | ERenderTargetBindFlag::RTBF_ShaderResource,
        ERenderTargetType::Color, ETextureDimension::Texture2D,
        (uint32)m_UIDesignWidth, (uint32)m_UIDesignHeight, vec4(0.f, 0.f, 0.f, 0.f), m_VirtualCanvasRTName
    };
    RenderTargetManager::Get().CreateRenderTarget(&virtualCanvasDesc);

	auto& rpMgr = RenderPassManager::Get();
	
	m_UIOverlayPassID = rpMgr.RegisterRenderPass(
		prefix + L"UIOverlayPass", {}, L"",
		ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
		ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
		vec4(0.f, 0.f, 0.f, 0.f), 200, ERenderSortType::FrontToBack, ERenderPassType::UI,
		EBlendMode::Opaque | EBlendMode::Masked | EBlendMode::AlphaBlend | EBlendMode::Additive | EBlendMode::NonPremultiplied);

    m_ClearPassID = rpMgr.RegisterRenderPass(
        prefix + L"ClearPass", {}, L"",
        ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
        ERenderPassLoadOperation::RPLO_Clear, ERenderPassStoreOperation::RPSO_Store,
        vec4(0.1f, 0.1f, 0.1f, 1.f), 10, ERenderSortType::None, ERenderPassType::Custom,
        EBlendMode::None);
}

void UIViewportPanel::Update(f32 dt)
{
	CalculateRenderResolution(m_PanelWidth, m_PanelHeight);

	//if (m_OwnedCamera && m_Focused && m_Hovered)
	//{
	//	m_OwnedCamera->HandleInput(dt);
	//}

	if (m_OwnedCamera)
	{
		m_OwnedCamera->FixedUpdate(dt);
		m_OwnedCamera->Update(dt);
		m_OwnedCamera->LateUpdate(dt);
	}

    RenderPassManager& rpMgr = RenderPassManager::Get();
    Renderer& renderer = Renderer::Get();
    renderer.RegisterViewportCamera(nullptr, rpMgr.GetRenderPassByID(m_UIOverlayPassID));

    // 2. UI 렌더링 지시 (Virtual Canvas에 쓰기)
    SubmitUIOverlayPass(m_VirtualCanvasRTName); 
    
    // 3. 배경 RT 설정 및 명시적 클리어, 그리드 렌더링
    m_DisplayRTName = m_UIColorRTName; // 배경은 항상 그리드가 있는 컬러 RT
    
    rpMgr.GetRenderPassByID(m_ClearPassID)->SetColorAttachments({ m_DisplayRTName });
    renderer.SubmitCustomCommand([](f32, RenderPass*) { return EResult::Success; }, m_ClearPassID);

    if (*m_Grid.GetVisible())
    {
        m_Grid.SubmitGrid(m_Camera, IsOrthographic(), m_DisplayRTName, L"");
    }

    // 4. [핵심] 채널 필터를 Virtual Canvas 전용으로 동작하게 만듭니다.
    m_CanvasToDraw = m_VirtualCanvasRTName;
    if (m_ChannelView != EViewportChannelView::RGBA)
    {
        m_ChannelFilter.SetChannelView(m_ChannelView);
        m_ChannelFilter.SubmitChannelPreviewPass(m_VirtualCanvasRTName, m_CanvasToDraw);
    }

    SyncCanvasLayoutOverride();

    KeyboardInput();
    MouseInput();
}

void UIViewportPanel::Draw()
{
	__super::Draw();
}

#pragma region Custom Draws
void UIViewportPanel::DrawCustomViewport()
{
    BaseViewportPanel::DrawRenderTargetImage(m_DisplayRTName);
    DrawCanvasOverlay();
    Draw2DGuizmo();
    DrawSelectionLocked();
}

void UIViewportPanel::DrawCustomOptions()
{
    DrawResolutionMenu();
    DrawLayoutMenu();
	DrawGizmoMenu();
    DrawDebugMenu();
    DrawChannelViewButton();
}
void UIViewportPanel::DrawCanvasOverlay()
{
    RenderTarget* canvasRT = RenderTargetManager::Get().GetRenderTarget(m_CanvasToDraw);
    if (!canvasRT || !canvasRT->GetTexture() || !m_Camera) return;
    mat4 view = m_Camera->GetViewMatrix();
    mat4 proj = m_Camera->GetProjMatrix();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    // 캔버스 4개의 꼭짓점 월드 좌표 (0,0 에서 우측 하단으로 전개되게끔 Y축 음수화)
    vec4 canvasCorners[4] = {
        vec4(0.0f, 0.0f, 0.0f, 1.0f),
        vec4((f32)m_UIDesignWidth, 0.0f, 0.0f, 1.0f),
        vec4((f32)m_UIDesignWidth, -(f32)m_UIDesignHeight, 0.0f, 1.0f),
        vec4(0.0f, -(f32)m_UIDesignHeight, 0.0f, 1.0f)
    };
    ImVec2 screenCorners[4];
    bool valid = true;
    for (int i = 0; i < 4; ++i)
    {
        vec4 clipPt = proj * view * canvasCorners[i];
        if (std::abs(clipPt.w) < 1e-6f) { valid = false; break; }
        vec3 ndcPt = vec3(clipPt) / clipPt.w;
        screenCorners[i].x = m_ImageScreenPos.x + (ndcPt.x * 0.5f + 0.5f) * m_ImageSize.x;
        screenCorners[i].y = m_ImageScreenPos.y + (1.0f - (ndcPt.y * 0.5f + 0.5f)) * m_ImageSize.y;
    }
    if (!valid) return;
    // 투영된 영역에 캔버스 이미지 출력
    dl->AddImageQuad(
        (ImTextureID)canvasRT->GetTexture()->GetNativeHandle(),
        screenCorners[0], screenCorners[1], screenCorners[2], screenCorners[3]
    );
    // 캔버스 바운드 (외곽선) 그리기
    const ImU32 canvasLineColor = IM_COL32(200, 200, 200, 180);
    dl->AddQuad(screenCorners[0], screenCorners[1], screenCorners[2], screenCorners[3], canvasLineColor, 2.0f);

    // 해상도 텍스트 표시
    char resText[32];
    snprintf(resText, sizeof(resText), "Canvas (%dx%d)", m_UIDesignWidth, m_UIDesignHeight);
    ImVec2 textSize = ImGui::CalcTextSize(resText);
    dl->AddText(ImVec2(screenCorners[0].x + 5.0f, screenCorners[0].y - textSize.y - 2.0f), canvasLineColor, resText);
}
#pragma endregion

#pragma region Options Bar
void UIViewportPanel::DrawGizmoMenu()
{
	if (ImGui::BeginMenu("Gizmo"))
	{
		if (ImGui::MenuItem("Translate (W)", "Shift + 1", m_GizmoOperation == ImGuizmo::TRANSLATE))
			m_GizmoOperation = ImGuizmo::TRANSLATE;
		if (ImGui::MenuItem("Rotate (E)", "Shift + 2", m_GizmoOperation == ImGuizmo::ROTATE))
			m_GizmoOperation = ImGuizmo::ROTATE;
		if (ImGui::MenuItem("Scale (R)", "Shift + 3", m_GizmoOperation == ImGuizmo::SCALE))
			m_GizmoOperation = ImGuizmo::SCALE;
		ImGui::Separator();
		if (ImGui::MenuItem("Local", nullptr, m_GizmoMode == ImGuizmo::LOCAL))
			m_GizmoMode = ImGuizmo::LOCAL;
		if (ImGui::MenuItem("World", nullptr, m_GizmoMode == ImGuizmo::WORLD))
			m_GizmoMode = ImGuizmo::WORLD;

		ImGui::Separator();
		if (ImGui::MenuItem("Snap to Grid", nullptr, m_GizmoUseSnap))
			m_GizmoUseSnap = !m_GizmoUseSnap;
		if (m_GizmoUseSnap)
		{
			ImGui::InputFloat3("Snap Translation", glm::value_ptr(m_GizmoSnapTranslation));
			ImGui::InputFloat3("Snap Rotation", glm::value_ptr(m_GizmoSnapRotation));
			ImGui::InputFloat3("Snap Scale", glm::value_ptr(m_GizmoSnapScale));
		}
		ImGui::EndMenu();
	}
}
void UIViewportPanel::DrawLayoutMenu()
{
    if (ImGui::BeginMenu("Canvas Layout Resolution"))
    {
        for (uint32 i = 0; i < g_PresetCount; ++i)
        {
            bool selected = (m_UIDesignWidth == (int32)g_ResolutionPresets[i].Width && m_UIDesignHeight == (int32)g_ResolutionPresets[i].Height);
            if (ImGui::MenuItem(g_ResolutionPresets[i].Name, nullptr, selected))
            {
                m_UIDesignWidth = (int32)g_ResolutionPresets[i].Width;
                m_UIDesignHeight = (int32)g_ResolutionPresets[i].Height;
                auto* rt = RenderTargetManager::Get().GetRenderTarget(m_VirtualCanvasRTName);
                if (rt) rt->Resize(m_UIDesignWidth, m_UIDesignHeight);
            }
        }
        
        ImGui::Separator();
        
        bool isCustom = true;
        for (uint32 i = 0; i < g_PresetCount; ++i) {
            if (m_UIDesignWidth == (int32)g_ResolutionPresets[i].Width && m_UIDesignHeight == (int32)g_ResolutionPresets[i].Height) {
                isCustom = false; break;
            }
        }

        if (ImGui::MenuItem("Custom...", nullptr, isCustom)) {}
        
        int w = m_UIDesignWidth;
        int h = m_UIDesignHeight;
        bool changed = false;
        if (ImGui::InputInt("Width", &w, 1, 100)) changed = true;
        if (ImGui::InputInt("Height", &h, 1, 100)) changed = true;

        if (changed)
        {
            m_UIDesignWidth = glm::clamp(w, 64, 7680);
            m_UIDesignHeight = glm::clamp(h, 64, 4320);
            auto* rt = RenderTargetManager::Get().GetRenderTarget(m_VirtualCanvasRTName);
            if (rt) rt->Resize(m_UIDesignWidth, m_UIDesignHeight);
        }

        ImGui::EndMenu();
    }
}
void UIViewportPanel::DrawDebugMenu()
{
    if (ImGui::BeginMenu("Debug"))
    {
        ImGui::MenuItem("Show Grid", nullptr, m_Grid.GetVisible());
        ImGui::EndMenu();
    }
}
void UIViewportPanel::ResizeRenderTargets(uint32 width, uint32 height)
{
    auto& rtMgr = RenderTargetManager::Get();
    RenderTarget* rt = rtMgr.GetRenderTarget(m_UIColorRTName);
    rt->Resize(width, height);

    if (m_Camera)
        m_Camera->SetAspect(static_cast<f32>(width) / static_cast<f32>(height));

    // Base: ChannelFilter RT 리사이즈
    BaseViewportPanel::ResizeRenderTargets(width, height);
}
#pragma endregion

#pragma region Overlay

void UIViewportPanel::DrawSelectionLocked()
{
	// 선택 잠금 상태인지 확인
	if (SelectionManager::Get().IsPrimarySelectionLocked())
	{
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		if (!drawList) return;
		// ─── 1. 위치 계산 (좌측 하단) ───
		float padding = 15.0f; // 여백
		float fontSize = ImGui::GetFontSize();

		// 위치: 이미지 왼쪽 끝 + 패딩, 이미지 아래쪽 끝 - 패딩 - 글자 높이
		ImVec2 textPos = ImVec2(
			m_ImageScreenPos.x + padding,
			m_ImageScreenPos.y + m_ImageSize.y - padding - fontSize
		);
		// ─── 2. 가독성을 위한 그림자/배경 (선택 사항) ───
		// 검은색 그림자를 살짝 깔아주면 밝은 배경에서도 잘 보입니다.
		drawList->AddText(ImVec2(textPos.x + 1, textPos.y + 1), IM_COL32(0, 0, 0, 200), "Selection Locked");
		// ─── 3. 초록색 텍스트 출력 ───
		// 밝은 초록색 (Green)
		ImU32 textColor = IM_COL32(50, 255, 50, 255);
		drawList->AddText(textPos, textColor, "Selection Locked");
	}
}

void UIViewportPanel::Draw2DGuizmo()
{
    GameObject* selectedObject = SelectionManager::Get().GetPrimarySelection();
    if (!selectedObject) return;

    RectTransform* transform = selectedObject->GetComponent<RectTransform>();
    if (!transform) return;

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    if (!drawList || m_ImageSize.x <= 0 || m_ImageSize.y <= 0) return;

    // ─── 1. ImGuizmo 설정 ───
    ImGuizmo::SetOrthographic(true);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(m_ImageScreenPos.x, m_ImageScreenPos.y, m_ImageSize.x, m_ImageSize.y);
    ImGuizmo::SetID(ImGui::GetID("2DGuizmo"));

    // ─── 2. 프로젝션 & 뷰 행렬 (진짜 카메라 사용!) ───
    mat4 viewMatrix = m_Camera->GetViewMatrix();
    mat4 projMatrix = m_Camera->GetProjMatrix();

    // ─── 3. 행렬 준비 ───
    mat4 oldWorldTransform = transform->GetWorldTransform();

    // 1) UI 월드 트랜스폼 데이터 추출 (2D)
    vec2 uiPos = vec2(oldWorldTransform[3]);
    float uiRot = atan2(oldWorldTransform[0][1], oldWorldTransform[0][0]);
    vec2 uiScale = vec2(glm::length(vec3(oldWorldTransform[0])), glm::length(vec3(oldWorldTransform[1])));

    // 2) Standard 3D 매트릭스로 변환 (Y 위치 반전, 회전 반전)
    // UI의 Y-Down 좌표계를 EditorCamera의 Y-Up 좌표계에 맞춥니다.
    mat4 stdWorldMatrix = glm::translate(glm::identity<mat4>(), vec3(uiPos.x, -uiPos.y, 0.0f));
    stdWorldMatrix = glm::rotate(stdWorldMatrix, -uiRot, vec3(0.0f, 0.0f, 1.0f));
    stdWorldMatrix = glm::scale(stdWorldMatrix, vec3(uiScale.x, uiScale.y, 1.0f));

    // 스냅 설정
    bool useSnap = m_GizmoUseSnap || ImGui::GetIO().KeyCtrl;
    vec3 snapValues = m_GizmoSnapTranslation;
    if (m_GizmoOperation == ImGuizmo::ROTATE) snapValues = m_GizmoSnapRotation;
    else if (m_GizmoOperation == ImGuizmo::SCALE) snapValues = m_GizmoSnapScale;

    // ─── 4. ImGuizmo 조작 ───
    mat4 deltaMatrix = glm::identity<mat4>();
    ImGuizmo::Manipulate(
        glm::value_ptr(viewMatrix),
        glm::value_ptr(projMatrix),
        m_GizmoOperation,
        ImGuizmo::LOCAL,
        glm::value_ptr(stdWorldMatrix),
        glm::value_ptr(deltaMatrix),
        useSnap ? glm::value_ptr(snapValues) : nullptr
    );

    // ─── 5. Delta 기반 데이터 반영 ───
    if (ImGuizmo::IsUsing())
    {
        ERectTransformFlags flags = transform->GetFlags();

        // 1) 조작된 stdWorldMatrix에서 데이터 추출
        vec2 newStdPos = vec2(stdWorldMatrix[3]);
        float newStdRot = atan2(stdWorldMatrix[0][1], stdWorldMatrix[0][0]);
        vec2 newStdScale = vec2(glm::length(vec3(stdWorldMatrix[0])), glm::length(vec3(stdWorldMatrix[1])));

        // 2) 다시 UI 공간(Y-Down)으로 복원
        vec2 newUiPos = vec2(newStdPos.x, -newStdPos.y);
        float newUiRot = -newStdRot;
        vec2 newUiScale = newStdScale;

        // [이동] Translation 차이 = AnchoredPosition 변화량
        vec2 posDelta = newUiPos - uiPos;
        if (glm::length(posDelta) > 0.0001f)
        {
            vec2 nextPos = transform->GetAnchoredPosition();
            if (!(flags & ERectTransformFlags::LockPositionX)) nextPos.x += posDelta.x;
            if (!(flags & ERectTransformFlags::LockPositionY)) nextPos.y += posDelta.y;
            transform->SetAnchoredPosition(nextPos);
        }

        // [회전] Z축 각도 차이
        float rotDelta = glm::degrees(newUiRot - uiRot);
        while (rotDelta > 180.0f) rotDelta -= 360.0f;
        while (rotDelta < -180.0f) rotDelta += 360.0f;

        if (glm::abs(rotDelta) > 0.001f)
        {
            if (!(flags & ERectTransformFlags::LockRotation))
                transform->SetRotation(transform->GetRotation() + rotDelta);
        }

        // [스케일] Scale 차이
        if (uiScale.x > 0.0001f && uiScale.y > 0.0001f)
        {
            vec2 scaleRatio = newUiScale / uiScale;
            if (glm::abs(scaleRatio.x - 1.0f) > 0.0001f || glm::abs(scaleRatio.y - 1.0f) > 0.0001f)
            {
                vec2 nextScale = transform->GetScale();
                if (!(flags & ERectTransformFlags::LockScale)) {
                    nextScale.x *= scaleRatio.x;
                    nextScale.y *= scaleRatio.y;
                }
                transform->SetScale(nextScale);
            }
        }
    }

    // ─── 6. 커스텀 비주얼: 피벗 마커 ───
    auto ToScreen = [&](const vec2& rtPos) -> ImVec2 {
        vec4 clipPt = projMatrix * viewMatrix * vec4(rtPos.x, -rtPos.y, 0.0f, 1.0f);
        if (std::abs(clipPt.w) < 1e-6f) return ImVec2(-9999, -9999);
        vec3 ndcPt = vec3(clipPt) / clipPt.w;
        return ImVec2(
            m_ImageScreenPos.x + (ndcPt.x * 0.5f + 0.5f) * m_ImageSize.x,
            m_ImageScreenPos.y + (1.0f - (ndcPt.y * 0.5f + 0.5f)) * m_ImageSize.y
        );
    };

    // AbsolutePosition = WorldTransform의 Translation = 피벗의 월드 좌표
    vec2 pivotWorld = transform->GetAbsolutePosition();
    ImVec2 screenPivot = ToScreen(pivotWorld);

    // 피벗 십자 마커 (노란색)
    drawList->AddCircle(screenPivot, 7.0f, IM_COL32(255, 200, 50, 255), 0, 2.0f);
    drawList->AddCircleFilled(screenPivot, 2.5f, IM_COL32(255, 200, 50, 255));
    drawList->AddLine(ImVec2(screenPivot.x - 9, screenPivot.y), ImVec2(screenPivot.x + 9, screenPivot.y), IM_COL32(255, 200, 50, 255), 1.5f);
    drawList->AddLine(ImVec2(screenPivot.x, screenPivot.y - 9), ImVec2(screenPivot.x, screenPivot.y + 9), IM_COL32(255, 200, 50, 255), 1.5f);
}

void UIViewportPanel::DrawCanvasBounds(ImDrawList* drawList, float displayRTWidth, float displayRTHeight, const mat4& view, const mat4& proj)
{
    // 1. 카메라 매트릭스를 기반으로 RT 공간(실제 해상도)을 화면 이미지 크기로 매핑
    const vec2 displayRTToImage(m_ImageSize.x / displayRTWidth, m_ImageSize.y / displayRTHeight);
    const mat4 rtToImageM = glm::scale(glm::identity<mat4>(), vec3(displayRTToImage, 1.0f));

    // Canvas(RT) 영역의 4개 꼭짓점 정의 (원점 0,0 기반으로 폭, 높이만큼)
    vec4 canvasCorners[4] = {
        vec4(0.0f, 0.0f, 0.0f, 1.0f),
        vec4(displayRTWidth, 0.0f, 0.0f, 1.0f),
        vec4(displayRTWidth, displayRTHeight, 0.0f, 1.0f),
        vec4(0.0f, displayRTHeight, 0.0f, 1.0f)
    };

    // 2. 화면 좌표계로 변환 (기즈모 그릴 때 썼던 로직과 동일)
    ImVec2 screenCorners[4];
    bool valid = true;

    for (int i = 0; i < 4; ++i)
    {
        // RT 크기를 이미지 스케일로 맞춘 뒤 proj/view 적용
        vec4 clipPt = proj * view * rtToImageM * canvasCorners[i];
        if (std::abs(clipPt.w) < 1e-6f) { valid = false; break; }

        vec3 ndcPt = vec3(clipPt) / clipPt.w;
        // NDC를 윈도우 스크린 좌표로 변환
        screenCorners[i].x = m_ImageScreenPos.x + (ndcPt.x * 0.5f + 0.5f) * m_ImageSize.x;
        screenCorners[i].y = m_ImageScreenPos.y + (1.0f - (ndcPt.y * 0.5f + 0.5f)) * m_ImageSize.y;
    }

    if (!valid) return;

    // 3. 캔버스 외곽선 렌더링 (하얀색 반투명 또는 회색 점선 효과)
    // 외곽선 색상 (예: UMG 스타일의 약간 어두운 흰색/회색)
    const ImU32 canvasLineColor = IM_COL32(200, 200, 200, 180);
    const float canvasLineThickness = 2.0f;

    // 사각형 그리기
    drawList->AddQuad(screenCorners[0], screenCorners[1], screenCorners[2], screenCorners[3], canvasLineColor, canvasLineThickness);

    // 모서리 강조 마커 (옵션 - Unity 캔버스처럼 네 귀퉁이에 작은 기호 표시)
    const float cornerSize = 10.0f;
    const float cornerThick = 2.0f;
    const ImU32 cornerColor = IM_COL32(255, 255, 255, 220);

    // Top-Left (0)
    drawList->AddLine(screenCorners[0], ImVec2(screenCorners[0].x + cornerSize, screenCorners[0].y), cornerColor, cornerThick);
    drawList->AddLine(screenCorners[0], ImVec2(screenCorners[0].x, screenCorners[0].y + cornerSize), cornerColor, cornerThick);
    // Top-Right (1)
    drawList->AddLine(screenCorners[1], ImVec2(screenCorners[1].x - cornerSize, screenCorners[1].y), cornerColor, cornerThick);
    drawList->AddLine(screenCorners[1], ImVec2(screenCorners[1].x, screenCorners[1].y + cornerSize), cornerColor, cornerThick);
    // Bottom-Right (2)
    drawList->AddLine(screenCorners[2], ImVec2(screenCorners[2].x - cornerSize, screenCorners[2].y), cornerColor, cornerThick);
    drawList->AddLine(screenCorners[2], ImVec2(screenCorners[2].x, screenCorners[2].y - cornerSize), cornerColor, cornerThick);
    // Bottom-Left (3)
    drawList->AddLine(screenCorners[3], ImVec2(screenCorners[3].x + cornerSize, screenCorners[3].y), cornerColor, cornerThick);
    drawList->AddLine(screenCorners[3], ImVec2(screenCorners[3].x, screenCorners[3].y - cornerSize), cornerColor, cornerThick);


    // 4. (추가) 캔버스 바깥 영역 어둡게 처리 (Passepartout 효과)
    // 사용자가 UI 요소가 잘리는 영역을 명확히 인지하게 함
    ImVec2 winMin = m_ImageScreenPos;
    ImVec2 winMax = ImVec2(m_ImageScreenPos.x + m_ImageSize.x, m_ImageScreenPos.y + m_ImageSize.y);
    const ImU32 dimColor = IM_COL32(0, 0, 0, 150); // 어두운 오버레이 색상

    // 상, 하, 좌, 우 4개의 렉트로 분할하여 어둡게 처리
    drawList->AddRectFilled(winMin, ImVec2(winMax.x, screenCorners[0].y), dimColor); // Top
    drawList->AddRectFilled(ImVec2(winMin.x, screenCorners[2].y), winMax, dimColor); // Bottom
    drawList->AddRectFilled(ImVec2(winMin.x, screenCorners[0].y), ImVec2(screenCorners[0].x, screenCorners[2].y), dimColor); // Left
    drawList->AddRectFilled(ImVec2(screenCorners[1].x, screenCorners[0].y), ImVec2(winMax.x, screenCorners[2].y), dimColor); // Right

    // 해상도 텍스트 표시 (예: 1920x1080)
    char resText[32];
    snprintf(resText, sizeof(resText), "Canvas (%.0fx%.0f)", displayRTWidth, displayRTHeight);
    ImVec2 textSize = ImGui::CalcTextSize(resText);
    ImVec2 textPos = ImVec2(screenCorners[0].x + 5.0f, screenCorners[0].y - textSize.y - 2.0f);
    drawList->AddText(textPos, canvasLineColor, resText);
}

void UIViewportPanel::SubmitUIOverlayPass(const wstring& currentRT)
{
    RenderPassManager::Get().GetRenderPassByID(m_UIOverlayPassID)->SetColorAttachments({ currentRT });
}
#pragma endregion
#pragma region Inputs
void UIViewportPanel::KeyboardInput()
{
	if (KEY_PRESSED(EKeyCode::LShift))
	{
		if (KEY_DOWN("1")) m_GizmoOperation = ImGuizmo::OPERATION::TRANSLATE;
		if (KEY_DOWN("2")) m_GizmoOperation = ImGuizmo::OPERATION::ROTATE;
		if (KEY_DOWN("3")) m_GizmoOperation = ImGuizmo::OPERATION::SCALE;
		if (KEY_DOWN(EKeyCode::L)) SelectionManager::Get().TogglePrimarySelectionLock();
	}
}

void UIViewportPanel::MouseInput()
{

    if (m_Hovered)
    {
        InputManager::Get().SetViewportRect(vec4(m_ImageScreenPos.x, m_ImageScreenPos.y, m_ImageSize.x, m_ImageSize.y));
        InputManager::Get().SetTargetResolution(vec2((f32)m_UIDesignWidth, (f32)m_UIDesignHeight));
    }

    const ImVec2 mouse = ImGui::GetMousePos();
    const bool imageHovered =
        (mouse.x >= m_ImageScreenPos.x) && (mouse.y >= m_ImageScreenPos.y) &&
        (mouse.x <= m_ImageScreenPos.x + m_ImageSize.x) && (mouse.y <= m_ImageScreenPos.y + m_ImageSize.y);

    if (!m_Focused || !m_Camera || !m_OwnedCamera->GetTransform()) return;

    // 1) 휠 줌 (진짜 카메라 Ortho Size 조절)
    const float wheel = InputManager::Get().GetMouseScrollDelta().y;
    if (imageHovered && std::abs(wheel) > 0.0001f)
    {
        const float zoomStep = KEY_PRESSED(EKeyCode::LShift) ? 2.0f : 0.5f;
        float currentOrthoSize = m_Camera->GetOrthoSize();
        // OrthoSize가 작아질수록 줌인(확대), 커질수록 줌아웃(축소)
        float newOrthoSize = glm::max(currentOrthoSize - (wheel * zoomStep), 0.1f);
        m_Camera->SetOrthoSize(newOrthoSize);
    }

    // 2) MMB 팬 (진짜 카메라 이동)
    if (imageHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Middle))
    {
        m_IsPanning = true;
        m_PanStartMouse = mouse;
        m_PanStartCameraPos = m_OwnedCamera->GetTransform()->GetWorldPosition();
    }
    if (m_IsPanning && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
    {
        ImVec2 delta(mouse.x - m_PanStartMouse.x, mouse.y - m_PanStartMouse.y);

        // OrthoSize에 비례해서 마우스 드래그 거리를 월드 이동 거리로 환산
        // (현재 창 높이의 절반이 OrthoSize와 매칭됨)
        float moveFactor = m_Camera->GetOrthoSize() / (m_PanelHeight * 0.5f);

        // 드래그 방향과 카메라 이동 방향은 반대 (-delta)
        vec3 newCamPos = m_PanStartCameraPos;
        newCamPos.x -= delta.x * moveFactor;
        newCamPos.y += delta.y * moveFactor; // 2D Ortho에서 화면 위쪽이 +Y

        m_OwnedCamera->GetTransform()->SetPosition(newCamPos);
    }
    if (m_IsPanning && ImGui::IsMouseReleased(ImGuiMouseButton_Middle))
    {
        m_IsPanning = false;
    }
}
Engine::Ray UIViewportPanel::ScreenPosToRay(const ImVec2& mousePos)
{
	f32 localX = mousePos.x - m_ImageScreenPos.x;
	f32 localY = mousePos.y - m_ImageScreenPos.y;

	f32 ndcX = (localX / m_ImageSize.x) * 2.0f - 1.0f;
	f32 ndcY = 1.0f - (localY / m_ImageSize.y) * 2.0f;
	mat4 projInvMatrix = m_Camera->GetProjMatrixInv();
	mat4 viewInvMatrix = m_Camera->GetViewMatrixInv();

	mat4 invVP = viewInvMatrix * projInvMatrix;

	vec4 nearPointNDC = vec4(ndcX, ndcY, -1.0f, 1.0f);
	vec4 farPointNDC = vec4(ndcX, ndcY, 1.0f, 1.0f);

	vec4 nearPointWorld = invVP * nearPointNDC;
	vec4 farPointWorld = invVP * farPointNDC;

	Ray ray;
	ray.Origin = vec3(nearPointWorld) / nearPointWorld.w;
	//ray.Origin.x += m_RenderTarget->GetWidth() / 2.f;
	//ray.Origin.y -= m_RenderTarget->GetHeight() / 2.f;
	ray.Direction = glm::normalize(vec3(farPointWorld / farPointWorld.w) - ray.Origin);

	cout << "Ray Origin: " << ray.Origin.x << ", " << ray.Origin.y << ", " << ray.Origin.z << endl;
	cout << "Ray Direction: " << ray.Direction.x << ", " << ray.Direction.y << ", " << ray.Direction.z << endl;

	return ray;
}
#pragma endregion

#pragma region Canvas

UICanvas* UIViewportPanel::FindCanvasFromObject(GameObject* object) const
{
    GameObject* cur = object;
    while (cur)
    {
        if (UICanvas* canvas = cur->GetComponent<UICanvas>())
            return canvas;
        cur = cur->GetParent();
    }
    return nullptr;
}

UICanvas* UIViewportPanel::ResolveTargetCanvas() const
{
    if (!m_AutoBindSelectedCanvas)
        return m_TargetCanvas;

    GameObject* selected = SelectionManager::Get().GetPrimarySelection();
    if (!selected)
        return nullptr;

    return FindCanvasFromObject(selected);
}

void UIViewportPanel::ClearCanvasLayoutOverride()
{
    if (m_TargetCanvas)
    {
        m_TargetCanvas->ClearExternalLayoutSize(m_LayoutOwnerToken);
        m_TargetCanvas = nullptr;
    }
}

void UIViewportPanel::SyncCanvasLayoutOverride()
{
    Engine::UICanvas* resolved = ResolveTargetCanvas();
    if (resolved != m_TargetCanvas)
    {
        if (m_TargetCanvas)
            m_TargetCanvas->ClearExternalLayoutSize(m_LayoutOwnerToken);
        m_TargetCanvas = resolved;
        if (m_TargetCanvas)
        {
            m_TargetCanvas->SetLayoutRoot(true);
            m_TargetCanvas->SetLayoutSource(ECanvasLayoutSource::ExternalOverride);
        }
    }
    if (!m_TargetCanvas) return;

    // 패널 해상도(m_RenderWidth) 대신 고정된 디자인 해상도를 캔버스에 주입
    const vec2 layoutSize(static_cast<f32>(m_UIDesignWidth), static_cast<f32>(m_UIDesignHeight));
    m_TargetCanvas->SetExternalLayoutSize(layoutSize, m_LayoutOwnerToken);
}
#pragma endregion