#pragma once

#include "Editor_Includes.h"
#include "ResourceHandle.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "ViewportDeferredPipeline.h"
#include <filesystem>
#include <optional>
#include "PostProcessPipeline.h"

BEGIN(Engine)
class Scene;
class RHITexture;
class Model;
struct BoundingBox;
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

	// 머티리얼 1회 렌더링 제출 (구체 메쉬에 머티리얼을 바인딩하고 오프스크린 파이프라인에 Submit)
	bool SubmitMaterial(Engine::MaterialInterface* material);

	// 단일 메쉬 1회 렌더링 제출 (AABB 자동 프레이밍, 머티리얼 미지정 시 기본 PBR 적용)
	bool SubmitMesh(Engine::Mesh* mesh, Engine::MaterialInterface* material = nullptr);
	// 복합 모델 1회 렌더링 제출 (다중 서브메쉬, 노드 계층, 통합 AABB 자동 프레이밍)
	bool SubmitModel(Engine::Model* model);

	// 프리팹 1회 렌더링 제출 (인-프로세스 폴백용)
	bool SubmitPrefab(const std::filesystem::path& prefabPath);

	// 최종 조명 합성이 완료된 128x128 렌더 타깃 텍스처 조회
	[[nodiscard]] Engine::RHITexture* GetFinalTexture() const;

	// 현재 렌더 타깃의 이미지를 복제한 독립 RHITexture 인스턴스 생성 (AssetCache 보관용)
	[[nodiscard]] Engine::RHITexture* CreateThumbnailTexture() const;

private:
	void SetupStudioScene();
	void RefreshStudioLights();
	[[nodiscard]] std::optional<CameraBuffer> CalculateAutoFramingCamera(const Engine::BoundingBox& bounds) const;
	[[nodiscard]] Engine::MaterialInterface* GetDefaultPreviewMaterial() const;
	void SubmitFrame(const CameraBuffer& camera);
private:
	uint32 m_Size = 128;

	// 128x128 오프스크린 디퍼드 렌더링 파이프라인
	ViewportDeferredPipeline m_Pipeline;

	// 썸네일 전용 후처리 (기본 구성: ACES 톤매핑 + 감마만 활성)
	Engine::PostProcessPipeline m_PostProcess;
	std::wstring m_OutputRTName;

	// 프리뷰 전용 독립 씬 및 엔티티
	Engine::Scene* m_Scene = nullptr;
	entt::entity m_MeshEntity = entt::null;

	// 기본 프리뷰 메쉬 (MaterialBall 우선, 실패 시 Sphere 폴백)
	Engine::ResourceHandle<Engine::Mesh> m_MaterialBallMesh;
	Engine::ResourceHandle<Engine::Mesh> m_SphereMesh;

	// 빛
	std::array<entt::entity, 3> m_StudioLights{ entt::null, entt::null, entt::null };

	// 스킨드 썸네일 제출 시 비동기 렌더링 수명을 보장하기 위한 바인드 포즈 행렬 버퍼
	std::vector<mat4> m_BindPoseMatrices;

	// 메쉬·모델 썸네일 기본 머티리얼 (DefaultMaterial 상속 인스턴스, ResourceManager 소유)
	Engine::ResourceHandle<Engine::MaterialInterface> m_PreviewMaterial;
};

END
