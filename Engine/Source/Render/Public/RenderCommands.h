#pragma once

#include "Engine_Includes.h"
#include "ResourceHandle.h"
#include "Structs.h"
#include "MaterialInterface.h"
#include "UIEllipseMaskGeometry.h"
#include "UIAlphaMaskGeometry.h"
#include <optional>

BEGIN(Engine)

// 프레임이 바뀌어도 같은 물체를 가리키는 번호. sceneId가 0이면 프레임 사이의 움직임을 추적하지 않는다.
// 한 엔티티가 여러 드로우를 내는 경우(스플라인 메시의 배치 인스턴스)에는 subId로 드로우를 구분한다.
struct RenderObjectId
{
	uint32 sceneId = 0;
	uint32 entityId = 0;
	uint32 subId = 0;

	bool IsValid() const { return sceneId != 0; }
	// subId가 0이면 (sceneId, entityId)만으로 정해지는 값과 같다.
	uint64 GetKey() const
	{
		return ((static_cast<uint64>(sceneId) << 32u) | entityId) ^ (static_cast<uint64>(subId) * 0x9E3779B97F4A7C15ull);
	}
};

struct HeightfieldDrawData
{
	std::shared_ptr<class RHITexture> heightmap;
	std::shared_ptr<class RHITexture> splatmap;
	std::shared_ptr<class Mesh> gridMesh;
	f32 maxHeight = 0.f;
	vec2 cellSpacing = vec2(1.f);
};

struct StaticDrawCommand
{
	class Mesh* mesh = { nullptr };
	class MaterialInterface* material = { nullptr };
	mat4 worldMatrix = { glm::identity<mat4>() };
	RenderObjectId objectId{};
	// 월드 행렬 밖에서 정점·배치가 바뀌었는지 알리는 값(MotionHistory.h의 MakeMotionStamp). 프레임 사이에 값이 바뀌면
	// 이전 월드 행렬로 모션 벡터를 만들 수 없으므로 그 프레임은 움직임이 없는 것으로 본다. 0이면 형상이 변하지 않는다.
	uint64 motionStamp = 0;
	uint32 pickingId = 0;
	std::optional<BoundingBox> worldBounds{};
	std::optional<HeightfieldDrawData> heightfield{};
	std::shared_ptr<Mesh> ownedMesh;
	// TLAS를 요청한 씬의 그림자 캐스터일 때만 설정한다. 이번 프레임 TLAS에 들어갔는지는 렌더 시점에
	// RaytracingSceneManager::GetTopLevelCasters로 확인한다.
	const class Scene* raytracedScene = { nullptr };
	uint32 raytracedViewID = 0;
	uint32 raytracedEntityIndex = 0;
	// 월드 행렬이 감긴 방향을 뒤집는지. Renderer::SubmitStatic이 정한다.
	bool mirrored = false;
	// false면 G-Buffer 셰이더가 FLAG_RECEIVE_DECALS를 빼고 기록한다(InstanceData pickingData.y = 1).
	bool receiveDecals = true;
};

// 프레임마다 CPU에서 만든 정점으로 그리는 지오메트리 (트레일·리본 등). 정점은 이미 월드 공간이다.
struct DynamicGeometryDrawCommand
{
	vector<VertexPosition> positions;
	vector<VertexMaterial> attributes; // positions와 같은 개수
	class Texture* texture = { nullptr };
	class MaterialInterface* material = { nullptr };
	ETopology topology = ETopology::TriangleStrip;
	glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f }; // 정점 색에 곱해지는 전체 색
	RenderObjectId objectId{};
	uint32 pickingId = 0;
};

struct SkinnedDrawCommand
{
	class Mesh* mesh = { nullptr };
	class MaterialInterface* material = { nullptr };
	MaterialConstants materialConstants{};
	mat4 worldMatrix = { glm::identity<mat4>() };
	const std::vector<mat4>* boneMatrices = { nullptr };
	RenderObjectId objectId{};
	uint32 pickingId = 0;
	// StaticDrawCommand와 같다. TLAS를 요청한 씬의 그림자 캐스터일 때만 설정하고,
	// 이번 프레임 TLAS에 들어갔는지는 렌더 시점에 RaytracingSceneManager::GetTopLevelCasters로 확인한다.
	const class Scene* raytracedScene = { nullptr };
	uint32 raytracedViewID = 0;
	uint32 raytracedEntityIndex = 0;
	// false면 G-Buffer 셰이더가 FLAG_RECEIVE_DECALS를 빼고 기록한다(SceneUBO pickingData.y = 1).
	bool receiveDecals = true;
};

struct SpriteDrawCommand
{
	class Mesh* mesh = { nullptr };
	class Texture* texture = { nullptr };
	class MaterialInterface* material = { nullptr };
	mat4 worldMatrix = { glm::identity<mat4>() };
	glm::vec4 color = { 1.0f, 1.0f, 1.0f, 1.0f };
	glm::vec4 uvTransform = { 0.0f, 0.0f, 1.0f, 1.0f };
	RenderObjectId objectId{};
	uint32 pickingId = 0;
	int32 sortingLayerID = 0;
	int32 orderInLayer = 0;
};

struct UIImageDrawCommand
{
	ResourceHandle<class Texture> textureHandle;
	ResourceHandle<class MaterialInterface> materialHandle;
	mat4 worldMatrix{ 1.0f };
	vec4 color{ 1.0f };
	vec4 uvTransform{ 0.0f, 0.0f, 1.0f, 1.0f };
	vec2 canvasSize{ 1920.0f, 1080.0f };
	std::optional<vec4> clipRect;
	UIEllipseMaskPlanes ellipseMaskPlanes{};
	std::size_t ellipseMaskCount = 0;
	UIAlphaMaskRegions alphaMasks{};
	std::size_t alphaMaskCount = 0;
};
END
