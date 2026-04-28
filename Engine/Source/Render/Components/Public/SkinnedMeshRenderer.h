#pragma once

#include "MeshRenderer.h"


BEGIN(Engine)

CLASS()
class ENGINE_API SkinnedMeshRenderer final : public MeshRenderer
{
	REFLECT_CLASS();
#pragma region Constructor&Destructor
private:
	SkinnedMeshRenderer() : MeshRenderer{} {}
	virtual ~SkinnedMeshRenderer() {}
	virtual EResult Initialize(void* arg = nullptr) override;
public:
	static SkinnedMeshRenderer* Create(void* arg = nullptr);
	virtual SkinnedMeshRenderer* Clone(GameObject* owner, void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Render
public:
	virtual EResult	Render(f32 dt, RenderPass* renderPass = nullptr)override;
#pragma endregion

#pragma region Setter

#pragma endregion

#pragma region Variable
private:
	RHIBuffer* m_BoneSSBO = nullptr;
	uint32 m_LastBoneCount = 0;
#pragma endregion
};
END