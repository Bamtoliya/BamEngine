#pragma once

#include "Engine_API.h"

namespace Engine
{
    struct AnimatorComponent;
    class Skeleton;

    enum class EAnimatorBasePoseSource
    {
        None,
        Clip,
        StateMachine,
    };

    [[nodiscard]] ENGINE_API EAnimatorBasePoseSource UpdateAnimatorBasePose(
        AnimatorComponent& animator, const Skeleton& skeleton, float dt);

    ENGINE_API void UpdateAnimatorSkinning(AnimatorComponent& animator, const Skeleton& skeleton, float dt);
}