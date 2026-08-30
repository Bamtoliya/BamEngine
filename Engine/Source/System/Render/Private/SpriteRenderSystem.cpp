#include "SpriteRenderSystem.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "ResourceHandle.h"
#include "ResourceHandle.inl"
#include "Sprite.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "Renderer.h"


IMPLEMENT_SINGLETON(SpriteRenderSystem)

EResult SpriteRenderSystem::Initialize(void* arg)
{
	return EResult();
}

void SpriteRenderSystem::Free()
{
}

void SpriteRenderSystem::OnSubmit(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt)
{
	for (Scene* scene : activeScenes)
	{
		auto view = scene->GetRegistry().view<SpriteRendererComponent, WorldTransformComponent, FlagComponent>();
		for (auto [entity, spriteRenderer, worldTransform, flag] : view.each())
		{
			if(!HasFlag(flag.flags, EEntityFlag::Visible)) continue;

			Sprite* pSprite = spriteRenderer.spriteHandle.Get();
			if (!pSprite) continue;

			Mesh* pMesh = nullptr;
			if (spriteRenderer.drawMode == ESpriteDrawMode::Simple)
			{
				TODO("SpriteRendererComponent의 drawMode가 Simple일 때, pMesh를 어떻게 가져올지 결정해야함");
				//pMesh = Renderer::Get().GetQuadMesh();
			} 
			else
			{
				pMesh = spriteRenderer.meshHandle.Get();
			}

			if (!pMesh) continue;

			vec4 finalUV = pSprite->GetRegionUV();

			if (spriteRenderer.flipX)
			{
				finalUV.x = finalUV.z - finalUV.x;
				finalUV.z *= -1.0f;
			}

			if (spriteRenderer.flipY)
			{
				finalUV.y = finalUV.w - finalUV.y;
				finalUV.w *= -1.0f;
			}

			finalUV.x += spriteRenderer.offset.x * finalUV.z;
			finalUV.y += spriteRenderer.offset.y * finalUV.w;
			finalUV.z *= spriteRenderer.tiling.x;
			finalUV.w *= spriteRenderer.tiling.y;

			SpriteDrawCommand cmd;
			cmd.texture = pSprite->GetTexture();
			cmd.mesh = pMesh;
			cmd.material = spriteRenderer.materialHandle.Get();
			if (!cmd.material)
			{
				TODO("SpriteRendererComponent의 materialHandle이 없을 때, 기본 Material을 가져오는 로직 필요");
				//cmd.material = Renderer::Get().GetDefaultMaterial();
			}

			cmd.worldMatrix = worldTransform.worldMatrix;
			cmd.color = spriteRenderer.color;
			cmd.uvTransform = finalUV;

			Renderer::Get().SubmitSprite(cmd, 0);
		}
	}
}
