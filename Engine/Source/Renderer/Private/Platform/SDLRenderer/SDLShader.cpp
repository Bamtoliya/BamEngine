#pragma once

#include "SDLShader.h"
#include "SDLBuffer.h"
#include "Vertex.h"

#pragma region Constructor&Destructor
EResult SDLShader::Initialize(void* arg)
{
	if (!arg) return EResult::InvalidArgument;
	DESC* desc = reinterpret_cast<DESC*>(arg);
	m_ShaderType = desc->ShaderType;
	return EResult::Success;
}

SDLShader* SDLShader::Create(void* arg)
{
	SDLShader* shader = new SDLShader{};
	if (shader->Initialize(arg) != EResult::Success)
	{
		shader->Release();
		return nullptr;
	}
	return shader;
}

void SDLShader::Free()
{
	RHIShader::Free();
}

#pragma endregion

#pragma region Process Vertex
SDL_Vertex* SDLShader::ProcessVertex(const SDLBuffer* vertexBuffer, const glm::mat4& worldMatrix, glm::vec4& matColor, glm::vec2& screenInfo)
{
	if (!vertexBuffer) return nullptr;

	const void* inputData = vertexBuffer->GetNativeHandle();
	uint32 stride = vertexBuffer->GetStride();
	uint32 bufferSize = vertexBuffer->GetSize();

	if (stride == 0) return nullptr; 

	uint32 vertexCount = bufferSize / stride;

	if (m_CacheVertices.size() < vertexCount)
	{
		m_CacheVertices.reserve(vertexCount * 2);
	}
	m_CacheVertices.resize(vertexCount);

	const uint8* byteData = reinterpret_cast<const uint8*>(inputData);

	f32 scale = 100.0f;

	for (uint32 i = 0; i < vertexCount; ++i)
	{
		const Vertex* src = reinterpret_cast<const Vertex*>(byteData + (i * stride));
		glm::vec4 worldPos = worldMatrix * glm::vec4(src->position, 1.0f);
		m_CacheVertices[i].position.x = (worldPos.x * scale) + screenInfo.x;
		m_CacheVertices[i].position.y = (-worldPos.y * scale) + screenInfo.y;
		glm::vec4 finalColor = src->color * matColor;

		m_CacheVertices[i].color.r = static_cast<Uint8>(finalColor.r * 255.0f);
		m_CacheVertices[i].color.g = static_cast<Uint8>(finalColor.g * 255.0f);
		m_CacheVertices[i].color.b = static_cast<Uint8>(finalColor.b * 255.0f);
		m_CacheVertices[i].color.a = static_cast<Uint8>(finalColor.a * 255.0f);

		m_CacheVertices[i].tex_coord.x = src->texCoord.x;
		m_CacheVertices[i].tex_coord.y = src->texCoord.y;
	}

	return m_CacheVertices.data();
}
#pragma endregion

