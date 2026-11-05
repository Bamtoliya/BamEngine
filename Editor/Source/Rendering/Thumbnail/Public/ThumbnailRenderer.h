#pragma once

#include "Editor_Includes.h"
#include "ResourceHandle.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "ViewportDeferredPipeline.h"

BEGIN(Engine)
class Scene;
class RHITexture;
class Model;
END

BEGIN(Editor)

class ThumbnailRenderer : public Base
{
	DECLARE_SINGLETON(ThumbnailRenderer)

public:
	ThumbnailRenderer() = default;
	virtual ~ThumbnailRenderer() = default;

	EResult Initialize(void* arg = nullptr);
	void Free() override;

	// 128x128 오프스크린 뷰포트 파이프라인 및 스튜디오 씬 초기화
	void Setup(uint32 size = 128);

	// 머티리얼 1회 렌더링 제출 (구체 메쉬에 머티리얼을 바인딩하고 오프스크린 파이프라인에 Submit)
	bool SubmitMaterial(Engine::MaterialInterface* material);

	// 단일 메쉬 1회 렌더링 제출 (AABB 자동 프레이밍, 머티리얼 미지정 시 기본 PBR 적용)
	bool SubmitMesh(Engine::Mesh* mesh, Engine::MaterialInterface* material = nullptr);
	// 복합 모델 1회 렌더링 제출 (다중 서브메쉬, 노드 계층, 통합 AABB 자동 프레이밍)
	bool SubmitModel(Engine::Model* model);

	// 최종 조명 합성이 완료된 128x128 렌더 타깃 텍스처 조회
	[[nodiscard]] Engine::RHITexture* GetFinalTexture() const;

	// 현재 렌더 타깃의 이미지를 복제한 독립 RHITexture 인스턴스 생성 (AssetCache 보관용)
	[[nodiscard]] Engine::RHITexture* CreateThumbnailTexture() const;

private:
	void SetupStudioScene();
	void RefreshStudioLights();
	[[nodiscard]] CameraBuffer CalculateAutoFramingCamera(const vec3& bMin, const vec3& bMax) const;
	[[nodiscard]] Engine::MaterialInterface* GetDefaultPreviewMaterial() const;
private:
	uint32 m_Size = 128;
	bool m_Initialized = false;

	// 128x128 오프스크린 디퍼드 렌더링 파이프라인
	ViewportDeferredPipeline m_Pipeline;

	// 프리뷰 전용 독립 씬 및 엔티티
	Engine::Scene* m_Scene = nullptr;
	entt::entity m_MeshEntity = entt::null;

	// 기본 프리뷰 메쉬 (Sphere)
	Engine::ResourceHandle<Engine::Mesh> m_SphereMesh;

	// 빛
	std::array<entt::entity, 3> m_StudioLights{ entt::null, entt::null, entt::null };
};

END