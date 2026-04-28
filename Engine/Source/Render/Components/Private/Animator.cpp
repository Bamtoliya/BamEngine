#pragma once
#include "Animator.h"
#include "ComponentRegistry.h"

REGISTER_COMPONENT(Animator)

#pragma region Constructor&Destructor
EResult Animator::Initialize(void* arg)
{
    return EResult::Success;
}

Animator* Animator::Create(void* arg)
{
    Animator* instance = new Animator();
    if (IsFailure(instance->Initialize(arg)))
    {
		Safe_Release(instance);
        return nullptr;
    }
    return instance;
}

void Animator::Free()
{
    m_FinalBoneMatrices.clear();
    m_GlobalTransforms.clear();
    m_States.clear();
}
#pragma endregion

void Animator::SetSkeleton(ResourceHandle<Skeleton> skeleton)
{
    m_Skeleton = skeleton;
    // 뼈 갯수가 로드되면 사이즈를 예약합니다. 
    // 예: 최대 100개, 혹은 실제 뼈 갯수.
    if (m_Skeleton.IsValid())
    {
        uint32 boneCount = m_Skeleton->GetBones().size();
        m_FinalBoneMatrices.assign(boneCount, glm::identity<mat4>());
        m_GlobalTransforms.assign(boneCount, glm::identity<mat4>());
    }
}

void Animator::AddState(const wstring& stateName, ResourceHandle<Animation> clip, bool loop, f32 speed)
{
    tagAnimationState state;
    state.Name = stateName;
    state.Clip = clip;
    state.Loop = loop;
    state.PlaybackSpeed = speed;
    m_States[stateName] = state;
}

void Animator::Play(const wstring& stateName, f32 blendTime)
{
    auto it = m_States.find(stateName);
    if (it != m_States.end())
    {
        m_CurrentState = &it->second;
        m_CurrentTime = 0.0f;
    }
}

void Animator::Update(f32 dt)
{
    // [임시 코드] 실행 중인 애니메이션이 없는데 등록된 상태가 있다면 첫 번째(0번) 상태 강제 재생
    if (!m_CurrentState && !m_States.empty())
    {
        m_CurrentState = &m_States.begin()->second;
        m_CurrentTime = 0.0f;
    }

    if (!m_CurrentState || !m_CurrentState->Clip.IsValid() || !m_Skeleton.IsValid())
        return;

    auto anim = m_CurrentState->Clip;
    f32 ticksPerSec = (anim->GetTicksPerSecond() != 0.0) ? (f32)anim->GetTicksPerSecond() : 25.0f;

    // 시간 흐름 적용
    m_CurrentTime += dt * ticksPerSec * m_CurrentState->PlaybackSpeed;

    // 반복 처리 로직
    if (m_CurrentTime >= anim->GetDuration())
    {
        if (m_CurrentState->Loop)
            m_CurrentTime = fmod(m_CurrentTime, (f32)anim->GetDuration());
        else
            m_CurrentTime = (f32)anim->GetDuration();
    }

    // 루트 본(Root Bone)부터 재귀적, 혹은 계층적 배열 순서로 연산 진행
    const auto& bones = m_Skeleton->GetBones();
    if (!bones.empty())
    {
        CalculateBoneTransforms(m_CurrentTime, anim, glm::identity<mat4>(), 0);
    }
}

void Animator::CalculateBoneTransforms(f32 time, ResourceHandle<Animation> clip, const mat4& parentTransform, int32 boneIndex)
{
    const auto& bone = m_Skeleton->GetBones()[boneIndex];
    mat4 nodeTransform = bone.LocalTransform; // 애니메이션 트랙이 없는 노드를 위해 로컬 트랜스폼으로 초기화

    // 해당 본과 이름이 맞는 애니메이션 트랙 찾기 (최적화를 원한다면 map 사용 가능)
    const AnimationTrack* pTrack = nullptr;
    for (const auto& track : clip->GetTracks())
    {
        if (track.TargetBoneName == bone.Name)
        {
            pTrack = &track;
            break;
        }
    }

    if (pTrack)
    {
        // Translation, Rotation, Scale 보간 계산
        vec3 t = InterpolateTranslation(time, *pTrack);
        quat r = InterpolateRotation(time, *pTrack);
        vec3 s = InterpolateScale(time, *pTrack);

        // SRT 결합
        mat4 matScale = glm::scale(glm::identity<mat4>(), s);
        mat4 matRot = glm::mat4_cast(r);
        mat4 matTrans = glm::translate(glm::identity<mat4>(), t);

        nodeTransform = matTrans * matRot * matScale;
    }

    // 부모 트랜스폼 누적
    mat4 globalTransform = parentTransform * nodeTransform;
    m_GlobalTransforms[boneIndex] = globalTransform;

    // 최종 GPU Skinning 매트릭스는: 부모누적트랜스폼 x 오프셋 매트릭스(바인드포즈 역행렬 변환)
    m_FinalBoneMatrices[boneIndex] = globalTransform * bone.OffsetMatrix;

    // 자식 본들에게 재귀
    const auto& bones = m_Skeleton->GetBones();
    for (uint32 childIndex = 0; childIndex < bones.size(); ++childIndex)
    {
        if (bones[childIndex].ParentIndex == boneIndex)
        {
            CalculateBoneTransforms(time, clip, globalTransform, childIndex);
        }
    }
}

// -----------------------------------------------------
// 보간 헬퍼 함수 (최초 프레임 탐색)
// -----------------------------------------------------
vec3 Animator::InterpolateTranslation(f32 time, const AnimationTrack& track)
{
    if (track.PositionKeyframes.empty()) return vec3(0.0f);
    if (track.PositionKeyframes.size() == 1) return track.PositionKeyframes[0].Value;

    // 간단한 선형 탐색 (이분 탐색/인덱스 캐싱 등으로 최적화 가능)
    for (size_t i = 0; i < track.PositionKeyframes.size() - 1; ++i)
    {
        if (time < (f32)track.PositionKeyframes[i + 1].Time)
        {
            f32 dt = (f32)(track.PositionKeyframes[i + 1].Time - track.PositionKeyframes[i].Time);
            f32 factor = (time - (f32)track.PositionKeyframes[i].Time) / dt;
            return glm::mix(track.PositionKeyframes[i].Value, track.PositionKeyframes[i + 1].Value, factor);
        }
    }
    return track.PositionKeyframes.back().Value;
}

quat Animator::InterpolateRotation(f32 time, const AnimationTrack& track)
{
    if (track.RotationKeyframes.empty()) return quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (track.RotationKeyframes.size() == 1) return track.RotationKeyframes[0].Value;

    for (size_t i = 0; i < track.RotationKeyframes.size() - 1; ++i)
    {
        if (time < (f32)track.RotationKeyframes[i + 1].Time)
        {
            f32 dt = (f32)(track.RotationKeyframes[i + 1].Time - track.RotationKeyframes[i].Time);
            f32 factor = (time - (f32)track.RotationKeyframes[i].Time) / dt;
            // 구면 선형 보간 (Slerp)
            return glm::slerp(track.RotationKeyframes[i].Value, track.RotationKeyframes[i + 1].Value, factor);
        }
    }
    return track.RotationKeyframes.back().Value;
}

vec3 Animator::InterpolateScale(f32 time, const AnimationTrack& track)
{
    if (track.ScaleKeyframes.empty()) return vec3(1.0f);
    if (track.ScaleKeyframes.size() == 1) return track.ScaleKeyframes[0].Value;

    for (size_t i = 0; i < track.ScaleKeyframes.size() - 1; ++i)
    {
        if (time < (f32)track.ScaleKeyframes[i + 1].Time)
        {
            f32 dt = (f32)(track.ScaleKeyframes[i + 1].Time - track.ScaleKeyframes[i].Time);
            f32 factor = (time - (f32)track.ScaleKeyframes[i].Time) / dt;
            return glm::mix(track.ScaleKeyframes[i].Value, track.ScaleKeyframes[i + 1].Value, factor);
        }
    }
    return track.ScaleKeyframes.back().Value;
}