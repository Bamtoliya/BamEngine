#pragma once

#include "SDLGPUPipeline.h"
#include "Vertex.h"

#pragma region Constructor&Destructor

EResult SDLGPUPipeline::Initialize(const DESC& desc)
{
	SDL_GPUDevice* device = static_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI());

	SDL_GPUGraphicsPipelineCreateInfo createInfo = {};
	Safe_AddRef(desc.VertexShader);
	Safe_AddRef(desc.PixelShader);
	SDL_GPUShader* vertexShader = static_cast<SDL_GPUShader*>(desc.VertexShader->GetNativeHandle());
	SDL_GPUShader* fragmentShader = static_cast<SDL_GPUShader*>(desc.PixelShader->GetNativeHandle());
	createInfo.vertex_shader = vertexShader;
	createInfo.fragment_shader = fragmentShader;

	// Vertex Input 설정 (sprite.vert의 layout에 맞춤)
	SDL_GPUVertexBufferDescription vertexBufferDesc = {};
	vertexBufferDesc.slot = 0;
	vertexBufferDesc.pitch = sizeof(Vertex);
	vertexBufferDesc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

	SDL_GPUVertexAttribute vertexAttributes[3] = {};
	// location 0: vec3 inPosition
	vertexAttributes[0].location = 0;
	vertexAttributes[0].buffer_slot = 0;
	vertexAttributes[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	vertexAttributes[0].offset = offsetof(Vertex, position);
	// location 1: vec2 inTexCoord
	vertexAttributes[1].location = 1;
	vertexAttributes[1].buffer_slot = 0;
	vertexAttributes[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
	vertexAttributes[1].offset = offsetof(Vertex, texCoord);
	// location 2: vec3 inNormal
	vertexAttributes[2].location = 2;
	vertexAttributes[2].buffer_slot = 0;
	vertexAttributes[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
	vertexAttributes[2].offset = offsetof(Vertex, normal);

	SDL_GPUVertexInputState vertexInputState = {};
	vertexInputState.vertex_buffer_descriptions = &vertexBufferDesc;
	vertexInputState.num_vertex_buffers = 1;
	vertexInputState.vertex_attributes = vertexAttributes;
	vertexInputState.num_vertex_attributes = 3;
	createInfo.vertex_input_state = vertexInputState;

	// Color Target (스왑체인 포맷)
	SDL_GPUColorTargetDescription colorTarget = {};
	colorTarget.format = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;

	SDL_GPUGraphicsPipelineTargetInfo targetInfo = {};
	targetInfo.color_target_descriptions = &colorTarget;
	targetInfo.num_color_targets = 1;
	createInfo.target_info = targetInfo;

	// Primitive type
	createInfo.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

	m_Pipeline = SDL_CreateGPUGraphicsPipeline(device, &createInfo);
	return m_Pipeline ? EResult::Success : EResult::Fail;
}
SDLGPUPipeline* SDLGPUPipeline::Create(RHI* rhi, const DESC& desc)
{
	SDLGPUPipeline* instance = new SDLGPUPipeline(rhi, desc);
	if (IsFailure(instance->Initialize(desc)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}
void SDLGPUPipeline::Free()
{
	if(m_Pipeline)
	{
		SDL_ReleaseGPUGraphicsPipeline(static_cast<SDL_GPUDevice*>(m_RHI->GetNativeRHI()), m_Pipeline);
		m_Pipeline = nullptr;
	}
	Safe_Release(m_Desc.VertexShader);
	Safe_Release(m_Desc.PixelShader);
	__super::Free();
}
#pragma endregion