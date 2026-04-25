#pragma once

#include "Component.h"
#include "Animation.h"
#include "Skeleton.h"
#include "ResourceHandle.h"
#include "ResourceHandle.inl"

BEGIN(Engine)

// State Machine을 위한 간이 상태 구조체
struct tagAnimationState
{
    wstring Name;
    ResourceHandle<Animation> Clip;
    bool Loop = true;
    f32 PlaybackSpeed = 1.0f;
};

CLASS()
class ENGINE_API Animator : public Component
{
    REFLECT_CLASS()
private:
    Animator() {}
	virtual ~Animator() = default;
    virtual EResult Initialize(void* arg = nullptr) override;
public:
	static Animator* Create(void* arg = nullptr);
    virtual void Free() override;
    virtual void Update(f32 dt); // 컴포넌트 업데이트 시 호출
public:
    void SetSkeleton(ResourceHandle<Skeleton> skeleton);

    // 상태 머신 제어
    void AddState(const wstring& stateName, ResourceHandle<Animation> clip, bool loop = true, f32 speed = 1.0f);
    void Play(const wstring& stateName, f32 blendTime = 0.0f); // 블렌딩은 추후 확장을 위해 예약

    // MeshRenderer 등에서 가져갈 최종 GPU 행렬 배열 (크기: MAX_BONES=100 가정)
    const vector<mat4>& GetFinalBoneMatrices() const { return m_FinalBoneMatrices; }

private:
    void CalculateBoneTransforms(f32 timeInSeconds, ResourceHandle<Animation> clip, const mat4& parentTransform, int32 boneIndex);

    // 키프레임 보간
    vec3 InterpolateTranslation(f32 time, const AnimationTrack& track);
    quat InterpolateRotation(f32 time, const AnimationTrack& track);
    vec3 InterpolateScale(f32 time, const AnimationTrack& track);

private:
    PROPERTY()
    ResourceHandle<Skeleton> m_Skeleton;

    // 연산 캐싱용 변수
    vector<mat4> m_FinalBoneMatrices;
    vector<mat4> m_GlobalTransforms;

    // State 관리 관련
    unordered_map<wstring, tagAnimationState> m_States;
    tagAnimationState* m_CurrentState = nullptr;

    f32 m_CurrentTime = 0.0f;
};
END
