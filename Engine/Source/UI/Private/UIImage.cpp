#pragma once

#include "UIImage.h"
#include "Renderer.h"
#include "ComponentRegistry.h"
#include "RenderPass.h"
#include "PipelineManager.h"
#include "Sprite.h"
#include "SamplerManager.h"

REGISTER_COMPONENT(UIImage)

#pragma region Constructor&Destructor

EResult UIImage::Initialize(void* arg)
{
	return EResult();
}

UIImage* UIImage::Create(void* arg)
{
	UIImage* instance = new UIImage();
	if (IsFailure(instance->Initialize(arg)))
	{
		Safe_Release(instance);
		return nullptr;
	}
	return instance;
}

Component* UIImage::Clone(GameObject* owner, void* arg)
{
	UIImage* instance = new UIImage();
	if(arg)
	{
		instance->SetOwner(owner);
	    if (IsFailure(instance->Initialize(arg)))
	    {
	        Safe_Release(instance);
	        return nullptr;
	    }
	}
	else
	{
		DESC desc;
	    if (IsFailure(instance->Initialize(&desc)))
	    {
	        Safe_Release(instance);
	        return nullptr;
		}
	}
	return instance;
}

void UIImage::Free()
{
	__super::Free();
}

#pragma endregion

void UIImage::LateUpdate(f32 dt)
{
	__super::LateUpdate(dt);
}

EResult UIImage::Render(f32 dt, RenderPass* renderPass)
{
	if (renderPass->GetPassType() != ERenderPassType::UI) return EResult::Success;
	if (!m_SpriteHandle) return EResult::Success;
	
	RHI* rhi = Renderer::Get().GetRHI();
	if (!rhi) return EResult::Fail;
	
	if (!m_Material) return EResult::Fail;
	if (IsFailure(BindPipeline(nullptr, m_Material.Get(), renderPass)))
	    return EResult::Fail;
	
	Texture* texture = m_SpriteHandle->GetTexture();
	
	if(!texture) return EResult::Fail;
	RHITexture* rhiTexture = texture->GetRHITexture();
	if(!rhiTexture) return EResult::Fail;
	tagSamplerDesc pointWrapDesc;
	pointWrapDesc.MinFilter = ESamplerFilter::Point;
	pointWrapDesc.MagFilter = ESamplerFilter::Point;
	pointWrapDesc.AddressU = ESamplerAddressMode::Wrap;
	pointWrapDesc.AddressV = ESamplerAddressMode::Wrap;
	pointWrapDesc.AddressW = ESamplerAddressMode::Wrap;
	RHISampler* sampler = SamplerManager::Get().GetOrCreateSampler(pointWrapDesc);
	
	Rect rect = m_SpriteHandle->GetRegion();
	vec4 uvRegion = {};
	uvRegion.x = rect.Left / texture->GetWorldWidth();
	uvRegion.y = rect.Top / texture->GetWorldHeight();
	uvRegion.z = rect.Right() / texture->GetWorldWidth();
	uvRegion.w = rect.Bottom() / texture->GetWorldHeight();

	RectTransform* rectTransform = GetRectTransform();
	if (!rectTransform) return EResult::Fail;
	
	tagUIUBO uboData;
	uboData.worldMatrix = rectTransform->GetMatrix();
	uboData.uvRegion = uvRegion;
	uboData.tiling = m_Tiling;
	uboData.offset = m_Offset;
	
   if (IsFailure(rhi->BindConstantBuffer((void*)&uboData, sizeof(tagUIUBO), 1, EShaderType::Vertex)))
       return EResult::Fail;

   if (IsFailure(rhi->BindTextureSampler(rhiTexture, sampler, 0)))
       return EResult::Fail;

    return rhi->Draw(6);
}
