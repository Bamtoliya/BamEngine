#pragma once

#include "Engine_Includes.h"
#include "System.h"
#include <functional>
#include <vector>

namespace Engine
{
    enum class EUITweenEase : uint8
    {
        Linear = 0,
        EaseInQuad,
        EaseOutQuad,
        EaseInOutQuad,
        EaseInCubic,
        EaseOutCubic,
        EaseInOutCubic,
        EaseInBack,
        EaseOutBack,
        EaseInOutBack,
        EaseOutBounce
    };

    enum class EUITweenProperty : uint8
    {
        Custom = 0,
        Scale,
        AnchoredPosition,
        Alpha,
        Color,
        Rotation
    };

    enum class EUITweenLoopMode : uint8
    {
        Once = 0,
        Loop,
        PingPong
    };

    namespace UITweenMath
    {
        ENGINE_API f32 EvaluateEase(EUITweenEase ease, f32 t) noexcept;
        ENGINE_API f32 EvaluatePunch(f32 t) noexcept;
        ENGINE_API f32 LerpScalar(f32 from, f32 to, f32 easedT) noexcept;
        ENGINE_API glm::vec2 LerpVec2(const glm::vec2& from, const glm::vec2& to, f32 easedT) noexcept;
        ENGINE_API glm::vec4 LerpVec4(const glm::vec4& from, const glm::vec4& to, f32 easedT) noexcept;
    }

    struct UITweenClip
    {
        uint32 id = 0;
        entt::entity targetEntity = entt::null;
        EUITweenProperty property = EUITweenProperty::Custom;
        EUITweenEase ease = EUITweenEase::EaseOutQuad;
        EUITweenLoopMode loopMode = EUITweenLoopMode::Once;

        glm::vec4 from{ 0.0f };
        glm::vec4 to{ 1.0f };
        f32 duration = 0.25f;
        f32 delay = 0.0f;
        f32 elapsed = 0.0f;

        // loopCount == 0은 무한 반복(Loop/PingPong 전용). Once는 항상 1회 재생.
        uint32 loopCount = 1;
        uint32 completedLoops = 0;

        bool playing = true;
        bool completed = false;
        bool autoRemove = true;

        std::function<void(f32 easedT, const glm::vec4& currentValue)> onUpdate;
        std::function<void()> onComplete;
    };

    struct UITweenComponent
    {
        std::vector<UITweenClip> clips;
        uint32 nextClipId = 1;
    };

    class ENGINE_API UITweenPlayer
    {
    public:
        uint32 Play(UITweenClip clip);

        uint32 PlayScale(entt::entity entity, const glm::vec2& from, const glm::vec2& to,
            f32 duration, EUITweenEase ease = EUITweenEase::EaseOutQuad, f32 delay = 0.0f);

        uint32 PunchScale(entt::entity entity, const glm::vec2& baseScale, const glm::vec2& peakScale,
            f32 duration, EUITweenEase ease = EUITweenEase::EaseOutQuad, f32 delay = 0.0f);

        uint32 PlayOffset(entt::entity entity, const glm::vec2& from, const glm::vec2& to,
            f32 duration, EUITweenEase ease = EUITweenEase::EaseOutCubic, f32 delay = 0.0f);

        uint32 PlayAlpha(entt::entity entity, f32 fromAlpha, f32 toAlpha,
            f32 duration, EUITweenEase ease = EUITweenEase::Linear, f32 delay = 0.0f);

        uint32 PlayColor(entt::entity entity, const glm::vec4& fromColor, const glm::vec4& toColor,
            f32 duration, EUITweenEase ease = EUITweenEase::Linear, f32 delay = 0.0f);

        uint32 PlayRotation(entt::entity entity, f32 fromDegrees, f32 toDegrees,
            f32 duration, EUITweenEase ease = EUITweenEase::EaseOutQuad, f32 delay = 0.0f);

        void Update(f32 deltaTime);
        void Update(entt::registry& registry, f32 deltaTime);

        bool Stop(uint32 id);
        void StopByEntity(entt::entity entity);
        void Clear();

        [[nodiscard]] bool IsPlaying(uint32 id) const noexcept;
        [[nodiscard]] std::size_t GetActiveCount() const noexcept { return m_Clips.size(); }

    private:
        std::vector<UITweenClip> m_Clips;
        uint32 m_NextId = 1;
    };

    class ENGINE_API UITweenSystem final : public ISystem
    {
    public:
        static UITweenSystem* Create(void* arg = nullptr);

        void OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt) override;
        uint8 GetExecutionPhases() const override { return static_cast<uint8>(ESystemPhase::Presentation); }

        static void UpdateRegistry(entt::registry& registry, f32 dt);

        static uint32 PlayScale(entt::registry& registry, entt::entity entity,
            const glm::vec2& from, const glm::vec2& to, f32 duration,
            EUITweenEase ease = EUITweenEase::EaseOutQuad, f32 delay = 0.0f);

        static uint32 PunchScale(entt::registry& registry, entt::entity entity,
            const glm::vec2& baseScale, const glm::vec2& peakScale, f32 duration,
            EUITweenEase ease = EUITweenEase::EaseOutQuad, f32 delay = 0.0f);

        static uint32 PlayOffset(entt::registry& registry, entt::entity entity,
            const glm::vec2& from, const glm::vec2& to, f32 duration,
            EUITweenEase ease = EUITweenEase::EaseOutCubic, f32 delay = 0.0f);

        static uint32 PlayAlpha(entt::registry& registry, entt::entity entity,
            f32 fromAlpha, f32 toAlpha, f32 duration,
            EUITweenEase ease = EUITweenEase::Linear, f32 delay = 0.0f);

        static uint32 PlayColor(entt::registry& registry, entt::entity entity,
            const glm::vec4& fromColor, const glm::vec4& toColor, f32 duration,
            EUITweenEase ease = EUITweenEase::Linear, f32 delay = 0.0f);

        static uint32 PlayRotation(entt::registry& registry, entt::entity entity,
            f32 fromDegrees, f32 toDegrees, f32 duration,
            EUITweenEase ease = EUITweenEase::EaseOutQuad, f32 delay = 0.0f);

        static void StopAll(entt::registry& registry, entt::entity entity);
    };
}
