#include "DebugDraw.h"

#if !BAM_SHIPPING
#include <algorithm>
#include <mutex>

BEGIN(Engine)

namespace
{
	std::mutex s_DebugDrawMutex;
	std::vector<DebugDrawCommand> s_DebugDrawCommands;

	void EnqueueDebugDrawCommand(DebugDrawCommand&& command)
	{
		std::lock_guard<std::mutex> lock(s_DebugDrawMutex);
		s_DebugDrawCommands.push_back(std::move(command));
	}
}

void DebugDraw::DrawLine(const vec3& start, const vec3& end, const vec4& color, f32 duration)
{
	DebugDrawCommand cmd;
	cmd.Shape = EDebugDrawShape::Line;
	cmd.Start = start;
	cmd.End = end;
	cmd.Color = color;
	cmd.RemainingDuration = duration;
	EnqueueDebugDrawCommand(std::move(cmd));
}

void DebugDraw::DrawBox(const vec3& center, const vec3& halfExtent, const vec4& color, const mat4& transform, f32 duration)
{
	DebugDrawCommand cmd;
	cmd.Shape = EDebugDrawShape::Box;
	cmd.Start = center;
	cmd.End = halfExtent;
	cmd.Color = color;
	cmd.Transform = transform;
	cmd.RemainingDuration = duration;
	EnqueueDebugDrawCommand(std::move(cmd));
}

void DebugDraw::DrawSphere(const vec3& center, f32 radius, const vec4& color, f32 duration)
{
	DebugDrawCommand cmd;
	cmd.Shape = EDebugDrawShape::Sphere;
	cmd.Start = center;
	cmd.Radius = radius;
	cmd.Color = color;
	cmd.RemainingDuration = duration;
	EnqueueDebugDrawCommand(std::move(cmd));
}

void DebugDraw::DrawArrow(const vec3& start, const vec3& end, const vec4& color, f32 headSize, f32 duration)
{
	DebugDrawCommand cmd;
	cmd.Shape = EDebugDrawShape::Arrow;
	cmd.Start = start;
	cmd.End = end;
	cmd.Color = color;
	cmd.Radius = headSize;
	cmd.RemainingDuration = duration;
	EnqueueDebugDrawCommand(std::move(cmd));
}

void DebugDraw::ConsumeCommands(f32 dt, std::vector<DebugDrawCommand>& out)
{
	out.clear();
	const f32 elapsed = std::max(0.0f, dt);

	std::lock_guard<std::mutex> lock(s_DebugDrawMutex);
	if (s_DebugDrawCommands.empty())
		return;

	out.reserve(s_DebugDrawCommands.size());
	std::vector<DebugDrawCommand> retained;
	retained.reserve(s_DebugDrawCommands.size());

	for (auto& cmd : s_DebugDrawCommands)
	{
		out.push_back(cmd);
		if (cmd.RemainingDuration > 0.0f)
		{
			cmd.RemainingDuration -= elapsed;
			if (cmd.RemainingDuration > 0.0f)
				retained.push_back(cmd);
		}
	}

	s_DebugDrawCommands.swap(retained);
}

void DebugDraw::Clear()
{
	std::lock_guard<std::mutex> lock(s_DebugDrawMutex);
	s_DebugDrawCommands.clear();
}

END
#endif
