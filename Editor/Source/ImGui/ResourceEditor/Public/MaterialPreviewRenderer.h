#pragma once

#include "Editor_Includes.h"
#include "ResourceHandle.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "RenderTarget.h"
#include "RenderPass.h"

BEGIN(Engine)
class Mesh;
class MaterialInterface;
class RenderTarget;
class RenderPass;
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

	void Initialize(uint32 width = 256, uint32 height = 256);
	void Free();

	// 매 프레임 프리뷰 렌더링 커맨드 제출
	void Submit(Engine::MaterialInterface* material, f32 dt);

	// ImGui에 프리뷰 뷰포트 및 컨트롤 UI 그리기
	void DrawUI(Engine::MaterialInterface* material);

	[[nodiscard]] Engine::RHITexture* GetPreviewRHITexture() const;

private:
	void LoadPreviewMeshes();

private:
	uint32 m_Width = 256;
	uint32 m_Height = 256;

	Engine::RenderTarget* m_ColorRT = nullptr;
	Engine::RenderTarget* m_DepthRT = nullptr;
	Engine::RenderPass* m_RenderPass = nullptr;
	Engine::RenderPassID m_RenderPassID = 0;

	// 프리뷰 메쉬들
	Engine::ResourceHandle<Engine::Mesh> m_SphereMesh;
	Engine::ResourceHandle<Engine::Mesh> m_CubeMesh;
	Engine::ResourceHandle<Engine::Mesh> m_PlaneMesh;
	EPreviewMeshType m_CurrentMeshType = EPreviewMeshType::Sphere;

private:
	// Camera 속성
	vec3 m_Target = vec3(0.0f);
	f32 m_Yaw = 0.5f;
	f32 m_Pitch = 0.25f;
	f32 m_Distance = 3.5f;
	f32 m_MoveSpeed = 3.0f;
	bool m_AutoRotate = true;
	f32 m_AutoRotateSpeed = 0.5f;
};

END