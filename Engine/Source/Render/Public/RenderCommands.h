#pragma once

#include "Engine_Includes.h"
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
};
END