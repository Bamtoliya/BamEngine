#pragma once

#include "Engine_Includes.h"
#include "System.h"
#include "CameraBuffer.h"
#include "Light.h"

BEGIN(Engine)

class Scene;
struct CompositeVFXComponent;
class ENGINE_API CompositeVFXSystem : public ISystem
{
private:
	CompositeVFXSystem() = default;
	virtual ~CompositeVFXSystem() = default;
	EResult Initialize(void* arg = nullptr);

public:
	static CompositeVFXSystem* Create(void* arg = nullptr);
	virtual void Free() override;

public:
	virtual void OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;
	virtual void OnSubmit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;

	// 독립 씬(이펙트 에디터 프리뷰 뷰포트 등)용 시뮬레이션 및 렌더링 제출 함수
	static void SimulateSceneVFX(Scene* scene, f32 dt);
	static void SubmitSceneVFX(Scene* scene, const CameraBuffer& cameraBuffer, uint32 targetPassId);
	static void RefreshFlashLight(CompositeVFXComponent& vfxComp);
	static uint32 CollectFlashLights(const Scene& scene, std::span<GPULight> output);

private:
	static void UpdateVFXEntity(Scene* scene, entt::entity entity, struct CompositeVFXComponent& vfxComp, const struct WorldTransformComponent& transform, f32 dt);

	virtual uint8 GetExecutionPhases() const override { return static_cast<uint8>(ESystemPhase::All); }
};

END
