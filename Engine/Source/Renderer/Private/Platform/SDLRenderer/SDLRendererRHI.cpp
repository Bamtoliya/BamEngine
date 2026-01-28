#pragma once

#include "SDLRendererRHI.h"
#include "SDLBuffer.h"
#include "SDLShader.h"
#include "SDLTexture.h"
#include "SDLPipeline.h"
#include "Vertex.h"

#pragma region Constructor&Destructor
EResult SDLRendererRHI::Initialize(void* arg)
{
	RHICREATEINFO* pDesc = reinterpret_cast<RHICREATEINFO*>(arg);
	m_Window = reinterpret_cast<SDL_Window*>(pDesc->WindowHandle);
	m_Renderer = SDL_CreateRenderer(m_Window, nullptr);
	if (!m_Window || !m_Renderer) return EResult::Fail;

	int32 width, height;
	SDL_GetWindowSize(m_Window, &width, &height);
	m_BackBuffer = SDL_CreateTexture(m_Renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
	if (!m_BackBuffer)
	{
		return EResult::Fail;
	}
	return EResult::Success;
}

RHI* SDLRendererRHI::Create(void* arg)
{
	SDLRendererRHI* Instance = new SDLRendererRHI();
	if (IsFailure(Instance->Initialize(arg)))
	{
		delete Instance;
		Instance = nullptr;
		fmt::print(stderr, "SDLRendererRHI Creation Failed\n");
	}
	return Instance;
}

void SDLRendererRHI::Free()
{
	if (m_BackBuffer)
	{
		SDL_DestroyTexture(m_BackBuffer);
		m_BackBuffer = nullptr;
	}
	if (m_Renderer)
	{
		SDL_DestroyRenderer(m_Renderer);
		m_Renderer = nullptr;
	}
}
#pragma endregion

#pragma region Frame
EResult SDLRendererRHI::BeginFrame()
{
	if (m_Renderer)
	{
		//SDL_SetRenderTarget(m_Renderer, m_BackBuffer);
		SDL_RenderClear(m_Renderer);
		SDL_SetRenderDrawColor(m_Renderer, 100, 100, 100, 255);
		return EResult::Success;
	}
	return EResult::Fail;
}


EResult SDLRendererRHI::EndFrame()
{
	if (m_Renderer)
	{
		SDL_RenderPresent(m_Renderer);
		return EResult::Success;
	}
	return EResult::Fail;
}
#pragma endregion

#pragma region Create Resources
RHIBuffer* SDLRendererRHI::CreateBuffer(void* data, uint32 size, uint32 stride, ERHIBufferType type)
{
	SDLBuffer* buffer = new SDLBuffer(type, size, stride);
	if (data)
	{
		memcpy(buffer->m_Data.data(), data, size);
	}
	return buffer;
}

RHIBuffer* SDLRendererRHI::CreateVertexBuffer(void* data, uint32 size, uint32 stride)
{
	return CreateBuffer(data, size, stride, ERHIBufferType::Vertex);
}

RHIBuffer* SDLRendererRHI::CreateIndexBuffer(void* data, uint32 size, uint32 stride)
{
	return CreateBuffer(data, size, stride, ERHIBufferType::Index);
}

RHITexture* SDLRendererRHI::CreateTextureFromFile(const char* filename)
{
	SDL_Surface* surface = SDL_LoadBMP(filename);
	if (!surface)
	{
		return nullptr;
	}

	SDLTexture* texture = new SDLTexture(surface->w, surface->h, 1, 1);
	texture->m_Texture = SDL_CreateTextureFromSurface(m_Renderer, surface);
	SDL_DestroySurface(surface);

	if (!texture->m_Texture)
	{
		delete texture;
		texture = nullptr;
		return nullptr;
	}
	return texture;
}

RHITexture* SDLRendererRHI::CreateTextureFromFile(const wchar* filename)
{
	string strFileName = WStrToStr(filename);
	SDL_Surface* surface = SDL_LoadBMP(strFileName.c_str());
	if (!surface)
	{
		return nullptr;
	}

	SDLTexture* texture = new SDLTexture(surface->w, surface->h, 1, 1);
	texture->m_Texture = SDL_CreateTextureFromSurface(m_Renderer, surface);
	SDL_DestroySurface(surface);

	if (!texture->m_Texture)
	{
		delete texture;
		texture = nullptr;
		return nullptr;
	}
	return texture;
}

RHITexture* SDLRendererRHI::CreateTextureFromMemory(void* data, uint32 size)
{
	return nullptr;
}

RHITexture* SDLRendererRHI::CreateTexture2D(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize)
{
	SDLTexture* texture = new SDLTexture(width, height, mipLevels, arraySize);
	texture->m_Texture = SDL_CreateTexture(m_Renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, width, height);

	if (texture->m_Texture && data)
	{
		SDL_UpdateTexture(texture->m_Texture, nullptr, data, width * 4);
	}

	return texture;
}

RHITexture* SDLRendererRHI::CreateTextureCube(void* data, uint32 size, uint32 mipLevels)
{
	return nullptr;
}

RHITexture* SDLRendererRHI::CreateTexture3D(void* data, uint32 width, uint32 height, uint32 depth, uint32 mipLevels)
{
	return nullptr;
}

RHITexture* SDLRendererRHI::CreateRenderTargetTexture(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize)
{
	SDLTexture* texture = new SDLTexture(width, height, mipLevels, arraySize);
	texture->m_Texture = SDL_CreateTexture(m_Renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);

	return texture;
}

RHITexture* SDLRendererRHI::CreateDepthStencilTexture(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize)
{
	return nullptr;
}

RHITexture* SDLRendererRHI::CreateTextureFromNativeHandle(void* nativeHandle)
{
	if (!nativeHandle) return nullptr;
	SDL_Texture* NativeTexture = reinterpret_cast<SDL_Texture*>(nativeHandle);
	SDLTexture* texture = new SDLTexture(NativeTexture->w, NativeTexture->h, 1, 1);
	texture->m_Texture = NativeTexture;
	return texture;
}

#pragma endregion


#pragma region Bind
EResult SDLRendererRHI::BindShader(RHIShader* shader)
{
	if (!shader) return EResult::InvalidArgument;
	m_CurrentShader = static_cast<RHIShader*>(shader);
	return EResult::Success;
}
EResult SDLRendererRHI::BindConstantBuffer(void* arg, uint32 slot)
{
	RHIBuffer* buffer = reinterpret_cast<RHIBuffer*>(arg);
	if (!buffer) return EResult::InvalidArgument;

	void* data = buffer->GetNativeHandle();

	if (slot == 0)
	{
		memcpy(&m_WorldMatrix, data, sizeof(mat4));
	}
	else if (slot == 1)
	{
		memcpy(&m_MaterialColor, data, sizeof(vec4));
	}

	return EResult::Success;
}
#pragma endregion


#pragma region Draw
EResult SDLRendererRHI::Draw(uint32 count)
{
	if (!m_Renderer || !m_VertexBuffer) return EResult::Fail;

	SDLBuffer* vertexBuffer = static_cast<SDLBuffer*>(m_VertexBuffer);
	if (!vertexBuffer) return EResult::Fail;

	SDL_Vertex* vertices = reinterpret_cast<SDL_Vertex*>(vertexBuffer->GetNativeHandle());

	// For demonstration, we will just draw points
	const uint32 stride = m_VertexBuffer->GetStride();
	if (!SDL_RenderGeometry(m_Renderer, nullptr, vertices, count, nullptr, 0))
	{
		return EResult::Fail;
	}

	return EResult::Success;
}

EResult SDLRendererRHI::DrawIndexed(uint32 count)
{
	if (!m_Renderer || !m_VertexBuffer || !m_IndexBuffer) return EResult::Fail;

	SDLBuffer* vertexBuffer = static_cast<SDLBuffer*>(m_VertexBuffer);
	SDLBuffer* indexBuffer = static_cast<SDLBuffer*>(m_IndexBuffer);

	if (!vertexBuffer || !indexBuffer) return EResult::Fail;

	int32 w, h;
	SDL_GetWindowSize(m_Window, &w, &h);
	glm::vec2 screenCenter(w * 0.5f, h * 0.5f);

	SDL_Vertex* finalVertices = static_cast<SDLShader*>(m_CurrentShader)->ProcessVertex(vertexBuffer, m_WorldMatrix, m_MaterialColor, screenCenter);
	if (!finalVertices) return EResult::Fail;
	//SDL_Vertex* vertices = reinterpret_cast<SDL_Vertex*>(vertexBuffer->GetNativeHandle());
	const int* indices = reinterpret_cast<const int*>(indexBuffer->GetNativeHandle());

	sizet numVertices = vertexBuffer->m_Data.size() / sizeof(Vertex);

	if (!SDL_RenderGeometry(m_Renderer, nullptr, finalVertices, numVertices, indices, count))
	{
		return EResult::Fail;
	}

	return EResult::Success;
}

#pragma endregion

