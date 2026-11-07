#include "TilemapRenderSystem.h"
#include "CoreComponents.h"
#include "RenderComponents.h"
#include "Sprite.h"
#include "Mesh.h"
#include "MaterialInterface.h"
#include "Renderer.h"
#include "RenderPass.h"
#include "Scene.h"
#include "PickingManager.h"

namespace Engine
{

EResult TilemapRenderSystem::Initialize(void* arg)
{
	(void)arg;
	return EResult::Success;
}

void TilemapRenderSystem::Free()
{
}

TilemapRenderSystem* TilemapRenderSystem::Create(void* arg)
{
	auto* instance = new TilemapRenderSystem();
	if (IsFailure(instance->Initialize(arg)))
	{
		instance->Free();
		delete instance;
		return nullptr;
	}
	return instance;
}

void TilemapRenderSystem::OnSubmit(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
{
	(void)registry;
	(void)dt;

	auto& renderer = Renderer::Get();
	const auto geometryTargets = renderer.CollectGeometryViewportCameras();
	if (geometryTargets.empty()) return;

	for (Scene* scene : activeScenes)
	{
		if (!scene) continue;

		auto view = scene->GetRegistry().view<TilemapComponent, WorldTransformComponent, FlagComponent>();

		for (auto [entity, tilemap, worldTransform, flag] : view.each())
		{
			if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;

			Mesh* mesh = tilemap.meshHandle.Get();
			MaterialInterface* material = tilemap.materialHandle.Get();
			if (!mesh || !material) continue;

			const mat4& worldMatrix = worldTransform.worldMatrix;
			const uint32 pickingId = PickingManager::Get().RegisterPickingEntity(scene, entity);

			for (const auto& target : geometryTargets)
			{
				if (!target.renderPass->IsAcceptsBlendMode(material->GetBlendMode())) continue;

				// 청크 순회
				for (const auto& [chunkKey, chunk] : tilemap.chunks)
				{
					if (!chunk.hasTiles) continue;

					const int32 cx = static_cast<int32>(chunkKey >> 32);
					const int32 cy = static_cast<int32>(chunkKey & 0xFFFFFFFF);

					for (int32 ly = 0; ly < CHUNK_SIZE; ++ly)
					{
						for (int32 lx = 0; lx < CHUNK_SIZE; ++lx)
						{
							const int32 idx = ly * CHUNK_SIZE + lx;
							const auto& cell = chunk.tiles[idx];
							if (!cell.sprite.IsValid() || !cell.sprite.Get()) continue;

							auto* sprite = cell.sprite.Get();
							auto* texture = sprite->GetTexture();
							if (!texture || !texture->GetRHITexture()) continue;

							const int32 gx = cx * CHUNK_SIZE + lx;
							const int32 gy = cy * CHUNK_SIZE + ly;

							const vec2 cellPos = tilemap.CellToWorld(gx, gy);
							const vec2 cellSize = tilemap.cellSize;
							const vec2 tileCenter = cellPos + cellSize * 0.5f;

							const mat4 localQuad = glm::translate(mat4(1.0f), vec3(tileCenter, 0.0f)) *
							                       glm::scale(mat4(1.0f), vec3(cellSize * 0.5f, 1.0f));

							SpriteDrawCommand command;
							command.mesh = mesh;
							command.texture = texture;
							command.material = material;
							command.color = cell.color;
							command.uvTransform = sprite->GetUVTransform();
							command.pickingId = pickingId;
							command.worldMatrix = worldMatrix * localQuad;

							renderer.SubmitSprite(command, target.renderPass->GetID());
						}
					}
				}
			}
		}
	}
}

} // namespace Engine
