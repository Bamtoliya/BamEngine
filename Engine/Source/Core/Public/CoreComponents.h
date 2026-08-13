#pragma once
#include "Engine_Includes.h"

struct TagComponent
{
	std::string Tag = "Entity";
};

struct IDComponent
{
	uint64 ID = 0;
};

STRUCT()
struct TransformComponent
{
	vec3 Position = { 0.0f, 0.0f, 0.0f };
	quat Rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
	vec3 Scale = { 1.0f, 1.0f, 1.0f };
};

struct WorldTransformComponent
{
	mat4 WorldMatrix = mat4(1.0f);
};

struct ReleationShipComponent
{
	entt::entity Parent = entt::null;
	std::vector<entt::entity> Children;
};