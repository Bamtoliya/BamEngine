#include "AnimatorPoseUpdate.h"
#include "AnimationEvaluator.h"
#include "Components/Render/AnimatorComponent.h"

namespace Engine
{
    namespace
    {
        void AdvanceClipPlayback(const Animation& clip, float step, bool isLooping, float& time, bool& isPlaying)
        {
            const f64 tps = clip.GetTicksPerSecond() > 0.0 ? clip.GetTicksPerSecond() : 25.0;
            const float duration = clip.GetDuration() > 0.0 ? static_cast<float>(clip.GetDuration() / tps) : 0.0f;

            time += step;
            if (!isLooping && duration > 0.0f && time >= duration)
            {
                time = duration;
                isPlaying = false;
            }
        }
    }

    EAnimatorBasePoseSource UpdateAnimatorBasePose(AnimatorComponent& animator, const Skeleton& skeleton, float dt)
    {
        auto& localPose = animator.localBonePose;
        localPose.clear();

        if (!animator.isPlaying) return EAnimatorBasePoseSource::None;

        const float step = dt * animator.playRate;
        if (animator.runtimeState.HasStateMachine())
        {
            animator.runtimeState.UpdateLocalPose(step, skeleton, localPose);
            animator.currentTime = animator.runtimeState.GetStateTime();
            return EAnimatorBasePoseSource::StateMachine;
        }

        const auto* clip = animator.currentClip.Get();
        if (!clip) return EAnimatorBasePoseSource::None;

        AdvanceClipPlayback(*clip, step, animator.isLooping, animator.currentTime, animator.isPlaying);
        AnimationEvaluator::EvaluateLocalPose(*clip, skeleton, animator.currentTime, localPose, animator.isLooping);
        return EAnimatorBasePoseSource::Clip;
    }

    void UpdateAnimatorSkinning(AnimatorComponent& animator, const Skeleton& skeleton, float dt)
    {
        auto& localPose = animator.localBonePose;
        if (localPose.empty()) return;

        for (auto& layer : animator.layers)
        {
            if (!layer.isPlaying || layer.weight <= 0.0f) continue;

            const auto* clip = layer.clip.Get();
            if (!clip) continue;

            if (layer.mask.boneWeights.size() != skeleton.GetBones().size())
            {
                layer.mask.BuildWeights(skeleton);
            }

            AdvanceClipPlayback(*clip, dt * layer.playRate, layer.isLooping, layer.currentTime, layer.isPlaying);
            AnimationEvaluator::ApplyMaskedLocalPose(*clip, layer.currentTime, layer.mask.boneWeights,
                layer.weight, skeleton, localPose, layer.isLooping);
        }

        AnimationEvaluator::BuildSkinningMatrices(skeleton, localPose, animator.finalBoneMatrices);
    }
}