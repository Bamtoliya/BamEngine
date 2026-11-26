#include "Engine_Includes.h"
#include "UITween.h"
#include "RectTransformComponent.h"
#include "UIImageComponent.h"
#include "UITextComponent.h"

#include <algorithm>
#include <cmath>

namespace Engine
{
    namespace UITweenDetail
    {
        f32 Clamp01(f32 t) noexcept
        {
            if (!std::isfinite(t))
                return 0.0f;
            return std::clamp(t, 0.0f, 1.0f);
        }

        glm::vec4 ClampColor01(const glm::vec4& c) noexcept
        {
            return glm::vec4(
                std::clamp(c.r, 0.0f, 1.0f),
                std::clamp(c.g, 0.0f, 1.0f),
                std::clamp(c.b, 0.0f, 1.0f),
                std::clamp(c.a, 0.0f, 1.0f));
        }

        void ApplyClipValue(entt::registry* registry, entt::entity target, EUITweenProperty prop, const glm::vec4& value)
        {
            if (!registry || target == entt::null || !registry->valid(target))
                return;

            switch (prop)
            {
            case EUITweenProperty::Scale:
                if (auto* rect = registry->try_get<RectTransformComponent>(target))
                    rect->scale = glm::vec2(value.x, value.y);
                break;

            case EUITweenProperty::AnchoredPosition:
                if (auto* rect = registry->try_get<RectTransformComponent>(target))
                    rect->anchoredPosition = glm::vec2(value.x, value.y);
                break;

            case EUITweenProperty::Rotation:
                if (auto* rect = registry->try_get<RectTransformComponent>(target))
                    rect->rotation = value.x;
                break;

            case EUITweenProperty::Alpha:
            {
                const f32 alpha = std::clamp(value.x, 0.0f, 1.0f);
                if (auto* image = registry->try_get<UIImageComponent>(target))
                    image->color.a = alpha;
                if (auto* text = registry->try_get<UITextComponent>(target))
                    text->color.a = alpha;
                break;
            }

            case EUITweenProperty::Color:
            {
                const glm::vec4 clamped = ClampColor01(value);
                if (auto* image = registry->try_get<UIImageComponent>(target))
                    image->color = clamped;
                if (auto* text = registry->try_get<UITextComponent>(target))
                    text->color = clamped;
                break;
            }

            case EUITweenProperty::Custom:
            default:
                break;
            }
        }

        UITweenClip MakeScaleClip(entt::entity entity, const glm::vec2& from, const glm::vec2& to,
            f32 duration, EUITweenEase ease, f32 delay)
        {
            UITweenClip clip;
            clip.targetEntity = entity;
            clip.property = EUITweenProperty::Scale;
            clip.ease = ease;
            clip.from = glm::vec4(from, 0.0f, 0.0f);
            clip.to = glm::vec4(to, 0.0f, 0.0f);
            clip.duration = std::max(0.0f, duration);
            clip.delay = std::max(0.0f, delay);
            return clip;
        }

        UITweenClip MakePunchScaleClip(entt::entity entity, const glm::vec2& baseScale, const glm::vec2& peakScale,
            f32 duration, EUITweenEase ease, f32 delay)
        {
            UITweenClip clip = MakeScaleClip(entity, baseScale, peakScale, std::max(0.0f, duration) * 0.5f, ease, delay);
            clip.loopMode = EUITweenLoopMode::PingPong;
            clip.loopCount = 2; // 0->1 상승 후 1->0 복귀
            return clip;
        }

        UITweenClip MakeOffsetClip(entt::entity entity, const glm::vec2& from, const glm::vec2& to,
            f32 duration, EUITweenEase ease, f32 delay)
        {
            UITweenClip clip;
            clip.targetEntity = entity;
            clip.property = EUITweenProperty::AnchoredPosition;
            clip.ease = ease;
            clip.from = glm::vec4(from, 0.0f, 0.0f);
            clip.to = glm::vec4(to, 0.0f, 0.0f);
            clip.duration = std::max(0.0f, duration);
            clip.delay = std::max(0.0f, delay);
            return clip;
        }

        UITweenClip MakeAlphaClip(entt::entity entity, f32 fromAlpha, f32 toAlpha,
            f32 duration, EUITweenEase ease, f32 delay)
        {
            UITweenClip clip;
            clip.targetEntity = entity;
            clip.property = EUITweenProperty::Alpha;
            clip.ease = ease;
            clip.from = glm::vec4(fromAlpha, 0.0f, 0.0f, 0.0f);
            clip.to = glm::vec4(toAlpha, 0.0f, 0.0f, 0.0f);
            clip.duration = std::max(0.0f, duration);
            clip.delay = std::max(0.0f, delay);
            return clip;
        }

        UITweenClip MakeColorClip(entt::entity entity, const glm::vec4& fromColor, const glm::vec4& toColor,
            f32 duration, EUITweenEase ease, f32 delay)
        {
            UITweenClip clip;
            clip.targetEntity = entity;
            clip.property = EUITweenProperty::Color;
            clip.ease = ease;
            clip.from = fromColor;
            clip.to = toColor;
            clip.duration = std::max(0.0f, duration);
            clip.delay = std::max(0.0f, delay);
            return clip;
        }

        UITweenClip MakeRotationClip(entt::entity entity, f32 fromDegrees, f32 toDegrees,
            f32 duration, EUITweenEase ease, f32 delay)
        {
            UITweenClip clip;
            clip.targetEntity = entity;
            clip.property = EUITweenProperty::Rotation;
            clip.ease = ease;
            clip.from = glm::vec4(fromDegrees, 0.0f, 0.0f, 0.0f);
            clip.to = glm::vec4(toDegrees, 0.0f, 0.0f, 0.0f);
            clip.duration = std::max(0.0f, duration);
            clip.delay = std::max(0.0f, delay);
            return clip;
        }

        struct PendingCallback
        {
            std::function<void(f32, const glm::vec4&)> onUpdate;
            std::function<void()> onComplete;
            f32 easedT = 0.0f;
            glm::vec4 value{ 0.0f };
        };

        void AdvanceClips(std::vector<UITweenClip>& clips, entt::registry* registry, f32 dt)
        {
            if (!std::isfinite(dt) || dt <= 0.0f || clips.empty())
                return;

            std::vector<PendingCallback> callbacks;
            callbacks.reserve(clips.size());

            for (UITweenClip& clip : clips)
            {
                if (!clip.playing || clip.completed)
                    continue;

                if (clip.delay > 0.0f)
                {
                    clip.delay -= dt;
                    if (clip.delay > 0.0f)
                        continue;

                    // 지연 시간이 이번 프레임에 끝났다면 남은 시간만 진행에 반영
                    clip.elapsed += -clip.delay;
                    clip.delay = 0.0f;
                }
                else
                {
                    clip.elapsed += dt;
                }

                f32 normalizedT = 1.0f;
                bool finishedNow = false;

                if (clip.duration <= 0.0f)
                {
                    normalizedT = (clip.loopMode == EUITweenLoopMode::PingPong && (clip.completedLoops % 2) == 1)
                        ? 0.0f
                        : 1.0f;
                    finishedNow = true;
                }
                else
                {
                    while (clip.elapsed >= clip.duration)
                    {
                        ++clip.completedLoops;

                        if (clip.loopMode == EUITweenLoopMode::Once ||
                            (clip.loopCount > 0 && clip.completedLoops >= clip.loopCount))
                        {
                            clip.elapsed = clip.duration;
                            finishedNow = true;
                            break;
                        }

                        clip.elapsed -= clip.duration;
                    }

                    if (finishedNow)
                    {
                        const bool endedOnReverse = (clip.loopMode == EUITweenLoopMode::PingPong) &&
                            ((clip.completedLoops % 2) == 0);
                        normalizedT = endedOnReverse ? 0.0f : 1.0f;
                    }
                    else
                    {
                        const f32 rawT = Clamp01(clip.elapsed / clip.duration);
                        const bool isReverse = (clip.loopMode == EUITweenLoopMode::PingPong) &&
                            ((clip.completedLoops % 2) == 1);
                        normalizedT = isReverse ? (1.0f - rawT) : rawT;
                    }
                }

                const f32 easedT = UITweenMath::EvaluateEase(clip.ease, normalizedT);
                const glm::vec4 currentValue = UITweenMath::LerpVec4(clip.from, clip.to, easedT);

                ApplyClipValue(registry, clip.targetEntity, clip.property, currentValue);

                if (clip.onUpdate || (finishedNow && clip.onComplete))
                {
                    PendingCallback cb;
                    cb.onUpdate = clip.onUpdate;
                    cb.easedT = easedT;
                    cb.value = currentValue;
                    if (finishedNow)
                        cb.onComplete = clip.onComplete;
                    callbacks.push_back(std::move(cb));
                }

                if (finishedNow)
                {
                    clip.playing = false;
                    clip.completed = true;
                }
            }

            std::erase_if(clips, [](const UITweenClip& clip) {
                return clip.completed && clip.autoRemove;
            });

            // 콜백 내부에서 새 트윈을 추가하거나 중지해도 안전하도록 목록 정리 후 호출
            for (const PendingCallback& cb : callbacks)
            {
                if (cb.onUpdate)
                    cb.onUpdate(cb.easedT, cb.value);
                if (cb.onComplete)
                    cb.onComplete();
            }
        }
    }

    namespace UITweenMath
    {
        f32 EvaluateEase(EUITweenEase ease, f32 t) noexcept
        {
            const f32 x = UITweenDetail::Clamp01(t);

            switch (ease)
            {
            case EUITweenEase::Linear:
                return x;

            case EUITweenEase::EaseInQuad:
                return x * x;

            case EUITweenEase::EaseOutQuad:
            {
                const f32 inv = 1.0f - x;
                return 1.0f - inv * inv;
            }

            case EUITweenEase::EaseInOutQuad:
                return x < 0.5f
                    ? 2.0f * x * x
                    : 1.0f - std::pow(-2.0f * x + 2.0f, 2.0f) * 0.5f;

            case EUITweenEase::EaseInCubic:
                return x * x * x;

            case EUITweenEase::EaseOutCubic:
            {
                const f32 inv = 1.0f - x;
                return 1.0f - inv * inv * inv;
            }

            case EUITweenEase::EaseInOutCubic:
                return x < 0.5f
                    ? 4.0f * x * x * x
                    : 1.0f - std::pow(-2.0f * x + 2.0f, 3.0f) * 0.5f;

            case EUITweenEase::EaseInBack:
            {
                constexpr f32 c1 = 1.70158f;
                constexpr f32 c3 = c1 + 1.0f;
                return c3 * x * x * x - c1 * x * x;
            }

            case EUITweenEase::EaseOutBack:
            {
                constexpr f32 c1 = 1.70158f;
                constexpr f32 c3 = c1 + 1.0f;
                const f32 shifted = x - 1.0f;
                return 1.0f + c3 * shifted * shifted * shifted + c1 * shifted * shifted;
            }

            case EUITweenEase::EaseInOutBack:
            {
                constexpr f32 c1 = 1.70158f;
                constexpr f32 c2 = c1 * 1.525f;
                return x < 0.5f
                    ? (std::pow(2.0f * x, 2.0f) * ((c2 + 1.0f) * 2.0f * x - c2)) * 0.5f
                    : (std::pow(2.0f * x - 2.0f, 2.0f) * ((c2 + 1.0f) * (x * 2.0f - 2.0f) + c2) + 2.0f) * 0.5f;
            }

            case EUITweenEase::EaseOutBounce:
            {
                constexpr f32 n1 = 7.5625f;
                constexpr f32 d1 = 2.75f;
                if (x < 1.0f / d1)
                    return n1 * x * x;
                if (x < 2.0f / d1)
                {
                    const f32 v = x - 1.5f / d1;
                    return n1 * v * v + 0.75f;
                }
                if (x < 2.5f / d1)
                {
                    const f32 v = x - 2.25f / d1;
                    return n1 * v * v + 0.9375f;
                }
                const f32 v = x - 2.625f / d1;
                return n1 * v * v + 0.984375f;
            }

            default:
                return x;
            }
        }

        f32 EvaluatePunch(f32 t) noexcept
        {
            const f32 x = UITweenDetail::Clamp01(t);
            return std::sin(x * 3.14159265f);
        }

        f32 LerpScalar(f32 from, f32 to, f32 easedT) noexcept
        {
            return from + (to - from) * easedT;
        }

        glm::vec2 LerpVec2(const glm::vec2& from, const glm::vec2& to, f32 easedT) noexcept
        {
            return from + (to - from) * easedT;
        }

        glm::vec4 LerpVec4(const glm::vec4& from, const glm::vec4& to, f32 easedT) noexcept
        {
            return from + (to - from) * easedT;
        }
    }

    uint32 UITweenPlayer::Play(UITweenClip clip)
    {
        clip.id = m_NextId++;
        if (m_NextId == 0)
            m_NextId = 1;
        clip.playing = true;
        clip.completed = false;
        clip.elapsed = 0.0f;
        clip.completedLoops = 0;
        const uint32 assignedId = clip.id;
        m_Clips.push_back(std::move(clip));
        return assignedId;
    }

    uint32 UITweenPlayer::PlayScale(entt::entity entity, const glm::vec2& from, const glm::vec2& to,
        f32 duration, EUITweenEase ease, f32 delay)
    {
        return Play(UITweenDetail::MakeScaleClip(entity, from, to, duration, ease, delay));
    }

    uint32 UITweenPlayer::PunchScale(entt::entity entity, const glm::vec2& baseScale, const glm::vec2& peakScale,
        f32 duration, EUITweenEase ease, f32 delay)
    {
        return Play(UITweenDetail::MakePunchScaleClip(entity, baseScale, peakScale, duration, ease, delay));
    }

    uint32 UITweenPlayer::PlayOffset(entt::entity entity, const glm::vec2& from, const glm::vec2& to,
        f32 duration, EUITweenEase ease, f32 delay)
    {
        return Play(UITweenDetail::MakeOffsetClip(entity, from, to, duration, ease, delay));
    }

    uint32 UITweenPlayer::PlayAlpha(entt::entity entity, f32 fromAlpha, f32 toAlpha,
        f32 duration, EUITweenEase ease, f32 delay)
    {
        return Play(UITweenDetail::MakeAlphaClip(entity, fromAlpha, toAlpha, duration, ease, delay));
    }

    uint32 UITweenPlayer::PlayColor(entt::entity entity, const glm::vec4& fromColor, const glm::vec4& toColor,
        f32 duration, EUITweenEase ease, f32 delay)
    {
        return Play(UITweenDetail::MakeColorClip(entity, fromColor, toColor, duration, ease, delay));
    }

    uint32 UITweenPlayer::PlayRotation(entt::entity entity, f32 fromDegrees, f32 toDegrees,
        f32 duration, EUITweenEase ease, f32 delay)
    {
        return Play(UITweenDetail::MakeRotationClip(entity, fromDegrees, toDegrees, duration, ease, delay));
    }

    void UITweenPlayer::Update(f32 deltaTime)
    {
        UITweenDetail::AdvanceClips(m_Clips, nullptr, deltaTime);
    }

    void UITweenPlayer::Update(entt::registry& registry, f32 deltaTime)
    {
        UITweenDetail::AdvanceClips(m_Clips, &registry, deltaTime);
    }

    bool UITweenPlayer::Stop(uint32 id)
    {
        const auto erased = std::erase_if(m_Clips, [id](const UITweenClip& clip) {
            return clip.id == id;
        });
        return erased > 0;
    }

    void UITweenPlayer::StopByEntity(entt::entity entity)
    {
        std::erase_if(m_Clips, [entity](const UITweenClip& clip) {
            return clip.targetEntity == entity;
        });
    }

    void UITweenPlayer::Clear()
    {
        m_Clips.clear();
    }

    bool UITweenPlayer::IsPlaying(uint32 id) const noexcept
    {
        for (const UITweenClip& clip : m_Clips)
        {
            if (clip.id == id && clip.playing && !clip.completed)
                return true;
        }
        return false;
    }

    UITweenSystem* UITweenSystem::Create(void* arg)
    {
        (void)arg;
        return new UITweenSystem();
    }

    void UITweenSystem::OnUpdate(entt::registry& registry, const vector<Scene*>& activeScenes, f32 dt)
    {
        UpdateRegistry(registry, dt);
        for (Scene* scene : activeScenes)
        {
            if (scene)
                UpdateRegistry(scene->GetRegistry(), dt);
        }
    }

    void UITweenSystem::UpdateRegistry(entt::registry& registry, f32 dt)
    {
        if (!std::isfinite(dt) || dt <= 0.0f)
            return;

        auto view = registry.view<UITweenComponent>();
        std::vector<entt::entity> entities;
        entities.reserve(view.size());
        for (const entt::entity entity : view)
            entities.push_back(entity);

        for (const entt::entity entity : entities)
        {
            if (!registry.valid(entity))
                continue;

            auto* tweenComp = registry.try_get<UITweenComponent>(entity);
            if (!tweenComp || tweenComp->clips.empty())
                continue;

            UITweenDetail::AdvanceClips(tweenComp->clips, &registry, dt);
        }
    }

    namespace UITweenDetail
    {
        uint32 EmplaceClip(entt::registry& registry, entt::entity entity, UITweenClip clip)
        {
            if (entity == entt::null || !registry.valid(entity))
                return 0;

            auto& comp = registry.get_or_emplace<UITweenComponent>(entity);
            clip.id = comp.nextClipId++;
            if (comp.nextClipId == 0)
                comp.nextClipId = 1;
            clip.targetEntity = entity;
            clip.playing = true;
            clip.completed = false;
            clip.elapsed = 0.0f;
            clip.completedLoops = 0;

            const uint32 assignedId = clip.id;
            comp.clips.push_back(std::move(clip));
            return assignedId;
        }
    }

    uint32 UITweenSystem::PlayScale(entt::registry& registry, entt::entity entity,
        const glm::vec2& from, const glm::vec2& to, f32 duration, EUITweenEase ease, f32 delay)
    {
        return UITweenDetail::EmplaceClip(registry, entity,
            UITweenDetail::MakeScaleClip(entity, from, to, duration, ease, delay));
    }

    uint32 UITweenSystem::PunchScale(entt::registry& registry, entt::entity entity,
        const glm::vec2& baseScale, const glm::vec2& peakScale, f32 duration, EUITweenEase ease, f32 delay)
    {
        return UITweenDetail::EmplaceClip(registry, entity,
            UITweenDetail::MakePunchScaleClip(entity, baseScale, peakScale, duration, ease, delay));
    }

    uint32 UITweenSystem::PlayOffset(entt::registry& registry, entt::entity entity,
        const glm::vec2& from, const glm::vec2& to, f32 duration, EUITweenEase ease, f32 delay)
    {
        return UITweenDetail::EmplaceClip(registry, entity,
            UITweenDetail::MakeOffsetClip(entity, from, to, duration, ease, delay));
    }

    uint32 UITweenSystem::PlayAlpha(entt::registry& registry, entt::entity entity,
        f32 fromAlpha, f32 toAlpha, f32 duration, EUITweenEase ease, f32 delay)
    {
        return UITweenDetail::EmplaceClip(registry, entity,
            UITweenDetail::MakeAlphaClip(entity, fromAlpha, toAlpha, duration, ease, delay));
    }

    uint32 UITweenSystem::PlayColor(entt::registry& registry, entt::entity entity,
        const glm::vec4& fromColor, const glm::vec4& toColor, f32 duration, EUITweenEase ease, f32 delay)
    {
        return UITweenDetail::EmplaceClip(registry, entity,
            UITweenDetail::MakeColorClip(entity, fromColor, toColor, duration, ease, delay));
    }

    uint32 UITweenSystem::PlayRotation(entt::registry& registry, entt::entity entity,
        f32 fromDegrees, f32 toDegrees, f32 duration, EUITweenEase ease, f32 delay)
    {
        return UITweenDetail::EmplaceClip(registry, entity,
            UITweenDetail::MakeRotationClip(entity, fromDegrees, toDegrees, duration, ease, delay));
    }

    void UITweenSystem::StopAll(entt::registry& registry, entt::entity entity)
    {
        if (entity == entt::null || !registry.valid(entity))
            return;

        if (auto* comp = registry.try_get<UITweenComponent>(entity))
            comp->clips.clear();
    }
}
