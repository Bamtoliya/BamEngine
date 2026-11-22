#pragma once

#include "Result.h"
#include "SystemManager.h"
#include "Systems.h"

BEGIN(Engine)

inline EResult RegisterCommonRuntimeSystems(SystemManager& systemManager, bool registerLightSystem = true)
{
    const auto addSystems = [&systemManager]<typename... SystemTypes>()
    {
        return (... && static_cast<bool>(systemManager.AddSystem<SystemTypes>()));
    };

    if (!addSystems.operator()<TransformSystem, SpringArmSystem, CameraSystem>())
        return EResult::Fail;

    if (registerLightSystem && !systemManager.AddSystem<LightSystem>())
        return EResult::Fail;

    if (!addSystems.operator()<
            MeshRenderSystem, SpriteRenderSystem, SpriteAnimationSystem, TilemapRenderSystem,
            AnimationSystem, ParticleSystem, ActionTimelineSystem, CompositeVFXSystem,
            ThirdPersonControllerSystem, KinematicCharacterSystem>())
        return EResult::Fail;

#if !defined(__EMSCRIPTEN__)
    if (!systemManager.AddSystem<NetworkSystem>())
        return EResult::Fail;
#endif

    if (!addSystems.operator()<
            UIRenderSystem, AudioSystem, TerrainSystem, PhysicsSystem, GameplayCombatSystem, GraphSystem, SplineFollowSystem>())
        return EResult::Fail;

    return EResult::Success;
}

END
