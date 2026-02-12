#pragma once

#include "SDLGPURHI.h"
#include "SDLGPUBuffer.h"
#include "SDLGPUShader.h"
#include "SDLGPUSampler.h"
#include "SDLGPUTexture.h"
#include "SDLGPUPipeline.h"
#include "RenderTargetManager.h"
#include "RenderPass.h"
#include "Vertex.h"

#pragma region Constructor&Destructor
EResult SDLGPURHI::Initialize(void* arg)
{
	if (arg)
	{
		CAST_DESC
		if (!desc->WindowHandle) return EResult::InvalidArgument;

		m_Window = reinterpret_cast<SDL_Window*>(desc->WindowHandle);
#ifdef _DEBUG
		m_Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, "vulkan");
#else
		m_Device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, false, "vulkan");
#endif

		if (!SDL_ClaimWindowForGPUDevice(m_Device, m_Window))
		{
			return EResult::Fail;
		}

		m_SwapChainHeight = desc->Height;
		m_SwapChainWidth = desc->Width;
		m_BackBuffer = SDLGPUTexture::Create(this, {}, false);
	}
	return EResult::Success;
}

RHI* SDLGPURHI::Create(void* arg)
{
	SDLGPURHI* Instance = new SDLGPURHI();
	if (IsFailure(Instance->Initialize(arg)))
	{
		delete Instance;
		Instance = nullptr;
		fmt::print(stderr, "SDLGPURHI Creation Failed\n");
	}
	return Instance;
}

void SDLGPURHI::Free()
{
	Safe_Release(m_VertexBuffer);
	Safe_Release(m_IndexBuffer);
	Safe_Release(m_CurrentShader);
	Safe_Release(m_CurrentPipeline);

	Safe_Release(m_BackBuffer);
	if(m_Device)
	{
		SDL_WaitForGPUIdle(m_Device);
		SDL_ReleaseWindowFromGPUDevice(m_Device, m_Window);
		SDL_DestroyGPUDevice(m_Device);
		m_Device = nullptr;
	}	
}
#pragma endregion

#pragma region Resize
EResult SDLGPURHI::Resize(uint32 width, uint32 height)
{
	RHI::Resize(width, height);
	SetViewport(0, 0, width, height);
	return EResult::Success;
}
#pragma endregion

#pragma region Frame

EResult SDLGPURHI::BeginFrame()
{
	m_CurrentCommandBuffer = SDL_AcquireGPUCommandBuffer(m_Device);
	if (!m_CurrentCommandBuffer) return EResult::Fail;

	SDL_GPUTexture* backBufferHandle = nullptr;

	if (!SDL_WaitAndAcquireGPUSwapchainTexture(m_CurrentCommandBuffer, m_Window, &backBufferHandle, nullptr, nullptr)) return EResult::Fail;

	m_BackBuffer->SetNativeHandle(backBufferHandle);

	if (backBufferHandle)
	{
		SDL_GPUColorTargetInfo colorTargetInfo = {};
		colorTargetInfo.texture = static_cast<SDL_GPUTexture*>(m_BackBuffer->GetNativeHandle());
		colorTargetInfo.clear_color = { 0.1f, 0.1f, 0.1f, 1.0f };
		colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
		colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;

		m_CurrentRenderPass = SDL_BeginGPURenderPass(m_CurrentCommandBuffer, &colorTargetInfo, 1, nullptr);
		if (!m_CurrentRenderPass) return EResult::Fail;
	}

	return EResult::Success;
}


EResult SDLGPURHI::EndFrame()
{
	if (m_CurrentRenderPass)
	{
		SDL_EndGPURenderPass(m_CurrentRenderPass);
		m_CurrentRenderPass = nullptr;
	}

	if (m_CurrentCommandBuffer)
	{
		SDL_SubmitGPUCommandBuffer(m_CurrentCommandBuffer);
		m_CurrentCommandBuffer = nullptr;
	}

	if (m_BackBuffer->GetNativeHandle())
	{
		m_BackBuffer->SetNativeHandle(nullptr);
	}
	
	return EResult::Success;
}
#pragma endregion

#pragma region Create Resources
#pragma region Buffer
RHIBuffer* SDLGPURHI::CreateBuffer(void* data, uint32 size, uint32 stride, ERHIBufferType type)
{
	tagRHIBufferDesc desc = {};
	desc.BufferType = type;
	desc.Size = size;
	desc.Stride = stride;
	desc.InitialData = data;

	SDLGPUBuffer* buffer = SDLGPUBuffer::Create(this, desc);
	if (!buffer)
	{
		Safe_Release(buffer);
		return nullptr;
	}
	return buffer;
}

RHIBuffer* SDLGPURHI::CreateVertexBuffer(void* data, uint32 size, uint32 stride)
{
	return CreateBuffer(data, size, stride, ERHIBufferType::Vertex);
}

RHIBuffer* SDLGPURHI::CreateIndexBuffer(void* data, uint32 size, uint32 stride)
{
	return CreateBuffer(data, size, stride, ERHIBufferType::Index);
}
#pragma endregion

#pragma region Texture

RHITexture* SDLGPURHI::CreateTextureFromFile(const char* filename)
{
	SDL_Surface* surface = SDL_LoadBMP(filename);
	if (!surface)
	{
		return nullptr;
	}

	SDL_Surface* convertedSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
	SDL_DestroySurface(surface);
	surface = convertedSurface;

	uint32 width = static_cast<uint32>(surface->w);
	uint32 height = static_cast<uint32>(surface->h);
	uint32 dataSize = width * height * 4;

	SDL_GPUTextureCreateInfo createInfo = {};

	createInfo.type = SDL_GPU_TEXTURETYPE_2D;
	createInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	createInfo.width = width;
	createInfo.height = height;
	createInfo.layer_count_or_depth = 1;
	createInfo.num_levels = 1;
	createInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

	SDLGPUTexture* texture = SDLGPUTexture::Create(this, createInfo);

	if (!texture->GetNativeHandle())
	{
		SDL_DestroySurface(surface);
		Safe_Release(texture);
		return nullptr;
	}

	if (IsFailure(UploadTextureData(static_cast<SDL_GPUTexture*>(texture->GetNativeHandle()), surface->pixels, width, height)))
	{
		SDL_DestroySurface(surface);
		Safe_Release(texture);
		return nullptr;
	}

	SDL_DestroySurface(surface);
	return texture;
}

RHITexture* SDLGPURHI::CreateTextureFromFile(const wchar* filename)
{
	string filePath = WStrToStr(filename);
	SDL_Surface* surface = SDL_LoadPNG(filePath.c_str());
	if (!surface)
	{
		return nullptr;
	}

	SDL_Surface* convertedSurface = SDL_ConvertSurface(surface, SDL_PIXELFORMAT_ABGR8888);
	SDL_DestroySurface(surface);
	surface = convertedSurface;

	uint32 width = static_cast<uint32>(surface->w);
	uint32 height = static_cast<uint32>(surface->h);
	uint32 dataSize = width * height * 4;

	SDL_GPUTextureCreateInfo createInfo = {};

	createInfo.type = SDL_GPU_TEXTURETYPE_2D;
	createInfo.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
	createInfo.width = width;
	createInfo.height = height;
	createInfo.layer_count_or_depth = 1;
	createInfo.num_levels = 1;
	createInfo.usage = SDL_GPU_TEXTUREUSAGE_SAMPLER;

	SDLGPUTexture* texture = SDLGPUTexture::Create(this, createInfo);

	if (!texture->GetNativeHandle())
	{
		SDL_DestroySurface(surface);
		Safe_Release(texture);
		return nullptr;
	}

	if (IsFailure(UploadTextureData(static_cast<SDL_GPUTexture*>(texture->GetNativeHandle()), surface->pixels, width, height)))
	{
		SDL_DestroySurface(surface);
		Safe_Release(texture);
		return nullptr;
	}

	SDL_DestroySurface(surface);
	return texture;
}

RHITexture* SDLGPURHI::CreateTextureFromMemory(void* data, uint32 size)
{
	return nullptr;
}

RHITexture* SDLGPURHI::CreateTexture2D(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize)
{
	//SDLTexture* texture = new SDLTexture(width, height, mipLevels, arraySize);
	//texture->m_Texture = SDL_CreateTexture(m_Renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STATIC, width, height);
	//
	//if (texture->m_Texture && data)
	//{
	//	SDL_UpdateTexture(texture->m_Texture, nullptr, data, width * 4);
	//}
	return nullptr;
}

RHITexture* SDLGPURHI::CreateTextureCube(void* data, uint32 size, uint32 mipLevels)
{
	return nullptr;
}

RHITexture* SDLGPURHI::CreateTexture3D(void* data, uint32 width, uint32 height, uint32 depth, uint32 mipLevels)
{
	return nullptr;
}

RHITexture* SDLGPURHI::CreateRenderTargetTexture(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize)
{
	tagRenderTargetDesc* rtDesc = reinterpret_cast<tagRenderTargetDesc*>(data);
	SDL_GPUTextureCreateInfo desc = {};
	desc.format = SDL_GPUTextureFormats[static_cast<uint8>(rtDesc->Format)];
	desc.type = SDL_GPUTextureTypes[static_cast<uint8>(rtDesc->TextureType)];
	desc.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
	desc.width = width;
	desc.height = height;
	desc.layer_count_or_depth = arraySize;
	desc.num_levels = mipLevels;

	SDLGPUTexture* texture = SDLGPUTexture::Create(this, desc);
	if (!texture)
	{
		Safe_Release(texture);
		return nullptr;
	}
	return texture;
}

RHITexture* SDLGPURHI::CreateDepthStencilTexture(void* data, uint32 width, uint32 height, uint32 mipLevels, uint32 arraySize)
{
	SDL_GPUTextureCreateInfo desc = {};
	desc.type = SDL_GPU_TEXTURETYPE_2D;
	desc.format = SDL_GPU_TEXTUREFORMAT_D24_UNORM_S8_UINT;
	desc.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
	desc.width = width;
	desc.height = height;
	desc.layer_count_or_depth = arraySize;
	desc.num_levels = mipLevels;

	SDLGPUTexture* texture = SDLGPUTexture::Create(this, desc);
	if (!texture)
	{
		Safe_Release(texture);
		return nullptr;
	}
	return texture;
}

RHITexture* SDLGPURHI::CreateTextureFromNativeHandle(void* nativeHandle)
{
	//if (!nativeHandle) return nullptr;
	//SDL_Texture* NativeTexture = reinterpret_cast<SDL_Texture*>(nativeHandle);
	//SDLTexture* texture = new SDLTexture(NativeTexture->w, NativeTexture->h, 1, 1);
	//texture->m_Texture = NativeTexture;
	//return texture;
	return nullptr;
}

EResult SDLGPURHI::UploadTextureData(SDL_GPUTexture* texture, void* data, uint32 width, uint32 height)
{
	if (!texture || !data) return EResult::InvalidArgument;

	uint32 dataSize = width * height * 4;

	SDL_GPUTransferBufferCreateInfo transferBufferInfo = {};
	transferBufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transferBufferInfo.size = dataSize;

	SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_Device, &transferBufferInfo);
	if (!transferBuffer) return EResult::Fail;

	uint8* mappedData = static_cast<uint8*>(SDL_MapGPUTransferBuffer(m_Device, transferBuffer, false));
	if(mappedData)
	{
		memcpy(mappedData, data, dataSize);
		SDL_UnmapGPUTransferBuffer(m_Device, transferBuffer);
	}
	else
	{
		SDL_ReleaseGPUTransferBuffer(m_Device, transferBuffer);
		return EResult::Fail;
	}

	SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_Device);

	// 4. 복사 패스(Copy Pass) 시작
	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);

	// 5. 업로드 명령 기록 (Transfer Buffer -> GPU Texture)
	SDL_GPUTextureTransferInfo source = {};
	source.transfer_buffer = transferBuffer;
	source.offset = 0;
	source.pixels_per_row = width;
	source.rows_per_layer = height;

	SDL_GPUTextureRegion destination = {};
	destination.texture = texture;
	destination.w = width;
	destination.h = height;
	destination.d = 1;

	SDL_UploadToGPUTexture(copyPass, &source, &destination, false);

	// 6. 패스 종료 및 제출
	SDL_EndGPUCopyPass(copyPass);
	SDL_SubmitGPUCommandBuffer(cmd); // GPU야 일해라!

	// 7. 전송 버퍼 해제
	// SDL_GPU는 내부적으로 커맨드 실행이 완료될 때까지 이 버퍼를 유지해줍니다.
	// 따라서 바로 Release를 호출해도 안전합니다.
	SDL_ReleaseGPUTransferBuffer(m_Device, transferBuffer);

	return EResult::Success;
}

EResult SDLGPURHI::UploadBufferData(SDL_GPUBuffer* buffer, void* data, uint32 size)
{
	if (!buffer || !data || size == 0) return EResult::InvalidArgument;

	SDL_GPUTransferBufferCreateInfo transferBufferInfo = {};
	transferBufferInfo.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
	transferBufferInfo.size = size;

	SDL_GPUTransferBuffer* transferBuffer = SDL_CreateGPUTransferBuffer(m_Device, &transferBufferInfo);
	if (!transferBuffer) return EResult::Fail;

	uint8* mappedData = static_cast<uint8*>(SDL_MapGPUTransferBuffer(m_Device, transferBuffer, false));
	if (mappedData)
	{
		memcpy(mappedData, data, size);
		SDL_UnmapGPUTransferBuffer(m_Device, transferBuffer);
	}
	else
	{
		SDL_ReleaseGPUTransferBuffer(m_Device, transferBuffer);
		return EResult::Fail;
	}

	SDL_GPUCommandBuffer* cmd = SDL_AcquireGPUCommandBuffer(m_Device);
	if (!cmd)
	{
		SDL_ReleaseGPUTransferBuffer(m_Device, transferBuffer);
		return EResult::Fail;
	}

	SDL_GPUCopyPass* copyPass = SDL_BeginGPUCopyPass(cmd);

	SDL_GPUTransferBufferLocation source = {};
	source.transfer_buffer = transferBuffer;
	source.offset = 0;

	SDL_GPUBufferRegion destination = {};
	destination.buffer = buffer;
	destination.offset = 0;
	destination.size = size;

	SDL_UploadToGPUBuffer(copyPass, &source, &destination, false);

	SDL_EndGPUCopyPass(copyPass);
	SDL_SubmitGPUCommandBuffer(cmd);

	SDL_ReleaseGPUTransferBuffer(m_Device, transferBuffer);
	return EResult::Success;
}

#pragma endregion

#pragma region Pipeline
RHIPipeline* SDLGPURHI::CreatePipeline(const tagRHIPipelineDesc& desc)
{
	return SDLGPUPipeline::Create(this, desc);
}
#pragma endregion

#pragma region Sampler
RHISampler* SDLGPURHI::CreateSampler(const tagRHISamplerDesc& desc)
{
	return SDLGPUSampler::Create(this, desc);
}

#pragma region Shader
RHIShader* SDLGPURHI::CreateShader(const tagRHIShaderDesc& desc)
{
	return SDLGPUShader::Create(this, desc);
}
#pragma endregion

#pragma endregion


#pragma endregion

#pragma region Bind
EResult SDLGPURHI::BindRenderTarget(RHITexture* renderTarget, RHITexture* depthStencil)
{
	if (m_CurrentRenderPass)
	{
		SDL_EndGPURenderPass(m_CurrentRenderPass);
		m_CurrentRenderPass = nullptr;
	}

	if (!m_CurrentCommandBuffer) return EResult::Fail;

	// Color target 설정
	SDL_GPUColorTargetInfo colorTargetInfo = {};

	if (renderTarget)
	{
		// 오프스크린 렌더 타겟에 렌더링
		colorTargetInfo.texture = static_cast<SDL_GPUTexture*>(renderTarget->GetNativeHandle());
		colorTargetInfo.clear_color = { m_ClearColor.r, m_ClearColor.g, m_ClearColor.b, m_ClearColor.a };
		colorTargetInfo.load_op = SDL_GPU_LOADOP_CLEAR;
		colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
	}
	else
	{
		// Backbuffer에 렌더링 — 기존 내용 보존 (blit 결과 유지)
		SDL_GPUTexture* backBufferHandle = static_cast<SDL_GPUTexture*>(m_BackBuffer->GetNativeHandle());
		if (!backBufferHandle) return EResult::Fail;
		colorTargetInfo.texture = backBufferHandle;
		colorTargetInfo.load_op = SDL_GPU_LOADOP_LOAD;
		colorTargetInfo.store_op = SDL_GPU_STOREOP_STORE;
	}

	if (!colorTargetInfo.texture) return EResult::Fail;

	// Depth/Stencil target 설정
	SDL_GPUDepthStencilTargetInfo depthInfo = {};
	SDL_GPUDepthStencilTargetInfo* depthInfoPtr = nullptr;

	if (depthStencil)
	{
		depthInfo.texture = static_cast<SDL_GPUTexture*>(depthStencil->GetNativeHandle());
		depthInfo.load_op = SDL_GPU_LOADOP_CLEAR;
		depthInfo.store_op = SDL_GPU_STOREOP_DONT_CARE;
		depthInfo.stencil_load_op = SDL_GPU_LOADOP_CLEAR;
		depthInfo.stencil_store_op = SDL_GPU_STOREOP_DONT_CARE;
		depthInfo.clear_depth = 1.0f;
		depthInfo.clear_stencil = 0;
		depthInfoPtr = &depthInfo;
	}

	m_CurrentRenderPass = SDL_BeginGPURenderPass(m_CurrentCommandBuffer, &colorTargetInfo, 1, depthInfoPtr);
	return m_CurrentRenderPass ? EResult::Success : EResult::Fail;
}
EResult SDLGPURHI::BindTexture(RHITexture* texture, uint32 slot)
{
	//if (texture == nullptr)
	//{
	//	m_CurrentTextures[slot] = nullptr;
	//	return EResult::Success;
	//}
	//
	//m_CurrentTextures[slot] = texture;
	return EResult::Success;
}
EResult SDLGPURHI::BindTextureSampler(RHITexture* texture, RHISampler* sampler, uint32 slot)
{
	if (!texture || !sampler || !m_CurrentRenderPass) return EResult::InvalidArgument;

	SDL_GPUTextureSamplerBinding binding = {};
	binding.texture = static_cast<SDL_GPUTexture*>(texture->GetNativeHandle());
	binding.sampler = static_cast<SDL_GPUSampler*>(sampler->GetNativeHandle());
	SDL_BindGPUFragmentSamplers(m_CurrentRenderPass, slot, &binding, 1);
	return EResult::Success;
}
EResult SDLGPURHI::BindRenderTargets(uint32 count, RHITexture** renderTargets, RHITexture* depthStencil)
{
	//bool bIsChanged = false;
	//if (m_CurrentRenderTargetCount != count) bIsChanged = true;
	//else if (m_CurrentDepthStencil != depthStencil) bIsChanged = true;
	//else
	//{
	//	for (uint32 i = 0; i < count; ++i)
	//	{
	//		if (m_CurrentRenderTargets[i] != renderTargets[i])
	//		{
	//			bIsChanged = true;
	//			break;
	//		}
	//	}
	//}
	//
	//if (!bIsChanged) return EResult::Success;
	//
	//m_CurrentRenderTargetCount = count;
	//for (uint32 i = 0; i < count; ++i)
	//{
	//	m_CurrentRenderTargets[i] = renderTargets[i];
	//}
	//m_CurrentDepthStencil = depthStencil;
	//
	//SDL_SetRenderTarget(m_Renderer, static_cast<SDLTexture*>(renderTargets[0])->m_Texture);
	//
	return EResult::Success;
}
EResult SDLGPURHI::BindRenderPass(RenderPass* renderPass)
{
	if (m_CurrentRenderPass)
	{
		SDL_EndGPURenderPass(m_CurrentRenderPass);
		m_CurrentRenderPass = nullptr;
	}

	if (!m_CurrentCommandBuffer) return EResult::Fail;

	SDL_GPUColorTargetInfo colorTargetInfo[MAX_RENDER_TARGET_COUNT] = {};
	uint32 targetCount = renderPass->GetRenderTargetCount();
	if (targetCount == 0)
	{
		colorTargetInfo[0].texture = static_cast<SDL_GPUTexture*>(m_BackBuffer->GetNativeHandle());
		colorTargetInfo[0].load_op = SDL_GPURenderPassLoadOperations[static_cast<uint8>(renderPass->GetLoadOperation())];
		colorTargetInfo[0].store_op = SDL_GPURenderPassStoreOperations[static_cast<uint8>(renderPass->GetStoreOperation())];
		colorTargetInfo[0].cycle = false;
		colorTargetInfo[0].mip_level = 0;
		colorTargetInfo[0].layer_or_depth_plane = 0;

		targetCount = 1;
	}
	else
	{
		for (uint i = 0; i < targetCount; ++i)
		{
			RenderTarget* renderTarget = RenderTargetManager::Get().GetRenderTarget(renderPass->GetRenderTargetName(i));
			colorTargetInfo[i].texture = static_cast<SDL_GPUTexture*>(renderTarget->GetTexture()->GetNativeHandle());
			vec4 clearColor = renderTarget->GetClearColor();
			if (renderPass->HasOverrideClearColor())
			{
				clearColor = renderPass->GetOverrideClearColor();
			}
			colorTargetInfo[i].clear_color = { clearColor.r, clearColor.g, clearColor.b, clearColor.a };
			colorTargetInfo[i].load_op = SDL_GPURenderPassLoadOperations[static_cast<uint8>(renderPass->GetLoadOperation())];
			colorTargetInfo[i].store_op = SDL_GPURenderPassStoreOperations[static_cast<uint8>(renderPass->GetStoreOperation())];

			colorTargetInfo[i].cycle = false;
			colorTargetInfo[i].mip_level = 0;
			colorTargetInfo[i].layer_or_depth_plane = 0;
		}
	}

	SDL_GPUDepthStencilTargetInfo depthInfo = {};
	bool bUseDepth = !renderPass->GetDepthStencilName().empty();

	if (bUseDepth)
	{
		RenderTarget* depthStencil = RenderTargetManager::Get().GetRenderTarget(renderPass->GetDepthStencilName());
		depthInfo.texture = static_cast<SDL_GPUTexture*>(depthStencil->GetTexture()->GetNativeHandle());
		depthInfo.cycle = false;
		depthInfo.clear_stencil = 0;
		depthInfo.clear_depth = 1.f;
		depthInfo.load_op = SDL_GPURenderPassLoadOperations[static_cast<uint8>(renderPass->GetLoadOperation())];
		depthInfo.store_op = SDL_GPURenderPassStoreOperations[static_cast<uint8>(renderPass->GetStoreOperation())];
		depthInfo.stencil_load_op = SDL_GPURenderPassLoadOperations[static_cast<uint8>(renderPass->GetStencilLoadOperation())];
		depthInfo.stencil_store_op = SDL_GPURenderPassStoreOperations[static_cast<uint8>(renderPass->GetStencilStoreOperation())];
	}

	m_CurrentRenderPass = SDL_BeginGPURenderPass(m_CurrentCommandBuffer, colorTargetInfo, targetCount, bUseDepth ? &depthInfo : nullptr);
	if(!m_CurrentRenderPass)
	{
		return EResult::Fail;
	}


	return EResult::Success;
}
EResult SDLGPURHI::BindShader(RHIShader* shader)
{
	//if (!shader) return EResult::InvalidArgument;
	//m_CurrentShader = static_cast<RHIShader*>(shader);
	return EResult::Success;
}
EResult SDLGPURHI::BindPipeline(RHIPipeline* pipeline)
{
	if (!pipeline)
		return EResult::InvalidArgument;
	m_CurrentPipeline = static_cast<RHIPipeline*>(pipeline);
	SDL_BindGPUGraphicsPipeline(m_CurrentRenderPass, static_cast<SDL_GPUGraphicsPipeline*>(m_CurrentPipeline->GetNativeHandle()));
	return EResult::Success;
}
EResult SDLGPURHI::BindConstantBuffer(void* arg, uint32 slot)
{
	if (!arg || !m_CurrentCommandBuffer) return EResult::InvalidArgument;

	SDL_PushGPUVertexUniformData(m_CurrentCommandBuffer, slot, arg, sizeof(mat4));
	return EResult::Success;
}
EResult SDLGPURHI::BindConstantBuffer(void* arg, uint32 size, uint32 slot)
{
	if (!arg || !m_CurrentCommandBuffer) return EResult::InvalidArgument;
	SDL_PushGPUVertexUniformData(m_CurrentCommandBuffer, slot, arg, size);
	SDL_PushGPUFragmentUniformData(m_CurrentCommandBuffer, slot, arg, size);
	return EResult::Success;
}
EResult SDLGPURHI::BindConstantRangeBuffer(void* arg, uint32 slot, uint32 offset, uint32 size)
{
	if (!arg || !m_CurrentCommandBuffer) return EResult::InvalidArgument;

	uint8* dataPtr = static_cast<uint8*>(arg) + offset;

	SDL_PushGPUVertexUniformData(m_CurrentCommandBuffer, slot, dataPtr, size);
	SDL_PushGPUFragmentUniformData(m_CurrentCommandBuffer, slot, dataPtr, size);

	return EResult::Success;
}
#pragma endregion

#pragma region Clear Resources
EResult SDLGPURHI::ClearRenderTarget(RHITexture* renderTarget, vec4 color)
{
	m_ClearColor = color;
	return EResult::Success;
}
EResult SDLGPURHI::ClearDepthStencil(RHITexture* depthStencil, f32 depth, uint8 stencil)
{
	return EResult::NotImplemented;
}
EResult SDLGPURHI::ClearRenderPass()
{
	if (!m_CurrentRenderPass) return EResult::Fail;
	SDL_EndGPURenderPass(m_CurrentRenderPass);
	m_CurrentRenderPass = nullptr;
	return EResult::Success;
}
#pragma endregion

#pragma region Setter
EResult SDLGPURHI::SetClearColor(vec4 color)
{
	//SDL_SetRenderDrawColor(m_Renderer,
	//	static_cast<uint8>(color.r * 255.0f),
	//	static_cast<uint8>(color.g * 255.0f),
	//	static_cast<uint8>(color.b * 255.0f),
	//	static_cast<uint8>(color.a * 255.0f));
	return EResult::Success;
}

EResult SDLGPURHI::SetViewport(int32 x, int32 y, uint32 width, uint32 height)
{
	SDL_GPUViewport viewport;
	viewport.x = 0.0f;
	viewport.y = 0.f;
	viewport.w = (float)width;
	viewport.h = (float)height;
	viewport.min_depth = 0.0f;
	viewport.max_depth = 1.0f;

	SDL_SetGPUViewport(m_CurrentRenderPass, &viewport);

	//SDL_Rect scissor;
	//scissor.x = 0;
	//scissor.y = 0;
	//scissor.w = (int)width;
	//scissor.h = (int)height;
	//
	//// 렌더 패스 객체(pass)에 시저 설정
	//SDL_SetGPUScissor(m_CurrentRenderPass, &scissor);
	return EResult::Success;
}
#pragma endregion

#pragma region Draw
EResult SDLGPURHI::Draw(uint32 count)
{
	if (!m_Device || !m_CurrentRenderPass || !m_VertexBuffer) return EResult::Fail;

	SDLGPUBuffer* vertexBuffer = static_cast<SDLGPUBuffer*>(m_VertexBuffer);
	SDL_GPUBuffer* nativeVertexBuffer = static_cast<SDL_GPUBuffer*>(vertexBuffer->GetNativeHandle());
	if (!nativeVertexBuffer) return EResult::Fail;

	SDL_GPUBufferBinding binding = {};
	binding.buffer = nativeVertexBuffer;
	binding.offset = 0;

	SDL_BindGPUVertexBuffers(m_CurrentRenderPass, 0, &binding, 1);
	SDL_DrawGPUPrimitives(m_CurrentRenderPass, count, 1, 0, 0);

	return EResult::Success;
}

EResult SDLGPURHI::DrawIndexed(uint32 count)
{
	if (!m_Device || !m_CurrentRenderPass || !m_VertexBuffer || !m_IndexBuffer) return EResult::Fail;

	SDLGPUBuffer* vertexBuffer = static_cast<SDLGPUBuffer*>(m_VertexBuffer);
	SDLGPUBuffer* indexBuffer = static_cast<SDLGPUBuffer*>(m_IndexBuffer);

	SDL_GPUBuffer* nativeVertexBuffer = static_cast<SDL_GPUBuffer*>(vertexBuffer->GetNativeHandle());
	SDL_GPUBuffer* nativeIndexBuffer = static_cast<SDL_GPUBuffer*>(indexBuffer->GetNativeHandle());

	if (!nativeVertexBuffer || !nativeIndexBuffer) return EResult::Fail;

	SDL_GPUBufferBinding vertexBinding = {};
	vertexBinding.buffer = nativeVertexBuffer;
	vertexBinding.offset = 0;

	SDL_GPUBufferBinding indexBinding = {};
	indexBinding.buffer = nativeIndexBuffer;
	indexBinding.offset = 0;

	SDL_GPUIndexElementSize indexSize = SDL_GPU_INDEXELEMENTSIZE_32BIT;
	if (indexBuffer->GetStride() == sizeof(uint16))
	{
		indexSize = SDL_GPU_INDEXELEMENTSIZE_16BIT;
	}
	else if (indexBuffer->GetStride() != sizeof(uint32))
	{
		return EResult::InvalidArgument;
	}

	SDL_BindGPUVertexBuffers(m_CurrentRenderPass, 0, &vertexBinding, 1);
	SDL_BindGPUIndexBuffer(m_CurrentRenderPass, &indexBinding, indexSize);

	SDL_DrawGPUIndexedPrimitives(m_CurrentRenderPass, count, 1, 0, 0, 0);
	return EResult::Success;
}

EResult SDLGPURHI::DrawTexture(RHITexture* texture)
{
	if (!texture || !m_CurrentCommandBuffer || !m_BackBuffer->GetNativeHandle()) return EResult::Fail;

	// SDL_BlitGPUTexture는 렌더 패스 밖에서 호출해야 함
	if (m_CurrentRenderPass)
	{
		SDL_EndGPURenderPass(m_CurrentRenderPass);
		m_CurrentRenderPass = nullptr;
	}

	SDL_GPUBlitInfo blitInfo = {};

	// Source: 오프스크린 렌더 타겟
	blitInfo.source.texture = static_cast<SDL_GPUTexture*>(texture->GetNativeHandle());
	blitInfo.source.w = texture->GetWidth();
	blitInfo.source.h = texture->GetHeight();

	// Destination: 스왑체인 backbuffer
	blitInfo.destination.texture = static_cast<SDL_GPUTexture*>(m_BackBuffer->GetNativeHandle());
	blitInfo.destination.w = m_SwapChainWidth;
	blitInfo.destination.h = m_SwapChainHeight;

	blitInfo.load_op = SDL_GPU_LOADOP_CLEAR;
	blitInfo.filter = SDL_GPU_FILTER_LINEAR;

	SDL_BlitGPUTexture(m_CurrentCommandBuffer, &blitInfo);

	return EResult::Success;
}

#pragma endregion