#pragma once

#include "Engine_API.h"
#include "NetworkInterpolation.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <type_traits>
#include <utility>

namespace Engine::Network
{
    // 부호 없는 시퀀스 번호 랩어라운드 안전 비교 (a > b)
    template <typename TSeq>
    constexpr bool IsSequenceGreater(TSeq a, TSeq b) noexcept
    {
        static_assert(std::is_unsigned_v<TSeq>, "Sequence type must be unsigned.");
        constexpr TSeq halfRange = (std::numeric_limits<TSeq>::max)() / 2 + 1;
        return ((a > b) && (a - b < halfRange)) || ((a < b) && (b - a > halfRange));
    }

    template <typename TSeq>
    constexpr bool IsSequenceGreaterOrEqual(TSeq a, TSeq b) noexcept
    {
        return a == b || IsSequenceGreater(a, b);
    }

    // 미확인 입력 히스토리 엔트리
    template <typename TInput, typename TState>
    struct PredictionEntry
    {
        std::uint32_t sequence = 0;
        TInput input{};
        TState predictedState{};
    };

    // 서버 보정(Reconciliation) 결과
    template <typename TState>
    struct ReconcileResult
    {
        TState state{};
        bool corrected = false;
        bool stale = false;
        float error = 0.0f;
        std::size_t replayedCount = 0;
    };

    // 시퀀스 번호 기반 클라이언트 예측 및 서버 보정 버퍼
    template <typename TInput, typename TState>
    class NetworkPredictionBuffer
    {
    public:
        using Entry = PredictionEntry<TInput, TState>;
        using ErrorFn = std::function<float(const TState& predicted, const TState& authoritative)>;

        explicit NetworkPredictionBuffer(
            std::size_t maxHistorySize = 128,
            float errorThreshold = 0.0f,
            ErrorFn errorFn = nullptr)
            : m_MaxHistorySize(std::max<std::size_t>(1, maxHistorySize))
            , m_ErrorThreshold(std::max(0.0f, errorThreshold))
            , m_ErrorFn(std::move(errorFn))
        {
        }

        // 로컬 입력과 해당 입력으로 예측된 상태를 시퀀스 순으로 기록
        bool RecordInput(std::uint32_t seq, const TInput& input, const TState& predictedState)
        {
            if (m_HasAcknowledged && !IsSequenceGreater(seq, m_LastAcknowledgedSeq))
            {
                return false;
            }

            if (!m_Entries.empty())
            {
                if (seq == m_Entries.back().sequence)
                {
                    m_Entries.back().input = input;
                    m_Entries.back().predictedState = predictedState;
                    return true;
                }

                if (!IsSequenceGreater(seq, m_Entries.back().sequence))
                {
                    return false;
                }
            }

            if (m_Entries.size() >= m_MaxHistorySize)
            {
                m_Entries.pop_front();
            }

            m_Entries.push_back(Entry{ seq, input, predictedState });
            return true;
        }

        // 서버가 처리 완료한 시퀀스 이하의 확인된 입력을 히스토리에서 제거
        std::size_t Acknowledge(std::uint32_t lastProcessedSeq)
        {
            if (m_HasAcknowledged && !IsSequenceGreater(lastProcessedSeq, m_LastAcknowledgedSeq))
            {
                return 0;
            }

            m_LastAcknowledgedSeq = lastProcessedSeq;
            m_HasAcknowledged = true;

            std::size_t removed = 0;
            while (!m_Entries.empty() && IsSequenceGreaterOrEqual(lastProcessedSeq, m_Entries.front().sequence))
            {
                m_Entries.pop_front();
                ++removed;
            }

            return removed;
        }

        // 설정된 오차 함수 및 임계값을 사용하여 서버 상태와 대조 후 필요 시 미확인 입력 재시뮬레이션
        template <typename ReplayFn>
        ReconcileResult<TState> Reconcile(std::uint32_t serverSeq, const TState& serverState, ReplayFn&& replayFn)
        {
            if (m_ErrorFn)
            {
                return Reconcile(serverSeq, serverState, std::forward<ReplayFn>(replayFn), m_ErrorFn, m_ErrorThreshold);
            }

            return ReconcileAlways(serverSeq, serverState, std::forward<ReplayFn>(replayFn));
        }

        // 명시적 오차 측정 함수 및 임계값을 지정하여 서버 상태 보정 수행
        template <typename ReplayFn, typename CustomErrorFn>
        ReconcileResult<TState> Reconcile(
            std::uint32_t serverSeq,
            const TState& serverState,
            ReplayFn&& replayFn,
            CustomErrorFn&& errorFn,
            float errorThreshold)
        {
            ReconcileResult<TState> result;
            if (m_HasAcknowledged && !IsSequenceGreater(serverSeq, m_LastAcknowledgedSeq))
            {
                result.state = serverState;
                result.stale = true;
                return result;
            }

            const Entry* matchedEntry = FindEntry(serverSeq);
            if (matchedEntry != nullptr)
            {
                const float err = static_cast<float>(errorFn(matchedEntry->predictedState, serverState));
                result.error = err;

                if (err <= errorThreshold)
                {
                    // 예측 오차가 임계값 이하이면 재시뮬레이션 없이 확인된 입력만 정리
                    result.state = m_Entries.empty() ? matchedEntry->predictedState : m_Entries.back().predictedState;
                    Acknowledge(serverSeq);
                    result.corrected = false;
                    result.replayedCount = 0;
                    return result;
                }
            }

            // 오차 임계값 초과 또는 히스토리 미존재: 서버 상태로부터 미확인 입력 재시뮬레이션
            Acknowledge(serverSeq);
            TState state = serverState;
            for (Entry& entry : m_Entries)
            {
                ApplyReplay(replayFn, state, entry.input);
                entry.predictedState = state;
            }

            result.state = state;
            result.corrected = true;
            result.replayedCount = m_Entries.size();
            return result;
        }

        void Clear()
        {
            m_Entries.clear();
            m_LastAcknowledgedSeq = 0;
            m_HasAcknowledged = false;
        }

        void SetMaxHistorySize(std::size_t maxHistorySize)
        {
            m_MaxHistorySize = std::max<std::size_t>(1, maxHistorySize);
            while (m_Entries.size() > m_MaxHistorySize)
            {
                m_Entries.pop_front();
            }
        }
        [[nodiscard]] std::size_t GetMaxHistorySize() const noexcept { return m_MaxHistorySize; }

        void SetErrorThreshold(float threshold) noexcept { m_ErrorThreshold = std::max(0.0f, threshold); }
        [[nodiscard]] float GetErrorThreshold() const noexcept { return m_ErrorThreshold; }

        void SetErrorFunction(ErrorFn errorFn) { m_ErrorFn = std::move(errorFn); }

        [[nodiscard]] std::size_t GetPendingCount() const noexcept { return m_Entries.size(); }
        [[nodiscard]] bool IsEmpty() const noexcept { return m_Entries.empty(); }
        [[nodiscard]] bool HasAcknowledged() const noexcept { return m_HasAcknowledged; }
        [[nodiscard]] std::uint32_t GetLastAcknowledgedSequence() const noexcept { return m_LastAcknowledgedSeq; }
        [[nodiscard]] const std::deque<Entry>& GetEntries() const noexcept { return m_Entries; }

    private:
        [[nodiscard]] const Entry* FindEntry(std::uint32_t seq) const noexcept
        {
            for (const Entry& entry : m_Entries)
            {
                if (entry.sequence == seq)
                {
                    return &entry;
                }
            }
            return nullptr;
        }

        template <typename ReplayFn>
        static void ApplyReplay(ReplayFn& replayFn, TState& state, const TInput& input)
        {
            using Ret = std::invoke_result_t<ReplayFn&, TState&, const TInput&>;
            if constexpr (std::is_void_v<Ret>)
            {
                replayFn(state, input);
            }
            else
            {
                state = replayFn(state, input);
            }
        }

        template <typename ReplayFn>
        ReconcileResult<TState> ReconcileAlways(std::uint32_t serverSeq, const TState& serverState, ReplayFn&& replayFn)
        {
            ReconcileResult<TState> result;
            if (m_HasAcknowledged && !IsSequenceGreater(serverSeq, m_LastAcknowledgedSeq))
            {
                result.state = serverState;
                result.stale = true;
                return result;
            }

            Acknowledge(serverSeq);
            TState state = serverState;
            for (Entry& entry : m_Entries)
            {
                ApplyReplay(replayFn, state, entry.input);
                entry.predictedState = state;
            }

            result.state = state;
            result.corrected = true;
            result.replayedCount = m_Entries.size();
            return result;
        }

    private:
        std::deque<Entry> m_Entries;
        std::size_t m_MaxHistorySize = 128;
        float m_ErrorThreshold = 0.0f;
        ErrorFn m_ErrorFn;
        std::uint32_t m_LastAcknowledgedSeq = 0;
        bool m_HasAcknowledged = false;
    };

    // NetworkTransformSnapshot 오차 계산 헬퍼
    ENGINE_API float ComputeTransformPositionError(
        const NetworkTransformSnapshot& predicted,
        const NetworkTransformSnapshot& authoritative);

    ENGINE_API float ComputeTransformRotationErrorRadians(
        const NetworkTransformSnapshot& predicted,
        const NetworkTransformSnapshot& authoritative);
}
