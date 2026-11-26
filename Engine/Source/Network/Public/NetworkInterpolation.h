#pragma once

#include "Engine_API.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <iterator>
#include <utility>

namespace Engine::Network
{
    // 원격 엔티티 트랜스폼 스냅샷 기본 데이터
    struct NetworkTransformSnapshot
    {
        glm::vec3 position{ 0.0f };
        glm::quat rotation = glm::identity<glm::quat>();
        glm::vec3 velocity{ 0.0f };
    };

    // 스냅샷 보간 샘플링 결과
    template <typename TState>
    struct InterpolationSample
    {
        TState state{};
        bool valid = false;
        bool extrapolated = false;
        double sampleTime = 0.0;
        float alpha = 0.0f;

        explicit operator bool() const noexcept { return valid; }
    };

    // 타임스탬프 기반 원격 엔티티 스냅샷 보간 버퍼
    template <typename TState>
    class NetworkInterpolationBuffer
    {
    public:
        struct Snapshot
        {
            double time = 0.0;
            TState state{};
        };

        explicit NetworkInterpolationBuffer(
            double interpolationDelay = 0.1,
            double maxExtrapolationTime = 0.25,
            std::size_t maxSnapshots = 64)
            : m_InterpolationDelay(std::max(0.0, interpolationDelay))
            , m_MaxExtrapolationTime(std::max(0.0, maxExtrapolationTime))
            , m_MaxSnapshots(std::max<std::size_t>(2, maxSnapshots))
        {
        }

        // 타임스탬프 순서를 유지하며 스냅샷을 버퍼에 추가 (비정상 시간값 무시, 동일 타임스탬프 덮어쓰기)
        bool PushSnapshot(double time, const TState& state)
        {
            if (!std::isfinite(time))
            {
                return false;
            }

            if (m_Snapshots.empty() || time > m_Snapshots.back().time)
            {
                m_Snapshots.push_back(Snapshot{ time, state });
            }
            else if (time == m_Snapshots.back().time)
            {
                m_Snapshots.back().state = state;
            }
            else
            {
                auto it = std::lower_bound(
                    m_Snapshots.begin(),
                    m_Snapshots.end(),
                    time,
                    [](const Snapshot& s, double t) { return s.time < t; });

                if (it != m_Snapshots.end() && it->time == time)
                {
                    it->state = state;
                }
                else
                {
                    m_Snapshots.insert(it, Snapshot{ time, state });
                }
            }

            while (m_Snapshots.size() > m_MaxSnapshots)
            {
                m_Snapshots.pop_front();
            }

            return true;
        }

        // renderTime - interpolationDelay 시점의 상태를 보간 샘플링 (maxExtrapolationTime 상한 방어)
        template <typename LerpFn>
        [[nodiscard]] InterpolationSample<TState> Sample(double renderTime, LerpFn&& lerpFn) const
        {
            return SampleImpl(
                renderTime,
                std::forward<LerpFn>(lerpFn),
                [&lerpFn](const TState& prev, const TState& curr, double span, double clampedExtrap) {
                    const float alpha = static_cast<float>(1.0 + (clampedExtrap / span));
                    return lerpFn(prev, curr, alpha);
                });
        }

        // 외삽 시 별도의 외삽 함수(예: 속도 기반 Dead Reckoning)를 사용하는 오버로드
        template <typename LerpFn, typename ExtrapFn>
        [[nodiscard]] InterpolationSample<TState> Sample(double renderTime, LerpFn&& lerpFn, ExtrapFn&& extrapFn) const
        {
            return SampleImpl(renderTime, std::forward<LerpFn>(lerpFn), std::forward<ExtrapFn>(extrapFn));
        }

        // minTime 이전의 불필요한 과거 스냅샷 정리 (최소 보간 쌍은 유지)
        void PruneBefore(double minTime)
        {
            while (m_Snapshots.size() > 2 && m_Snapshots[1].time <= minTime)
            {
                m_Snapshots.pop_front();
            }
        }

        void Clear() { m_Snapshots.clear(); }

        void SetInterpolationDelay(double delaySeconds) { m_InterpolationDelay = std::max(0.0, delaySeconds); }
        [[nodiscard]] double GetInterpolationDelay() const noexcept { return m_InterpolationDelay; }

        void SetMaxExtrapolationTime(double maxSeconds) { m_MaxExtrapolationTime = std::max(0.0, maxSeconds); }
        [[nodiscard]] double GetMaxExtrapolationTime() const noexcept { return m_MaxExtrapolationTime; }

        void SetMaxSnapshots(std::size_t maxSnapshots)
        {
            m_MaxSnapshots = std::max<std::size_t>(2, maxSnapshots);
            while (m_Snapshots.size() > m_MaxSnapshots)
            {
                m_Snapshots.pop_front();
            }
        }
        [[nodiscard]] std::size_t GetMaxSnapshots() const noexcept { return m_MaxSnapshots; }

        [[nodiscard]] std::size_t GetSnapshotCount() const noexcept { return m_Snapshots.size(); }
        [[nodiscard]] bool IsEmpty() const noexcept { return m_Snapshots.empty(); }
        [[nodiscard]] const std::deque<Snapshot>& GetSnapshots() const noexcept { return m_Snapshots; }

    private:
        template <typename LerpFn, typename ExtrapFn>
        [[nodiscard]] InterpolationSample<TState> SampleImpl(double renderTime, LerpFn&& lerpFn, ExtrapFn&& extrapFn) const
        {
            InterpolationSample<TState> result;
            if (!std::isfinite(renderTime) || m_Snapshots.empty())
            {
                return result;
            }

            const double targetTime = renderTime - m_InterpolationDelay;
            result.sampleTime = targetTime;
            result.valid = true;

            if (m_Snapshots.size() == 1)
            {
                result.state = m_Snapshots.front().state;
                result.extrapolated = (targetTime > m_Snapshots.front().time);
                result.alpha = 0.0f;
                return result;
            }

            if (targetTime <= m_Snapshots.front().time)
            {
                result.state = m_Snapshots.front().state;
                result.extrapolated = false;
                result.alpha = 0.0f;
                return result;
            }

            if (targetTime <= m_Snapshots.back().time)
            {
                auto upper = std::lower_bound(
                    m_Snapshots.begin(),
                    m_Snapshots.end(),
                    targetTime,
                    [](const Snapshot& s, double t) { return s.time < t; });

                if (upper == m_Snapshots.begin())
                {
                    result.state = m_Snapshots.front().state;
                    result.alpha = 0.0f;
                    return result;
                }

                const Snapshot& curr = *upper;
                const Snapshot& prev = *std::prev(upper);
                const double span = curr.time - prev.time;
                const float alpha = (span > 1e-9)
                    ? std::clamp(static_cast<float>((targetTime - prev.time) / span), 0.0f, 1.0f)
                    : 1.0f;

                result.state = lerpFn(prev.state, curr.state, alpha);
                result.extrapolated = false;
                result.alpha = alpha;
                return result;
            }

            // targetTime > m_Snapshots.back().time: 외삽 상한 방어
            const Snapshot& prev = m_Snapshots[m_Snapshots.size() - 2];
            const Snapshot& curr = m_Snapshots[m_Snapshots.size() - 1];
            const double extrapTime = targetTime - curr.time;
            const double span = curr.time - prev.time;

            if (m_MaxExtrapolationTime <= 0.0 || span <= 1e-9)
            {
                result.state = curr.state;
                result.extrapolated = false;
                result.sampleTime = curr.time;
                result.alpha = 1.0f;
                return result;
            }

            const double clampedExtrap = std::min(extrapTime, m_MaxExtrapolationTime);
            result.state = extrapFn(prev.state, curr.state, span, clampedExtrap);
            result.extrapolated = true;
            result.sampleTime = curr.time + clampedExtrap;
            result.alpha = static_cast<float>(1.0 + (clampedExtrap / span));
            return result;
        }

    private:
        std::deque<Snapshot> m_Snapshots;
        double m_InterpolationDelay = 0.1;
        double m_MaxExtrapolationTime = 0.25;
        std::size_t m_MaxSnapshots = 64;
    };

    // NetworkTransformSnapshot 표준 보간/외삽 헬퍼
    ENGINE_API NetworkTransformSnapshot LerpTransformSnapshot(
        const NetworkTransformSnapshot& a,
        const NetworkTransformSnapshot& b,
        float alpha);

    ENGINE_API NetworkTransformSnapshot ExtrapolateTransformSnapshot(
        const NetworkTransformSnapshot& prev,
        const NetworkTransformSnapshot& curr,
        double spanSeconds,
        double clampedExtrapSeconds);
}
