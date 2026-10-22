#pragma once

#include "Engine_Includes.h"
#include "ResourceHandle.h"
#include "Structs.h"
#include <optional>
BEGIN(Engine)

struct RenderObjectId
{
	uint32 sceneId = 0;
	uint32 entityId = 0;
};

struct StaticDrawCommand
{
	class Mesh* mesh = { nullptr };
	class MaterialInterface* material = { nullptr };
	mat4 worldMatrix = { glm::identity<mat4>() };
	RenderObjectId objectId{};
	uint32 pickingId = 0;
	std::optional<AABB> worldBounds{};
};

struct SkinnedDrawCommand
{
	class Mesh* mesh = { nullptr };
	class MaterialInterface* material = { nullptr };
	mat4 worldMatrix = { glm::identity<mat4>() };
	const std::vector<mat4>* boneMatrices = { nullptr };
	RenderObjectId objectId{};
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
};

struct UIImageDrawCommand
{
	ResourceHandle<class Texture> textureHandle;
	ResourceHandle<class MaterialInterface> materialHandle;
	mat4 worldMatrix{ 1.0f };
	vec4 color{ 1.0f };
	vec4 uvTransform{ 0.0f, 0.0f, 1.0f, 1.0f };
	vec2 canvasSize{ 1920.0f, 1080.0f };
};
END
