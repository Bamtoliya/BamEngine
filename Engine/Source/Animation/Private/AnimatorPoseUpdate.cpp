#include "AnimatorPoseUpdate.h"
#include "AnimationEvaluator.h"
#include "AnimatorComponent.h"

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

        // 이전 포즈에서 현재 기본 포즈로 섞는다. 섞기가 끝나거나 본 수가 다르면(스켈레톤 교체) 이전 포즈를 버린다.
        void ApplyBlendFromPose(AnimatorComponent& animator, float dt)
        {
            auto& from = animator.blendFromPose;
            auto& localPose = animator.localBonePose;
            if (from.empty()) return;

            animator.blendFromElapsed += dt;
            const float t = animator.blendFromElapsed / animator.blendFromDuration;
            if (t >= 1.0f || from.size() != localPose.size()) { from.clear(); return; }

            const float alpha = t * t * (3.0f - 2.0f * t); // smoothstep: 시작과 끝을 부드럽게
            for (size_t i = 0; i < localPose.size(); ++i)
                localPose[i] = AnimationEvaluator::BlendBoneLocalPose(from[i], localPose[i], alpha);
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
        ApplyBlendFromPose(animator, dt);

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