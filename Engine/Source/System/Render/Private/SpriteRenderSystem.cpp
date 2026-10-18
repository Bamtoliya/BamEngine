#include "SpriteRenderSystem.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "ResourceHandle.h"
#include "Sprite.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "Renderer.h"
#include "RenderPass.h"
#include "Scene.h"
#include "PickingManager.h"

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

void SpriteRenderSystem::OnSubmit(entt::registry& registry, const vector<Scene*> activeScenes, f32 dt)
{
    auto& renderer = Renderer::Get();

    // 뷰포트별 패스와 카메라 정보를 함께 보관
    struct GeometryTarget
    {
        RenderPass* pass = nullptr;
        CameraBuffer cameraBuffer;
    };
    vector<GeometryTarget> geometryTargets;

    for (const auto& viewport : renderer.GetActiveViewportCameras())
    {
        RenderPass* pass = viewport.renderPass;
        if (!pass || !viewport.hasCamera || pass->GetPassType() != ERenderPassType::Geometry) continue;

        auto it = std::find_if(geometryTargets.begin(), geometryTargets.end(),
            [pass](const GeometryTarget& t) { return t.pass == pass; });
        if (it == geometryTargets.end())
        {
            geometryTargets.push_back({ pass, viewport.cameraBuffer });
        }
    }

    if (geometryTargets.empty()) return;

    for (Scene* scene : activeScenes)
    {
        if (!scene) continue;

        auto view = scene->GetRegistry().view<SpriteRendererComponent, WorldTransformComponent, FlagComponent>();

        for (auto [entity, spriteRenderer, worldTransform, flag] : view.each())
        {
            if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;

            if (spriteRenderer.drawMode != ESpriteDrawMode::Simple) continue;

            Sprite* sprite = spriteRenderer.spriteHandle.Get();
            Mesh* mesh = spriteRenderer.meshHandle.Get();
            MaterialInterface* material = spriteRenderer.materialHandle.Get();

            if (!sprite || !mesh || !material) continue;

            Texture* texture = sprite->GetTexture();
            if (!texture || !texture->GetRHITexture()) continue;

            vec4 uv = sprite->GetUVTransform();
            if (uv.z <= 0.f || uv.w <= 0.f) continue;

            uv.x += spriteRenderer.offset.x * uv.z;
            uv.y += spriteRenderer.offset.y * uv.w;
            uv.z *= spriteRenderer.tiling.x;
            uv.w *= spriteRenderer.tiling.y;

            if (spriteRenderer.flipX)
            {
                uv.x += uv.z;
                uv.z = -uv.z;
            }

            if (spriteRenderer.flipY)
            {
                uv.y += uv.w;
                uv.w = -uv.w;
            }

            SpriteDrawCommand command;
            mat4 quadMatrix;
            if (!sprite->TryGetQuadMatrix(quadMatrix)) continue;
            command.mesh = mesh;
            command.texture = texture;
            command.material = material;
            command.color = spriteRenderer.color;
            command.uvTransform = uv;
            command.pickingId = PickingManager::Get().RegisterPickingEntity(scene, entity);

            for (const auto& target : geometryTargets)
            {
                if (!target.pass->IsAcceptsBlendMode(material->GetBlendMode())) continue;

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
                        // 완전 구면 빌보드: 카메라 역행렬의 Right, Up, Forward 축과 일치
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
                        // Y축 고정 빌보드: Y축은 월드 수직 고정, 카메라 위치를 향해 좌우로만 회전
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

                command.worldMatrix = finalWorldMatrix * quadMatrix;
                renderer.SubmitSprite(command, target.pass->GetID());
            }
        }
    }
}