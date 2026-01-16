
#pragma once
namespace Engine
{
	struct Vertex
	{
		glm::vec3 position = {};
		glm::vec3 normal = {};
		glm::vec2 texCoord = {};
		glm::vec3 tangent = {};
		glm::vec3 bitangent = {};
	};

	struct Vertex2D
	{
		glm::vec3 position = {};
		glm::vec4 color = {};
		glm::vec2 texCoord = {};
	};
}