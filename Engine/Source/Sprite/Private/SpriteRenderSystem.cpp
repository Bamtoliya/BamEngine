#include "SpriteRenderSystem.h"
#include "WorldComponents.h"
#include "RenderComponents.h"
#include "ResourceHandle.h"
#include "Sprite.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "Renderer.h"
#include "RenderPass.h"
#include "Scene.h"
#include "PickingManager.h"
#include <algorithm>

namespace Engine
{

EResult SpriteRenderSystem::Initialize(void* arg)
{
	return EResult::Success;
}

void SpriteRenderSystem::Free()
{
}

SpriteRenderSystem* SpriteRenderSystem::Create(void* arg)
{
	auto* instance = new SpriteRenderSystem();
	if (IsFailure(instance->Initialize(arg)))
	{
		instance->Free();
		delete instance;
		return nullptr;
	}
	return instance;
}

void SpriteRenderSystem::OnSubmit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
    auto& renderer = Renderer::Get();

    // 뷰포트별 패스와 카메라 정보를 함께 보관
    const auto geometryTargets = renderer.CollectGeometryViewportCameras();
    if (geometryTargets.empty()) return;

    for (Scene* scene : activeScenes)
    {
        if (!scene) continue;

        auto view = scene->GetRegistry().view<SpriteRendererComponent, WorldTransformComponent, FlagComponent>();

        for (auto [entity, spriteRenderer, worldTransform, flag] : view.each())
        {
            if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;

            Sprite* sprite = spriteRenderer.spriteHandle.Get();
            Mesh* mesh = spriteRenderer.meshHandle.Get();
            MaterialInterface* material = spriteRenderer.materialHandle.Get();

            if (!sprite || !mesh || !material) continue;

            Texture* texture = sprite->GetTexture();
            if (!texture || !texture->GetRHITexture()) continue;

            RHITexture* rhiTex = texture->GetRHITexture();
            const f32 texW = static_cast<f32>(rhiTex->GetWidth());
            const f32 texH = static_cast<f32>(rhiTex->GetHeight());
            if (texW <= 0.0f || texH <= 0.0f) continue;

            const Rect region = sprite->GetRegion();
            const f32 regionW = (region.width > 0.0f) ? region.width : texW;
            const f32 regionH = (region.height > 0.0f) ? region.height : texH;
            const f32 ppu = (sprite->GetPixelPerUnit() > 0.0f) ? sprite->GetPixelPerUnit() : 100.0f;
            const vec2 spriteBaseSize = vec2(regionW, regionH) / ppu;

            vec4 baseUV = sprite->GetUVTransform();
            if (baseUV.z <= 0.f || baseUV.w <= 0.f) continue;

            baseUV.x += spriteRenderer.offset.x * baseUV.z;
            baseUV.y += spriteRenderer.offset.y * baseUV.w;
            baseUV.z *= spriteRenderer.tiling.x;
            baseUV.w *= spriteRenderer.tiling.y;

            if (spriteRenderer.flipX)
            {
                baseUV.x += baseUV.z;
                baseUV.z = -baseUV.z;
            }

            if (spriteRenderer.flipY)
            {
                baseUV.y += baseUV.w;
                baseUV.w = -baseUV.w;
            }

            const uint32 pickingId = PickingManager::Get().RegisterPickingEntity(scene, entity);

            for (const auto& target : geometryTargets)
            {
                if (!target.renderPass->IsAcceptsBlendMode(material->GetBlendMode())) continue;

                mat4 finalWorldMatrix = worldTransform.worldMatrix;

                // 빌보드 모드가 활성화되어 있다면 회전 행렬을 카메라 시선에 맞춰 재계산
                if (spriteRenderer.billboardMode != ESpriteBillboardMode::None)
                {
                    vec3 worldPos = vec3(worldTransform.worldMatrix[3]);
                    vec3 scale = vec3(
                        std::max(glm::length(vec3(worldTransform.worldMatrix[0])), 0.0001f),
                        std::max(glm::length(vec3(worldTransform.worldMatrix[1])), 0.0001f),
                        std::max(glm::length(vec3(worldTransform.worldMatrix[2])), 0.0001f));

                    mat4 rotMatrix = mat4(1.0f);

                    if (spriteRenderer.billboardMode == ESpriteBillboardMode::CameraFacing)
                    {
                        vec3 camRight = glm::normalize(vec3(target.cameraBuffer.invViewMatrix[0]));
                        vec3 camUp = glm::normalize(vec3(target.cameraBuffer.invViewMatrix[1]));
                        vec3 camForward = glm::normalize(vec3(target.cameraBuffer.invViewMatrix[2]));
                        rotMatrix = mat4(
                            vec4(camRight, 0.0f),
                            vec4(camUp, 0.0f),
                            vec4(camForward, 0.0f),
                            vec4(0.0f, 0.0f, 0.0f, 1.0f));
                    }
                    else if (spriteRenderer.billboardMode == ESpriteBillboardMode::AxialY)
                    {
                        vec3 lookDir = target.cameraBuffer.cameraPosition - worldPos;
                        lookDir.y = 0.0f;
                        if (glm::dot(lookDir, lookDir) > 0.0001f)
                        {
                            lookDir = glm::normalize(lookDir);
                            vec3 up = vec3(0.0f, 1.0f, 0.0f);
                            vec3 right = glm::normalize(glm::cross(lookDir, up));
                            vec3 forward = glm::cross(right, up);
                            rotMatrix = mat4(
                                vec4(right, 0.0f),
                                vec4(up, 0.0f),
                                vec4(forward, 0.0f),
                                vec4(0.0f, 0.0f, 0.0f, 1.0f));
                        }
                    }

                    finalWorldMatrix = glm::translate(mat4(1.0f), worldPos) * rotMatrix * glm::scale(mat4(1.0f), scale);
                }

                // ── 렌더 모드별 드로우 분기 ──
                if (spriteRenderer.drawMode == ESpriteDrawMode::Simple)
                {
                    // [Simple 모드] 기존 단일 쿼드
                    SpriteDrawCommand command;
                    mat4 quadMatrix;
                    if (!sprite->TryGetQuadMatrix(quadMatrix)) continue;

                    command.mesh = mesh;
                    command.texture = texture;
                    command.material = material;
                    command.color = spriteRenderer.color;
                    command.uvTransform = baseUV;
                    command.pickingId = pickingId;
                    command.sortingLayerID = spriteRenderer.sortingLayerID;
                    command.orderInLayer = spriteRenderer.orderInLayer;
                    command.worldMatrix = finalWorldMatrix * quadMatrix;
                    renderer.SubmitSprite(command, target.renderPass->GetID());
                }
                else if (spriteRenderer.drawMode == ESpriteDrawMode::Sliced)
                {
                    // [Sliced 모드 (9-Slice)]
                    const vec4 border = sprite->GetBorder(); // Left, Right, Top, Bottom (픽셀)
                    const vec2 targetSize = vec2(
                        (spriteRenderer.size.x > 0.0f) ? spriteRenderer.size.x : spriteBaseSize.x,
                        (spriteRenderer.size.y > 0.0f) ? spriteRenderer.size.y : spriteBaseSize.y);

                    const f32 bL = border.x / ppu;
                    const f32 bR = border.y / ppu;
                    const f32 bT = border.z / ppu;
                    const f32 bB = border.w / ppu;

                    const f32 centerW = (std::max)(0.0f, targetSize.x - bL - bR);
                    const f32 centerH = (std::max)(0.0f, targetSize.y - bT - bB);

                    // UV 분할 계산
                    const f32 u0 = region.left / texW;
                    const f32 u1 = (region.left + border.x) / texW;
                    const f32 u2 = (region.left + regionW - border.y) / texW;
                    const f32 u3 = (region.left + regionW) / texW;

                    const f32 v0 = region.top / texH;
                    const f32 v1 = (region.top + border.z) / texH;
                    const f32 v2 = (region.top + regionH - border.w) / texH;
                    const f32 v3 = (region.top + regionH) / texH;

                    const f32 xSizes[3] = { bL, centerW, bR };
                    const f32 ySizes[3] = { bB, centerH, bT };
                    const f32 uCoords[4] = { u0, u1, u2, u3 };
                    const f32 vCoords[4] = { v3, v2, v1, v0 }; // V좌표는 하단부터 상단으로

                    const vec2 pivot = sprite->GetPivot();
                    const vec2 origin = -pivot * targetSize;

                    f32 currY = origin.y;
                    for (int y = 0; y < 3; ++y)
                    {
                        const f32 curH = ySizes[y];
                        if (curH <= 0.0001f) { continue; }

                        f32 currX = origin.x;
                        for (int x = 0; x < 3; ++x)
                        {
                            const f32 curW = xSizes[x];
                            if (curW <= 0.0001f) { continue; }

                            const vec2 patchCenter = vec2(currX + curW * 0.5f, currY + curH * 0.5f);
                            const mat4 patchMatrix = glm::translate(mat4(1.0f), vec3(patchCenter, 0.0f)) *
                                                     glm::scale(mat4(1.0f), vec3(curW * 0.5f, curH * 0.5f, 1.0f));

                            SpriteDrawCommand command;
                            command.mesh = mesh;
                            command.texture = texture;
                            command.material = material;
                            command.color = spriteRenderer.color;
                            command.uvTransform = vec4(uCoords[x], vCoords[y + 1], uCoords[x + 1] - uCoords[x], vCoords[y] - vCoords[y + 1]);
                            command.pickingId = pickingId;
                            command.sortingLayerID = spriteRenderer.sortingLayerID;
                            command.orderInLayer = spriteRenderer.orderInLayer;
                            command.worldMatrix = finalWorldMatrix * patchMatrix;
                            renderer.SubmitSprite(command, target.renderPass->GetID());

                            currX += curW;
                        }
                        currY += curH;
                    }
                }
                else if (spriteRenderer.drawMode == ESpriteDrawMode::Tiled)
                {
                    // [Tiled 모드] 바둑판식 타일 반복
                    const vec2 targetSize = vec2(
                        (spriteRenderer.size.x > 0.0f) ? spriteRenderer.size.x : spriteBaseSize.x,
                        (spriteRenderer.size.y > 0.0f) ? spriteRenderer.size.y : spriteBaseSize.y);

                    const vec2 unitSize = spriteBaseSize;
                    if (unitSize.x <= 0.0001f || unitSize.y <= 0.0001f) continue;

                    const vec2 pivot = sprite->GetPivot();
                    const vec2 origin = -pivot * targetSize;

                    for (f32 y = 0.0f; y < targetSize.y; y += unitSize.y)
                    {
                        const f32 curH = (std::min)(unitSize.y, targetSize.y - y);
                        const f32 fracY = curH / unitSize.y;

                        for (f32 x = 0.0f; x < targetSize.x; x += unitSize.x)
                        {
                            const f32 curW = (std::min)(unitSize.x, targetSize.x - x);
                            const f32 fracX = curW / unitSize.x;

                            const vec2 patchCenter = origin + vec2(x + curW * 0.5f, y + curH * 0.5f);
                            const mat4 patchMatrix = glm::translate(mat4(1.0f), vec3(patchCenter, 0.0f)) *
                                                     glm::scale(mat4(1.0f), vec3(curW * 0.5f, curH * 0.5f, 1.0f));

                            SpriteDrawCommand command;
                            command.mesh = mesh;
                            command.texture = texture;
                            command.material = material;
                            command.color = spriteRenderer.color;
                            command.uvTransform = vec4(baseUV.x, baseUV.y, baseUV.z * fracX, baseUV.w * fracY);
                            command.pickingId = pickingId;
                            command.sortingLayerID = spriteRenderer.sortingLayerID;
                            command.orderInLayer = spriteRenderer.orderInLayer;
                            command.worldMatrix = finalWorldMatrix * patchMatrix;
                            renderer.SubmitSprite(command, target.renderPass->GetID());
                        }
                    }
                }
                else if (spriteRenderer.drawMode == ESpriteDrawMode::Filled)
                {
                    // [Filled 모드] 게이지 채우기
                    const f32 amount = std::clamp(spriteRenderer.fillAmount, 0.0f, 1.0f);
                    if (amount <= 0.0001f) continue;

                    vec2 filledSize = spriteBaseSize;
                    vec4 filledUV = baseUV;
                    vec2 pivotOffset = (vec2(0.5f) - sprite->GetPivot()) * spriteBaseSize;

                    if (spriteRenderer.fillMethod == ESpriteFillMethod::Horizontal)
                    {
                        filledSize.x *= amount;
                        filledUV.z *= amount;
                        // 좌측 기준 정렬
                        pivotOffset.x = (-sprite->GetPivot().x + amount * 0.5f) * spriteBaseSize.x;
                    }
                    else if (spriteRenderer.fillMethod == ESpriteFillMethod::Vertical)
                    {
                        filledSize.y *= amount;
                        filledUV.w *= amount;
                        // 하단 기준 정렬
                        pivotOffset.y = (-sprite->GetPivot().y + amount * 0.5f) * spriteBaseSize.y;
                    }

                    const mat4 quadMatrix = glm::translate(mat4(1.0f), vec3(pivotOffset, 0.0f)) *
                                            glm::scale(mat4(1.0f), vec3(filledSize * 0.5f, 1.0f));

                    SpriteDrawCommand command;
                    command.mesh = mesh;
                    command.texture = texture;
                    command.material = material;
                    command.color = spriteRenderer.color;
                    command.uvTransform = filledUV;
                    command.pickingId = pickingId;
                    command.sortingLayerID = spriteRenderer.sortingLayerID;
                    command.orderInLayer = spriteRenderer.orderInLayer;
                    command.worldMatrix = finalWorldMatrix * quadMatrix;
                    renderer.SubmitSprite(command, target.renderPass->GetID());
                }
            }
        }
    }
}

} // namespace Engine
