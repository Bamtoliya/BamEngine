#pragma once

#include "Types.h"
#include "RenderTypes.h"
#include "ReflectionMacro.h"

namespace Engine
{
#pragma region Vertex
	STRUCT()
	struct ENGINE_API VertexPosition
	{
		REFLECT_STRUCT();
		PROPERTY()
		glm::vec3 position = {};
		VertexPosition() = default;
		VertexPosition(const glm::vec3& pos)
			: position(pos)
		{
		}
		static const tagInputLayoutDesc Layout;
	};

	STRUCT()
		struct ENGINE_API VertexMaterial
	{
		REFLECT_STRUCT();
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
		VertexMaterial() = default;
		VertexMaterial(const glm::vec3& nor, const glm::vec2& uv, const glm::vec3& tan, const glm::vec3& bitan, const glm::vec4& col)
			: normal(nor), texCoord(uv), tangent(tan), bitangent(bitan), color(col) {
		}
		static const tagInputLayoutDesc Layout;
	};

	struct VertexSkinData
	{
		glm::uvec4 boneIDs;
		glm::vec4 weights;
		static const tagInputLayoutDesc Layout;
	};

	STRUCT()
		struct ENGINE_API Vertex
	{
		REFLECT_STRUCT();

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

		static const tagInputLayoutDesc Layout;
	};

	struct Vertex2D
	{
		glm::vec3 position = {};
		glm::vec4 color = {};
		glm::vec2 texCoord = {};

		static const tagInputLayoutDesc Layout;
	};



#ifdef _DEBUG
	struct ENGINE_API DebugVertex
	{
		vec3 position;
		vec4 color;

		static const tagInputLayoutDesc Layout;
	};
#endif
	struct ENGINE_API SceneUBO
	{
		mat4 worldMatrix;
	};
#pragma endregion
}