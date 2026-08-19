#pragma once
#include "Engine_Includes.h"
#include "Reflection/ReflectionMacro.h"

BEGIN(Engine)

ENUM()
enum class EEntityFlag : uint8
{
	None = 0,
	Active = 1 << 0,
	Visible = 1 << 1,
	Paused = 1 << 2,
	Dead = 1 << 3,
	Default = Active | Visible,
};

ENABLE_BITMASK_OPERATORS(EEntityFlag)

STRUCT()
struct TagComponent
{
	REFLECT_STRUCT()
	PROPERTY(EDITABLE)
	unordered_set<wstring> Tags;
};

STRUCT()
struct IDComponent
{
	REFLECT_STRUCT()
	PROPERTY(READONLY)
	uint64 ID = 0;
};

STRUCT()
struct NameComponent
{
	REFLECT_STRUCT()
	PROPERTY(EDITABLE)
	wstring Name = L"Entity";
};

STRUCT()
struct FlagComponent
{
	REFLECT_STRUCT()
	PROPERTY(EDITABLE)
	EEntityFlag Flags = EEntityFlag::Default;
};

STRUCT()
struct TransformComponent
{
	REFLECT_STRUCT()
	PROPERTY(EDITABLE)
	vec3 Position = { 0.0f, 0.0f, 0.0f };
	PROPERTY(EDITABLE)
	quat Rotation = { 0.0f, 0.0f, 0.0f, 1.0f };
	PROPERTY(EDITABLE)
	vec3 Scale = { 1.0f, 1.0f, 1.0f };
};

STRUCT()
struct WorldTransformComponent
{
	REFLECT_STRUCT()
	PROPERTY(READONLY)
	mat4 WorldMatrix = mat4(1.0f);
};

STRUCT()
struct HierarchyComponent
{
	REFLECT_STRUCT()
	entt::entity Parent = entt::null;
	std::vector<entt::entity> Children;
};

END