#pragma once

#include <ReflectionMacro.h>
namespace Engine
{

	STRUCT()
	struct Vertex
	{
		REFLECT_STRUCT(Vertex);

		PROPERTY()
		glm::vec3 position = {};
		PROPERTY()
		glm::vec3 normal = {};
		PROPERTY()
		glm::vec2 texCoord = {};
		PROPERTY()
		glm::vec3 tangent = {};
		PROPERTY()
		glm::vec3 bitangent = {};
		PROPERTY()
		glm::vec4 color = { 1.f, 1.f, 1.f, 1.f };

		Vertex() = default;
		Vertex(const glm::vec3& pos, const glm::vec3& norm, const glm::vec2& uv)
			: position(pos), normal(norm), texCoord(uv)
		{
		}

		Vertex(const glm::vec3& pos, const glm::vec2& uv)
			: position(pos), texCoord(uv)
		{
		}

		Vertex(
			const glm::vec3& pos,
			const glm::vec3& nor,
			const glm::vec2& uv,
			const glm::vec3& tan)
			: position(pos), normal(nor), texCoord(uv), tangent(tan)
		{

		}

		Vertex(
			const glm::vec3& pos,
			const glm::vec3& nor,
			const glm::vec2& uv,
			const glm::vec3& tan,
			const glm::vec3& bitan,
			const glm::vec4& col)
			: position(pos), normal(nor), texCoord(uv), tangent(tan), bitangent(bitan), color(col)
		{
		}
	};

	struct Vertex2D
	{
		glm::vec3 position = {};
		glm::vec4 color = {};
		glm::vec2 texCoord = {};
	};
}