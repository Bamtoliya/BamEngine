#pragma once

#include "System.h"
#include "RenderTypes.h"
#include "StreamingDemand.h"

BEGIN(Engine)
class RenderPass;
class Shader;

class ENGINE_API MeshRenderSystem : public ISystem
{
private:
	struct PassTarget
	{
		RenderPass* pass = nullptr;
		RenderPassID viewID = INVALID_PASS_ID;
		vec3 cameraPos = vec3(0.0f);
		float projY = 1.0f;
		bool hasLODCamera = false;
		size_t viewIndex = SIZE_MAX; // views의 인덱스. 뷰에 속하지 않는 패스는 SIZE_MAX
	};

private:
	MeshRenderSystem() {}
	virtual ~MeshRenderSystem() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static MeshRenderSystem* Create(void* arg = nullptr);
	virtual void Free() override;
public:
	virtual void OnSubmit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;

private:
	vector<PassTarget> m_ScratchTargets;
	vector<RenderPassID> m_ScratchViews;
	vector<StreamingView> m_ScratchStreamingViews;
	vector<int32> m_ScratchViewLODs;
	unordered_map<const Shader*, bool> m_ScratchPlainUVShaders;
};
END