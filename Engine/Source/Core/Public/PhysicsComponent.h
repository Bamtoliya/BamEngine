#pragma once

#include "Engine_Includes.h"

struct RigidBodyComponent
{
	enum class ERigidBodyType
	{
		Static,
		Dynamic,
		Kinematic
	} BodyType = ERigidBodyType::Static;

	f32 Mass = 1.f;
	f32 LinearDrag = 0.f;
	f32 AngularDrag = 0.f;

	bool UseGravity = true;
	bool IsKinematic = false;
	bool FixedRotation = false;
	void* RuntimeBody = nullptr;
};

struct Box2DColliderComponent
{
	vec2 Size = vec2(1.f);
	vec2 Offset = vec2(0.f);
	bool IsTrigger = false;
};

struct BoxColliderComponent
{
	vec3 Size = vec3(1.f);
	vec3 Offset = vec3(0.f);
	bool IsTrigger = false;
};