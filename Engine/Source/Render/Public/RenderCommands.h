#pragma once

#include "Engine_Includes.h"
#include "ResourceHandle.h"
#include "Structs.h"
#include "MaterialInterface.h"
#include "UIEllipseMaskGeometry.h"
#include <optional>

BEGIN(Engine)

struct RenderObjectId
{
	uint32 sceneId = 0;
	uint32 entityId = 0;
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
	uint32 pickingId = 0;
	std::optional<BoundingBox> worldBounds{};
	std::optional<HeightfieldDrawData> heightfield{};
	std::shared_ptr<Mesh> ownedMesh;
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
};
END
