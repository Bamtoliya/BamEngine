#pragma once

#include "Engine_Includes.h"
#include "Base.h"
#include "Structs.h"

BEGIN(Engine)
class ENGINE_API PickingManager final : public Base
{
	DECLARE_SINGLETON(PickingManager)
#pragma region Constructor&Destructor
private:
	PickingManager() {}
	virtual ~PickingManager() = default;
	EResult Initialize(void* arg = nullptr);
public:
	virtual void Free() override;
#pragma endregion

public:
	uint32 RegisterPickingEntity(class Scene* scene, entt::entity entity);
	class Entity* ResolvePickingEntity(uint32 pickingId) const;
	void BeginFrame();
	void EndFrame(bool frameSucceeded);
	void MarkPickingPass(RenderPassID passID) { m_PickingPasses.insert(passID); }
	void ClearState();
	bool HasPickingPass(RenderPassID passID) const { return m_CompletedPickingPasses.contains(passID); }
	uint64 GetPickingFrameSerial() const { return m_PickingFrameSerial; }
	uint64 GetCompletedPickingFrameSerial() const { return m_CompletedPickingFrameSerial; }
	static bool TryGetViewportUV(const vec2& mouse, const vec2& origin, const vec2& size, vec2& outUV);
	static bool TryBuildRay(const mat4& inverseViewProjection, const vec2& uv, Ray& outRay, f32* outMaxDistance = nullptr);
	class Entity* PickEntityByRay(const Ray& ray, f32 maxDistance, HitResult* outHitResult = nullptr, bool testTriangles = true) const;
private:
	struct PickingTarget
	{
		class Scene* scene = nullptr;
		entt::entity entity = entt::null;
		uint64 persistentId = 0;
	};
	void ClearPickingTargets(vector<PickingTarget>& targets);
private:
	vector<PickingTarget> m_PickingTargets;
	vector<PickingTarget> m_CompletedPickingTargets;
	unordered_map<class Scene*, unordered_map<entt::entity, uint32>> m_PickingIds;
	unordered_set<RenderPassID> m_PickingPasses;
	unordered_set<RenderPassID> m_CompletedPickingPasses;
	uint64 m_PickingFrameSerial = 0;
	uint64 m_CompletedPickingFrameSerial = 0;
};
END