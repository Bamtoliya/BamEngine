#pragma once

#include "Editor_Includes.h"
#include "ResourceHandle.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "PreviewViewportWidget.h"

BEGIN(Editor)

enum class EPreviewMeshType : uint8
{
	Sphere,
	Cube,
	Plane,
	Count
};

class MaterialPreviewRenderer
{
public:
	MaterialPreviewRenderer() = default;
	~MaterialPreviewRenderer();

	void Initialize(uint32 width = 512, uint32 height = 512);
	void Free();

	// 매 프레임 프리뷰 씬 업데이트 및 파이프라인 제출
	void Submit(Engine::MaterialInterface* material, f32 dt);

	// ImGui 뷰포트 UI 및 조작 렌더링
	void DrawUI(Engine::MaterialInterface* material);

	[[nodiscard]] Engine::RHITexture* GetPreviewRHITexture() const { return m_Widget.GetPreviewRHITexture(); }

private:
	void UpdatePreviewMesh(Engine::MaterialInterface* material);

private:
	// 공통 3D 프리뷰 위젯 (씬, 카메라 컨트롤러, 디퍼드 파이프라인 내장)
	PreviewViewportWidget m_Widget;

	// 프리뷰 대상 메쉬 엔티티 및 리소스 핸들
	entt::entity m_MeshEntity = entt::null;
	Engine::ResourceHandle<Engine::Mesh> m_SphereMesh;
	Engine::ResourceHandle<Engine::Mesh> m_CubeMesh;
	Engine::ResourceHandle<Engine::Mesh> m_PlaneMesh;

	// 프리뷰 상태
	EPreviewMeshType m_CurrentMeshType = EPreviewMeshType::Sphere;
	bool m_AutoRotate = true;
	f32 m_AutoRotateSpeed = 25.0f; // 초당 회전 각도 (도)
};

END