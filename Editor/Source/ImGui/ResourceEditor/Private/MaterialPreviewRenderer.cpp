#include "MaterialPreviewRenderer.h"
#include "EditorUI.h"
#include "ResourceManager.h"
#include "TransformComponent.h"
#include "StaticMeshRendererComponent.h"
#include "IconsFontAwesome7.h"

namespace Editor
{
	MaterialPreviewRenderer::~MaterialPreviewRenderer()
	{
		Free();
	}

	void MaterialPreviewRenderer::Initialize(uint32 width, uint32 height)
	{
		// 1. 공통 3D 프리뷰 위젯 초기화 (바닥 그리드 기본 On)
		m_Widget.Initialize(L"MatPreview", width, height, true);

		// 2. 프리뷰 메쉬 리소스 로드
		auto& rm = Engine::ResourceManager::Get();
		m_SphereMesh = rm.GetResourceHandle<Engine::Mesh>(L"SphereMesh");
		m_CubeMesh = rm.GetResourceHandle<Engine::Mesh>(L"CubeMesh");
		m_QuadMesh = rm.GetResourceHandle<Engine::Mesh>(L"QuadMesh");
		m_PlaneMesh = rm.GetResourceHandle<Engine::Mesh>(L"PlaneMesh");

		// 3. 프리뷰 대상 메쉬 엔티티 생성 (컴포넌트만 부착하면 위젯이 자동 렌더링)
		auto& entity = m_Widget.CreateEntity(L"MaterialPreviewMesh");
		m_MeshEntity = entity.GetEntityHandle();

		auto& meshComp = m_Widget.GetRegistry().emplace<Engine::StaticMeshRendererComponent>(m_MeshEntity);
		meshComp.meshHandle = m_SphereMesh;
		meshComp.castShadow = true;

		// 4. 카메라 초기 포커스 세팅
		m_Widget.FocusTarget(Engine::vec3(0.0f, 0.0f, 0.0f), 3.0f);
	}

	void MaterialPreviewRenderer::Free()
	{
		m_Widget.Free();
		m_MeshEntity = entt::null;
	}

	void MaterialPreviewRenderer::ReleaseMaterial()
	{
		if (m_MeshEntity == entt::null) return;
		if (auto* mesh = m_Widget.GetRegistry().try_get<Engine::StaticMeshRendererComponent>(m_MeshEntity))
			mesh->materialHandle.Reset();
	}

	void MaterialPreviewRenderer::UpdatePreviewMesh(Engine::MaterialInterface* material)
	{
		if (m_MeshEntity == entt::null) return;

		auto& registry = m_Widget.GetRegistry();
		auto* meshComp = registry.try_get<Engine::StaticMeshRendererComponent>(m_MeshEntity);
		if (!meshComp) return;

		switch (m_CurrentMeshType)
		{
		case EPreviewMeshType::Sphere: meshComp->meshHandle = m_SphereMesh; break;
		case EPreviewMeshType::Cube:   meshComp->meshHandle = m_CubeMesh;   break;
		case EPreviewMeshType::Quad:   meshComp->meshHandle = m_QuadMesh;   break;
		case EPreviewMeshType::Plane:  meshComp->meshHandle = m_PlaneMesh;  break;
		default: break;
		}

		if (material)
		{
			auto handle = Engine::ResourceManager::Get().GetResourceHandle<Engine::MaterialInterface>(material->GetKey());
			if (meshComp->materialHandle != handle)
			{
				meshComp->materialHandle = handle;
			}
		}
	}

	void MaterialPreviewRenderer::Submit(Engine::MaterialInterface* material, f32 dt)
	{
		if (!material || m_MeshEntity == entt::null) return;

		// 1. 머티리얼 및 메쉬 동기화
		UpdatePreviewMesh(material);

		// 2. 메쉬 자전 (자전 시 조명 하이라이트가 회전하면서 PBR 특성 시각화)
		if (m_AutoRotate)
		{
			auto* trans = m_Widget.GetRegistry().try_get<Engine::TransformComponent>(m_MeshEntity);
			if (trans)
			{
				(void)trans->RotateLocal(glm::vec3(0.0f, 1.0f, 0.0f), static_cast<double>(m_AutoRotateSpeed * dt));
			}
		}

		// 3. 위젯 프레임 틱 & 자동 렌더링 제출
		m_Widget.BeginFrame(dt);
		m_Widget.Submit(dt);
	}

	void MaterialPreviewRenderer::ResetView()
	{
		auto* trans = m_Widget.GetRegistry().try_get<Engine::TransformComponent>(m_MeshEntity);
		if (trans) (void)trans->SetEulerRotation(Engine::vec3(0.0f));
		m_Widget.FocusTarget(Engine::vec3(0.0f), 3.0f);
		m_AutoRotate = true;
	}

	void MaterialPreviewRenderer::DrawViewportOnly(const ImVec2& size)
	{
		ImVec2 targetSize = size;
		if (targetSize.x <= 0.0f || targetSize.y <= 0.0f)
		{
			targetSize = ImGui::GetContentRegionAvail();
			targetSize.x = (std::max)(64.0f, targetSize.x);
			targetSize.y = (std::max)(64.0f, targetSize.y);
		}
		m_Widget.Draw(targetSize);
	}

	void MaterialPreviewRenderer::DrawUI(Engine::MaterialInterface* material)
	{
		EditorUI::ToolbarLayout toolbar;
		EditorUI::IconButtonOptions meshOptions;
		meshOptions.Active = m_CurrentMeshType == EPreviewMeshType::Sphere;
		if (toolbar.Icon("Sphere", ICON_FA_GLOBE, "MaterialPreview.Sphere", meshOptions))
			m_CurrentMeshType = EPreviewMeshType::Sphere;

		meshOptions.Active = m_CurrentMeshType == EPreviewMeshType::Cube;
		if (toolbar.Icon("Cube", ICON_FA_CUBE, "MaterialPreview.Cube", meshOptions))
			m_CurrentMeshType = EPreviewMeshType::Cube;

		meshOptions.Active = m_CurrentMeshType == EPreviewMeshType::Quad;
		if (toolbar.Icon("Quad", ICON_FA_SQUARE, "MaterialEditor.Quad", meshOptions))
			m_CurrentMeshType = EPreviewMeshType::Quad;

		meshOptions.Active = m_CurrentMeshType == EPreviewMeshType::Plane;
		if (toolbar.Icon("Plane", ICON_FA_TABLE_CELLS_LARGE, "MaterialPreview.Plane", meshOptions))
			m_CurrentMeshType = EPreviewMeshType::Plane;

		if (toolbar.Icon("AutoRotate", m_AutoRotate ? ICON_FA_PAUSE : ICON_FA_PLAY,
			m_AutoRotate ? "EditorCommon.Pause" : "EditorCommon.Play"))
			m_AutoRotate = !m_AutoRotate;

		if (toolbar.Icon("ResetView", ICON_FA_ROTATE_LEFT, "GraphPanel.ResetView"))
		{
			ResetView();
		}

		ImGui::Separator();

		// 2. 가용 영역 전체에 맞추어 공통 프리뷰 위젯 렌더링
		DrawViewportOnly();
	}
}