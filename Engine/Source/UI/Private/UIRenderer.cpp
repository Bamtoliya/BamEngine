//#pragma once
//#include "UIRenderer.h"
//#include "ComponentRegistry.h"
//#include "RectTransform.h"
//#include "Renderer.h"
//#include "GameObject.h"
//#include "SamplerManager.h"
//#include "ResourceManager.h"
//#include "Camera.h"
//#include "RenderPass.h"
//
//REGISTER_COMPONENT(UIRenderer)
//
//EResult UIRenderer::Initialize(void* arg)
//{
//    return EResult();
//}
//
//UIRenderer* UIRenderer::Create(void* arg)
//{
//	UIRenderer* instance = new UIRenderer();
//    if (IsFailure(instance->Initialize(arg)))
//    {
//		Safe_Release(instance);
//		return nullptr;
//    }
//    return instance;
//}
//
//Component* UIRenderer::Clone(GameObject* owner, void* arg)
//{
//    UIRenderer* instance = new UIRenderer();
//    if(arg)
//    {
//		instance->SetOwner(owner);
//        if (IsFailure(instance->Initialize(arg)))
//        {
//            Safe_Release(instance);
//            return nullptr;
//        }
//    }
//    else
//    {
//		DESC desc;
//        if (IsFailure(instance->Initialize(&desc)))
//        {
//            Safe_Release(instance);
//            return nullptr;
//		}
//    }
//    return instance;
//}
//
//void UIRenderer::Free()
//{
//	__super::Free();
//}
//
//void UIRenderer::LateUpdate(f32 dt)
//{
//	if (m_Active && m_Owner && m_Owner->IsActive() && m_Owner->IsVisible())
//	{
//        MaterialInterface* material = m_Material.Get();
//		if (!material) return;
//
//		const EBlendMode blendMode = material->GetBlendMode();
//		const auto& activePasses = Renderer::Get().GetActiveViewportCameras();
//		for (const auto& passInfo : activePasses)
//		{
//			if (!passInfo.RenderPass) continue;
//
//			const ERenderPassType passType = passInfo.RenderPass->GetPassType();
//
//			if (passType != ERenderPassType::UI) continue;
//			if (!passInfo.RenderPass->IsAcceptsBlendMode(blendMode))
//				continue;
//
//			Renderer::Get().SubmitUI(this, passInfo.RenderPass->GetID());
//		}
//	}
//}
//
//EResult UIRenderer::Render(f32 dt, RenderPass* renderPass)
//{
//    if (!m_SpriteHandle) return EResult::Success;
//
//    RHI* rhi = Renderer::Get().GetRHI();
//    if (!rhi) return EResult::Fail;
//
//	if (!m_Material) return EResult::Fail;
//    if (IsFailure(BindPipeline(nullptr, m_Material.Get(), renderPass)))
//        return EResult::Fail;
//
//	Texture* texture = m_SpriteHandle->GetTexture();
//	
//    if(!texture) return EResult::Fail;
//	RHITexture* rhiTexture = texture->GetRHITexture();
//    if(!rhiTexture) return EResult::Fail;
//    tagSamplerDesc pointWrapDesc;
//    pointWrapDesc.MinFilter = ESamplerFilter::Point;
//    pointWrapDesc.MagFilter = ESamplerFilter::Point;
//    pointWrapDesc.AddressU = ESamplerAddressMode::Wrap;
//    pointWrapDesc.AddressV = ESamplerAddressMode::Wrap;
//    pointWrapDesc.AddressW = ESamplerAddressMode::Wrap;
//    RHISampler* sampler = SamplerManager::Get().GetOrCreateSampler(pointWrapDesc);
//
//    Rect rect = m_SpriteHandle->GetRegion();
//    vec4 uvRegion = {};
//    uvRegion.x = rect.Left / texture->GetWorldWidth();
//    uvRegion.y = rect.Top / texture->GetWorldHeight();
//    uvRegion.z = rect.Right() / texture->GetWorldWidth();
//    uvRegion.w = rect.Bottom() / texture->GetWorldHeight();
//
//    tagUIUBO uboData;
//	uboData.worldMatrix = m_Owner->GetComponent<RectTransform>()->GetMatrix();
//	uboData.uvRegion = uvRegion;
//	uboData.tiling = m_Tiling;
//	uboData.offset = m_Offset;
//
//    if (IsFailure(rhi->BindConstantBuffer((void*)&uboData, sizeof(tagUIUBO), 1, EShaderType::Vertex)))
//        return EResult::Fail;
//
//    if (IsFailure(rhi->BindTextureSampler(rhiTexture, sampler, 0)))
//        return EResult::Fail;
//
//    return rhi->Draw(6);
//}