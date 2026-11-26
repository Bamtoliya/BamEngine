#pragma once

#include "Engine_API.h"
#include "Macro.h"
#include "Types.h"
#include <vector>

BEGIN(Engine)

enum class EDebugDrawShape : uint8
{
	Line,
	Box,
	Sphere,
	Arrow
};

struct DebugDrawCommand
{
	EDebugDrawShape Shape = EDebugDrawShape::Line;
	vec3 Start{ 0.0f };
	vec3 End{ 0.0f };
	vec4 Color{ 1.0f };
	mat4 Transform{ 1.0f };
	f32 Radius = 0.0f;
	f32 RemainingDuration = 0.0f;
};

class DebugDraw final
{
public:
	DebugDraw() = delete;

#if BAM_SHIPPING
	inline static void DrawLine(const vec3&, const vec3&, const vec4& = vec4(1.0f), f32 = 0.0f) {}
	inline static void DrawBox(const vec3&, const vec3&, const vec4& = vec4(1.0f), const mat4& = mat4(1.0f), f32 = 0.0f) {}
	inline static void DrawSphere(const vec3&, f32, const vec4& = vec4(1.0f), f32 = 0.0f) {}
	inline static void DrawArrow(const vec3&, const vec3&, const vec4& = vec4(1.0f), f32 = 0.25f, f32 = 0.0f) {}
	inline static void ConsumeCommands(f32, std::vector<DebugDrawCommand>& out) { out.clear(); }
	inline static void Clear() {}
#else
	ENGINE_API static void DrawLine(const vec3& start, const vec3& end, const vec4& color = vec4(1.0f), f32 duration = 0.0f);
	ENGINE_API static void DrawBox(const vec3& center, const vec3& halfExtent, const vec4& color = vec4(1.0f), const mat4& transform = mat4(1.0f), f32 duration = 0.0f);
	ENGINE_API static void DrawSphere(const vec3& center, f32 radius, const vec4& color = vec4(1.0f), f32 duration = 0.0f);
	ENGINE_API static void DrawArrow(const vec3& start, const vec3& end, const vec4& color = vec4(1.0f), f32 headSize = 0.25f, f32 duration = 0.0f);
	ENGINE_API static void ConsumeCommands(f32 dt, std::vector<DebugDrawCommand>& out);
	ENGINE_API static void Clear();
#endif
};

END
