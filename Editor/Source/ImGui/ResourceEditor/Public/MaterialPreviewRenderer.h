#pragma once

#include "Editor_Includes.h"
#include "ResourceHandle.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "ViewportDeferredPipeline.h"

BEGIN(Engine)
class Scene;
class Entity;
END

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

	[[nodiscard]] Engine::RHITexture* GetPreviewRHITexture() const;

private:
	void SetupPreviewScene();
	void UpdatePreviewMesh(Engine::MaterialInterface* material);

private:
	uint32 m_Width = 512;
	uint32 m_Height = 512;

	// 독립 뷰포트 디퍼드 파이프라인
	ViewportDeferredPipeline m_Pipeline;

	// 프리뷰 전용 독립 씬 및 엔티티
	Engine::Scene* m_PreviewScene = nullptr;
	entt::entity m_MeshEntity = entt::null;
	entt::entity m_LightEntity = entt::null;

	// 프리뷰 메쉬 핸들
	Engine::ResourceHandle<Engine::Mesh> m_SphereMesh;
	Engine::ResourceHandle<Engine::Mesh> m_CubeMesh;
	Engine::ResourceHandle<Engine::Mesh> m_PlaneMesh;
	EPreviewMeshType m_CurrentMeshType = EPreviewMeshType::Sphere;

	// 카메라 속성
	vec3 m_Target = vec3(0.0f);
	f32 m_Yaw = 0.5f;
	f32 m_Pitch = 0.25f;
	f32 m_Distance = 3.5f;
	f32 m_MoveSpeed = 3.0f;
	bool m_AutoRotate = true;
	f32 m_AutoRotateSpeed = 0.5f;
};

END