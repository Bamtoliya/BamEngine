#include "DecalCollector.h"

#include "ComponentEnable.h"
#include "DecalComponent.h"
#include "FlagComponent.h"
#include "Scene.h"
#include "TimeManager.h"
#include "WorldTransformComponent.h"

void Engine::CollectDecalComponents(const Scene& scene, std::vector<DecalDrawData>& output)
{
	const f32 totalTime = TimeManager::IsCreated() ? static_cast<f32>(TimeManager::Get().GetTotalTime()) : 0.0f;
	const auto& world = scene.GetRegistry();
	auto view = world.view<const DecalComponent, const WorldTransformComponent, const FlagComponent>();
	for (auto [entity, decal, transform, flag] : view.each())
	{
		if (!IsComponentEnabled<DecalComponent>(world, entity)) continue;
		if (!flag.IsActiveInHierarchy() || !flag.IsVisibleInHierarchy() || HasFlag(flag.effectiveFlags, EEntityFlag::Dead)) continue;
		if (decal.Opacity <= 0.0f) continue;

		f32 lifeAlpha = 1.0f;
		if (decal.lifetime > 0.0f)
		{
			if (decal.elapsedTime >= decal.lifetime) continue;
			const f32 fadeDuration = std::max(0.0f, decal.fadeOutDuration);
			const f32 fadeStart = std::max(0.0f, decal.lifetime - fadeDuration);
			lifeAlpha = GetDecalTrackAlpha(decal.elapsedTime, decal.lifetime, fadeStart);
			if (lifeAlpha <= 0.0f) continue;
		}

		DecalDrawData data;
		data.BoxToWorld = MakeDecalBoxMatrix(transform.worldMatrix, decal.Size);
		mat4 worldToBox;
		if (!TryGetDecalWorldToBox(data.BoxToWorld, worldToBox)) continue;

		const f32 animTime = (decal.lifetime > 0.0f) ? decal.elapsedTime : totalTime;
		const vec2 uvOffset = decal.uvOffset + decal.uvScrollSpeed * animTime;

		data.BaseColor = decal.BaseColor;
		data.BaseColorTexture = decal.BaseColorTexture.Get();
		data.NormalTexture = decal.NormalTexture.Get();
		data.PBRTexture = decal.PBRTexture.Get();
		data.Opacity = decal.Opacity * lifeAlpha;
		data.NormalOpacity = decal.NormalOpacity;
		data.PBROpacity = decal.PBROpacity;
		data.Roughness = decal.Roughness;
		data.Metallic = decal.Metallic;
		data.MaxAngle = decal.MaxAngle;
		data.DepthFade = decal.DepthFade;
		data.SortOrder = decal.SortOrder;
		data.UVScaleOffset = vec4(decal.uvScale.x, decal.uvScale.y, uvOffset.x, uvOffset.y);
		output.push_back(data);
	}
}
