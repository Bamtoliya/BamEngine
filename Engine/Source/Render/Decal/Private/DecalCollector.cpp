#include "DecalCollector.h"

#include "ComponentEnable.h"
#include "DecalComponent.h"
#include "FlagComponent.h"
#include "Scene.h"
#include "WorldTransformComponent.h"

void Engine::CollectDecalComponents(const Scene& scene, std::vector<DecalDrawData>& output)
{
	const auto& world = scene.GetRegistry();
	auto view = world.view<const DecalComponent, const WorldTransformComponent, const FlagComponent>();
	for (auto [entity, decal, transform, flag] : view.each())
	{
		if (!IsComponentEnabled<DecalComponent>(world, entity)) continue;
		if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;
		if (decal.Opacity <= 0.0f) continue;

		DecalDrawData data;
		data.BoxToWorld = MakeDecalBoxMatrix(transform.worldMatrix, decal.Size);
		mat4 worldToBox;
		if (!TryGetDecalWorldToBox(data.BoxToWorld, worldToBox)) continue;

		data.BaseColor = decal.BaseColor;
		data.BaseColorTexture = decal.BaseColorTexture.Get();
		data.NormalTexture = decal.NormalTexture.Get();
		data.PBRTexture = decal.PBRTexture.Get();
		data.Opacity = decal.Opacity;
		data.NormalOpacity = decal.NormalOpacity;
		data.PBROpacity = decal.PBROpacity;
		data.Roughness = decal.Roughness;
		data.Metallic = decal.Metallic;
		data.MaxAngle = decal.MaxAngle;
		data.DepthFade = decal.DepthFade;
		data.SortOrder = decal.SortOrder;
		output.push_back(data);
	}
}
