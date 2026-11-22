#pragma once

#include "Types.h"

#include <algorithm>
#include <iterator>
#include <map>
#include <optional>
#include <vector>

namespace Engine
{
    // 큰 GPU 힙 안에서 차지한 연속 페이지
    struct GpuHeapAllocation
    {
        static constexpr uint32 InvalidHeap = 0xFFFFFFFFu;

        uint32 Heap = InvalidHeap;
        uint32 FirstPage = 0;
        uint32 PageCount = 0;

        [[nodiscard]] bool IsValid() const { return Heap != InvalidHeap && PageCount > 0; }
    };

    // 크기가 같은 힙 여러 개를 페이지 단위로 나눈다. GPU 객체는 모르며 힙 번호만 다룬다.
    // GPU가 사용을 마쳤는지는 호출자가 확인하고 Free한다.
    class GpuHeapPageAllocator
    {
    public:
        explicit GpuHeapPageAllocator(uint32 pagesPerHeap) : m_PagesPerHeap(pagesPerHeap) {}

        // 있는 힙에서 찾는다. 자리가 없으면 빈 값이므로 AddHeap 뒤에 다시 시도한다.
        [[nodiscard]] std::optional<GpuHeapAllocation> Allocate(uint32 pageCount)
        {
            if (pageCount == 0 || pageCount > m_PagesPerHeap) return std::nullopt;
            for (uint32 index = 0; index < m_Heaps.size(); ++index)
            {
                auto& heap = m_Heaps[index];
                if (!heap.Active) continue;
                for (auto range = heap.Free.begin(); range != heap.Free.end(); ++range)
                {
                    const auto [first, count] = *range;
                    if (count < pageCount) continue;

                    heap.Free.erase(range);
                    if (count > pageCount) heap.Free.emplace(first + pageCount, count - pageCount);
                    heap.UsedPages += pageCount;
                    return GpuHeapAllocation{ index, first, pageCount };
                }
            }
            return std::nullopt;
        }

        // 비운 번호가 있으면 다시 쓴다.
        uint32 AddHeap()
        {
            auto slot = std::ranges::find(m_Heaps, false, &Heap::Active);
            if (slot == m_Heaps.end()) slot = m_Heaps.insert(m_Heaps.end(), Heap{});
            slot->Active = true;
            slot->UsedPages = 0;
            slot->Free.clear();
            slot->Free.emplace(0u, m_PagesPerHeap);
            return static_cast<uint32>(slot - m_Heaps.begin());
        }

        // 사용 중인 페이지가 없는 힙만 지운다.
        bool RemoveHeap(uint32 index)
        {
            if (!IsHeapEmpty(index)) return false;
            m_Heaps[index] = {};
            return true;
        }

        // 잘못된 범위나 이미 돌려준 페이지면 아무것도 바꾸지 않고 false를 반환한다.
        bool Free(const GpuHeapAllocation& allocation)
        {
            if (!allocation.IsValid() || allocation.Heap >= m_Heaps.size()) return false;
            auto& heap = m_Heaps[allocation.Heap];
            if (!heap.Active || allocation.FirstPage >= m_PagesPerHeap ||
                allocation.PageCount > m_PagesPerHeap - allocation.FirstPage) return false;

            const uint32 end = allocation.FirstPage + allocation.PageCount;
            auto next = heap.Free.lower_bound(allocation.FirstPage);
            auto previous = next == heap.Free.begin() ? heap.Free.end() : std::prev(next);
            if (next != heap.Free.end() && next->first < end) return false;
            if (previous != heap.Free.end() && previous->first + previous->second > allocation.FirstPage) return false;

            uint32 first = allocation.FirstPage;
            uint32 count = allocation.PageCount;
            if (next != heap.Free.end() && next->first == end)
            {
                count += next->second;
                heap.Free.erase(next);
            }
            if (previous != heap.Free.end() && previous->first + previous->second == first)
            {
                first = previous->first;
                count += previous->second;
                heap.Free.erase(previous);
            }
            heap.Free.emplace(first, count);
            heap.UsedPages -= allocation.PageCount;
            return true;
        }

        [[nodiscard]] bool IsHeapEmpty(uint32 index) const
        {
            return index < m_Heaps.size() && m_Heaps[index].Active && m_Heaps[index].UsedPages == 0;
        }

        [[nodiscard]] uint32 GetEmptyHeapCount() const
        {
            return static_cast<uint32>(std::ranges::count_if(m_Heaps, [](const Heap& heap) { return heap.Active && heap.UsedPages == 0; }));
        }

        [[nodiscard]] uint32 GetUsedPages(uint32 index) const { return index < m_Heaps.size() ? m_Heaps[index].UsedPages : 0; }

    private:
        struct Heap
        {
            bool Active = false;
            uint32 UsedPages = 0;
            std::map<uint32, uint32> Free; // 시작 페이지 -> 페이지 수
        };

        uint32 m_PagesPerHeap = 0;
        std::vector<Heap> m_Heaps;
    };
}
