#pragma once

#include "Types.h"

#include <algorithm>
#include <limits>
#include <span>
#include <vector>

namespace Engine
{
    // 스트리밍 대상 하나의 상태. 단계 0이 가장 자세하고 마지막 단계가 항상 남는 최소 상태다.
    struct StreamingItem
    {
        uint32 Id = 0;
        std::span<const uint64> LevelBytes; // 단계별 GPU 바이트. 계획을 세우는 동안 유효해야 한다
        uint32 Level = 0;                // 지금 단계
        uint32 Wanted = 0;               // 화면에 필요한 단계
        f32 Priority = 0.0f;             // 클수록 중요하다
        bool Needed = false;             // 최근에 화면에서 요청했는지
        bool Busy = false;               // 바꾸는 중
        uint32 Target = 0;               // Busy일 때 바꾸려는 단계
    };

    struct StreamingAction
    {
        uint32 Id = 0;
        uint32 Level = 0;
    };

    struct StreamingPlan
    {
        std::vector<StreamingAction> Reductions; // 먼저 처리한다
        std::vector<StreamingAction> Loads;      // 중요한 순서
        uint64 UsedBytes = 0;                    // 계획을 모두 마친 뒤의 사용량
        uint32 UnmetCount = 0;                   // 자리가 없어 원하는 단계까지 올리지 못한 수
    };

    // 한도 안에서는 내리지 않는다. 자리가 필요하면 쓰지 않는 것, 필요보다 자세한 것, 훨씬 덜 중요한 것 순으로 내린다.
    [[nodiscard]] inline StreamingPlan PlanStreaming(const std::vector<StreamingItem>& items, uint64 budgetBytes, size_t maxLoads)
    {
        StreamingPlan plan;
        std::vector<bool> changed(items.size(), false);
        for (size_t i = 0; i < items.size(); ++i)
        {
            const auto& item = items[i];
            // 바꾸는 중이면 끝난 뒤의 크기로 센다. 내리는 중인 것을 지금 크기로 세면 필요 이상으로 더 내리게 된다.
            plan.UsedBytes += item.LevelBytes[item.Busy ? item.Target : item.Level];
        }

        enum class EReason : uint8 { Unneeded, Excess, LessImportant };
        struct Candidate
        {
            size_t Index = 0;
            EReason Reason = EReason::Unneeded;
            uint32 Level = 0;
        };
        std::vector<Candidate> candidates;
        for (size_t i = 0; i < items.size(); ++i)
        {
            const auto& item = items[i];
            const auto floor = static_cast<uint32>(item.LevelBytes.size() - 1);
            if (item.Busy || item.Level >= floor) continue;
            if (!item.Needed) candidates.push_back({ i, EReason::Unneeded, floor });
            else if (item.Wanted > item.Level) candidates.push_back({ i, EReason::Excess, (std::min)(item.Wanted, floor) });
            else candidates.push_back({ i, EReason::LessImportant, item.Level + 1 });
        }
        std::stable_sort(candidates.begin(), candidates.end(), [&](const Candidate& a, const Candidate& b)
            {
                if (a.Reason != b.Reason) return a.Reason < b.Reason;
                return items[a.Index].Priority < items[b.Index].Priority;
            });

        // important보다 두 배 이상 덜 중요한 것까지만 내려, 비슷한 것끼리 번갈아 내리고 올리지 않게 한다.
        const auto canReduce = [&](const Candidate& candidate, f32 important)
            {
                return !changed[candidate.Index] && (candidate.Reason != EReason::LessImportant || items[candidate.Index].Priority * 2.0f < important);
            };
        const auto getFreedBytes = [&](const Candidate& candidate)
            {
                const auto& item = items[candidate.Index];
                return item.LevelBytes[item.Level] - item.LevelBytes[candidate.Level];
            };
        // allowPartial이 아니면 자리를 다 만들 수 있을 때만 내린다. 내리기만 하고 읽지 못하면 다음에 다시 올리게 된다.
        const auto makeRoom = [&](uint64 bytes, f32 important, bool allowPartial)
            {
                uint64 available = 0;
                for (const auto& candidate : candidates)
                    if (canReduce(candidate, important)) available += getFreedBytes(candidate);
                if (!allowPartial && plan.UsedBytes + bytes > budgetBytes + available) return false;

                for (auto& candidate : candidates)
                {
                    if (plan.UsedBytes + bytes <= budgetBytes) return true;
                    if (!canReduce(candidate, important)) continue;
                    const auto& item = items[candidate.Index];
                    changed[candidate.Index] = true;
                    plan.UsedBytes -= getFreedBytes(candidate);
                    plan.Reductions.push_back({ item.Id, candidate.Level });
                }
                return plan.UsedBytes + bytes <= budgetBytes;
            };

        // 한도를 줄였으면 새로 읽기 전에 먼저 맞춘다.
        if (plan.UsedBytes > budgetBytes) static_cast<void>(makeRoom(0, std::numeric_limits<f32>::infinity(), true));

        std::vector<size_t> wanted;
        for (size_t i = 0; i < items.size(); ++i)
            if (items[i].Needed && !items[i].Busy && !changed[i] && items[i].Wanted < items[i].Level) wanted.push_back(i);
        std::stable_sort(wanted.begin(), wanted.end(), [&](size_t a, size_t b) { return items[a].Priority > items[b].Priority; });

        for (const size_t index : wanted)
        {
            if (plan.Loads.size() >= maxLoads) break;
            const auto& item = items[index];
            if (changed[index]) continue;

            const auto need = [&](uint32 level) { return item.LevelBytes[level] - item.LevelBytes[item.Level]; };
            // 전부 들어가지 않으면 들어가는 만큼만 올린다.
            uint32 level = item.Wanted;
            bool fits = plan.UsedBytes + need(level) <= budgetBytes || (need(level) <= budgetBytes && makeRoom(need(level), item.Priority, false));
            while (!fits && ++level < item.Level) fits = plan.UsedBytes + need(level) <= budgetBytes;
            if (level != item.Wanted) ++plan.UnmetCount;
            if (!fits) continue;

            changed[index] = true;
            plan.UsedBytes += need(level);
            plan.Loads.push_back({ item.Id, level });
        }
        return plan;
    }
}
