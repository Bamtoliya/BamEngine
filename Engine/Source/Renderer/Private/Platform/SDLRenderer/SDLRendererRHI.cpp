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
	if (!arg) return EResult::InvalidArgument;
	CAST_DESC
	m_Window = reinterpret_cast<SDL_Window*>(desc->WindowHandle);
	m_Renderer = SDL_CreateRenderer(m_Window, nullptr);
	if (!m_Window || !m_Renderer) return EResult::Fail;

	int32 width, height;
	SDL_GetWindowSize(m_Window, &width, &height);
	m_BackBuffer = new SDLTexture(m_Renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
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
	Safe_Release(m_BackBuffer);
	if (m_Renderer)
	{
		SDL_DestroyRenderer(m_Renderer);
		m_Renderer = nullptr;
	}
}
#pragma endregion

#pragma region Resize
EResult SDLRendererRHI::Resize(uint32 width, uint32 height)
{
	RHI::Resize(width, height);
	SetViewport(0, 0, width, height);
	return EResult::Success;
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
	SDL_Surface* surface = SDL_LoadPNG(strFileName.c_str());
	if (!surface)
	{
		return nullptr;
	}

	SDLTexture* texture = new SDLTexture(m_Renderer, surface);
	SDL_DestroySurface(surface);

	if (!texture->GetNativeHandle())
	{
		Safe_Release(texture);
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
	SDL_Texture* sdlNativeTexture = SDL_CreateTexture(m_Renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
	if (!sdlNativeTexture)
	{
		delete texture;
		return nullptr;
	}
	texture->m_Texture = sdlNativeTexture;
	return texture;
}

RHITexture* SDLRendererRHI::CreateDepthStencilTexture(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize)
{
	SDLTexture* texture = new SDLTexture(width, height, mipLevels, arraySize);
	SDL_Texture* sdlNativeTexture = SDL_CreateTexture(m_Renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, width, height);
	if (!sdlNativeTexture)
	{
		delete texture;
		return nullptr;
	}
	texture->m_Texture = sdlNativeTexture;
	return texture;
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
EResult SDLRendererRHI::BindRenderTarget(RHITexture* renderTarget, RHITexture* depthStencil)
{
	if (renderTarget)
	{
		SDL_Texture* sdlTexture = reinterpret_cast<SDL_Texture*>(renderTarget->GetNativeHandle());
		if(!SDL_SetRenderTarget(m_Renderer, sdlTexture))
		{
			printf("Failed to Set Render Target: %s\n", SDL_GetError());
			if (!SDL_SetRenderTarget(m_Renderer, nullptr))
				return EResult::Fail;
			return EResult::Fail;
		}
	}
	else
	{
		if (!SDL_SetRenderTarget(m_Renderer, nullptr) != 0)
		{
			return EResult::Fail;
		}
	}
	return EResult::Success;
}
EResult SDLRendererRHI::BindTexture(RHITexture* texture, uint32 slot)
{
	if (texture == nullptr)
	{
		m_CurrentTextures[slot] = nullptr;
		return EResult::Success;
	}

	m_CurrentTextures[slot] = texture;
	return EResult::Success;
}
EResult SDLRendererRHI::BindRenderTargets(uint32 count, RHITexture** renderTargets, RHITexture* depthStencil)
{
	bool bIsChanged = false;
	if (m_CurrentRenderTargetCount != count) bIsChanged = true;
	else if (m_CurrentDepthStencil != depthStencil) bIsChanged = true;
	else
	{
		for (uint32 i = 0; i < count; ++i)
		{
			if (m_CurrentRenderTargets[i] != renderTargets[i])
			{
				bIsChanged = true;
				break;
			}
		}
	}

	if (!bIsChanged) return EResult::Success;

	m_CurrentRenderTargetCount = count;
	for (uint32 i = 0; i < count; ++i)
	{
		m_CurrentRenderTargets[i] = renderTargets[i];
	}
	m_CurrentDepthStencil = depthStencil;

	SDL_SetRenderTarget(m_Renderer, static_cast<SDLTexture*>(renderTargets[0])->m_Texture);

	return EResult();
}
EResult SDLRendererRHI::BindShader(RHIShader* shader)
{
	if (!shader) return EResult::InvalidArgument;
	m_CurrentShader = static_cast<RHIShader*>(shader);
	return EResult::Success;
}
EResult SDLRendererRHI::BindConstantBuffer(void* arg, uint32 slot)
{
	if (!arg) return EResult::InvalidArgument;

	if (slot == 0)
	{
		memcpy(&m_WorldMatrix, arg, sizeof(mat4));
	}
	else if (slot == 1)
	{
		memcpy(&m_MaterialColor, arg, sizeof(vec4));
	}

	return EResult::Success;
}
#pragma endregion

#pragma region Clear Resources
EResult SDLRendererRHI::ClearRenderTarget(RHITexture* renderTarget, vec4 color)
{
	SDL_Texture* oldTarget = SDL_GetRenderTarget(m_Renderer);
	if (renderTarget)
	{
		SDL_Texture* sdlTexture = static_cast<SDLTexture*>(renderTarget)->m_Texture;
		SDL_SetRenderTarget(m_Renderer, sdlTexture);
	}
	else
	{
		SDL_SetRenderTarget(m_Renderer, nullptr);
	}

	SDL_SetRenderDrawColor(m_Renderer,
		static_cast<uint8>(color.r * 255.0f),
		static_cast<uint8>(color.g * 255.0f),
		static_cast<uint8>(color.b * 255.0f),
		static_cast<uint8>(color.a * 255.0f));

	SDL_RenderClear(m_Renderer);
	SDL_SetRenderTarget(m_Renderer, oldTarget);

	return EResult::Success;
}
EResult SDLRendererRHI::ClearDepthStencil(RHITexture* depthStencil, f32 depth, uint8 stencil)
{
	return EResult::NotImplemented;
}
#pragma endregion

#pragma region Setter
EResult SDLRendererRHI::SetClearColor(vec4 color)
{
	SDL_SetRenderDrawColor(m_Renderer,
		static_cast<uint8>(color.r * 255.0f),
		static_cast<uint8>(color.g * 255.0f),
		static_cast<uint8>(color.b * 255.0f),
		static_cast<uint8>(color.a * 255.0f));
	return EResult::Success;
}

EResult SDLRendererRHI::SetViewport(int32 x, int32 y, uint32 width, uint32 height)
{
	SDL_Rect viewport;
	viewport.x = x;
	viewport.y = y;
	viewport.w = width;
	viewport.h = height;
	if (!SDL_SetRenderViewport(m_Renderer, &viewport))
	{
		return EResult::Fail;
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

	float w, h;

	// 현재 렌더러가 렌더 타겟(텍스처)을 보고 있는지 확인합니다.
	SDL_Texture* currentTarget = SDL_GetRenderTarget(m_Renderer);

	if (currentTarget)
	{
		// 렌더 타겟이 설정되어 있다면, 그 텍스처의 크기를 가져옵니다.
		SDL_GetTextureSize(currentTarget, &w, &h);
	}
	else
	{
		// 렌더 타겟이 없다면(nullptr), 메인 윈도우(백버퍼)의 크기를 가져옵니다.
		int iw, ih;
		SDL_GetWindowSize(m_Window, &iw, &ih);
		w = (float)iw;
		h = (float)ih;
	}

	glm::vec2 screenCenter(w * 0.5f, h * 0.5f);

	SDL_Vertex* finalVertices = static_cast<SDLShader*>(m_CurrentShader)->ProcessVertex(vertexBuffer, m_WorldMatrix, m_MaterialColor, screenCenter);
	if (!finalVertices) return EResult::Fail;
	//SDL_Vertex* vertices = reinterpret_cast<SDL_Vertex*>(vertexBuffer->GetNativeHandle());

	SDL_Texture* texture = static_cast<SDL_Texture*>(m_CurrentTextures[0]->GetNativeHandle());
	const int* indices = reinterpret_cast<const int*>(indexBuffer->GetNativeHandle());

	sizet numVertices = vertexBuffer->m_Data.size() / sizeof(Vertex);

	if (!SDL_RenderGeometry(m_Renderer, texture, finalVertices, numVertices, indices, count))
	{
		return EResult::Fail;
	}

	return EResult::Success;
}

EResult SDLRendererRHI::DrawTexture(RHITexture* texture)
{
	SDL_Texture* sdlTexture = static_cast<SDL_Texture*>(texture->GetNativeHandle());
	SDL_RenderTexture(m_Renderer, sdlTexture, nullptr, nullptr);
	return EResult::Success;
}

#pragma endregion