#include "GpuHeapPageAllocator.h"

#include <cstdio>
#include <random>
#include <vector>

using namespace Engine;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "GpuHeapPageAllocatorCheck failed: %s\n", message);
        ++g_Failures;
    }

    bool Overlaps(const GpuHeapAllocation& a, const GpuHeapAllocation& b)
    {
        return a.Heap == b.Heap && a.FirstPage < b.FirstPage + b.PageCount && b.FirstPage < a.FirstPage + a.PageCount;
    }
}

int main()
{
    GpuHeapPageAllocator pages(16);
    Expect(!pages.Allocate(1), "no heap must mean no allocation");
    Expect(pages.AddHeap() == 0, "the first heap must use the first slot");
    Expect(!pages.Allocate(0) && !pages.Allocate(17), "empty and oversized requests must be rejected");

    const auto a = pages.Allocate(4);
    const auto b = pages.Allocate(8);
    const auto c = pages.Allocate(4);
    Expect(a && b && c && !Overlaps(*a, *b) && !Overlaps(*b, *c) && !Overlaps(*a, *c), "allocations must not overlap");
    Expect(!pages.Allocate(1) && pages.GetUsedPages(0) == 16, "a full heap must reject requests");

    // 떨어진 빈 구간은 합쳐지지 않고, 이어진 구간은 하나로 합친다.
    Expect(pages.Free(*a) && pages.Free(*c) && !pages.Allocate(5), "separate gaps must not be joined");
    Expect(pages.Free(*b) && pages.IsHeapEmpty(0), "freeing everything must empty the heap");
    const auto whole = pages.Allocate(16);
    Expect(whole && whole->FirstPage == 0, "neighbouring pages must be joined");

    Expect(pages.Free(*whole) && !pages.Free(*whole), "pages must not be released twice");
    Expect(!pages.Free({}) && !pages.Free({ 5, 0, 1 }) && !pages.Free({ 0, 15, 2 }), "invalid ranges must be rejected");
    const auto half = pages.Allocate(8);
    Expect(half && !pages.Free({ 0, 4, 8 }) && pages.GetUsedPages(0) == 8, "a partly free range must be rejected without changes");

    Expect(!pages.RemoveHeap(0), "a used heap must not be removed");
    Expect(pages.AddHeap() == 1 && pages.GetEmptyHeapCount() == 1, "a second heap must use a new slot");
    Expect(pages.RemoveHeap(1) && !pages.RemoveHeap(1) && !pages.IsHeapEmpty(1), "an empty heap must be removed once");
    const auto beyond = pages.Allocate(16);
    Expect(!beyond, "removed heaps must not be used");
    Expect(pages.AddHeap() == 1 && pages.Allocate(16)->Heap == 1, "a removed slot must be reused");

    // 무작위로 할당·해제해도 겹치지 않고 모두 돌려받는다.
    GpuHeapPageAllocator random(64);
    std::mt19937 engine(1234);
    std::vector<GpuHeapAllocation> live;
    bool valid = true;
    for (int step = 0; step < 4000 && valid; ++step)
    {
        if (live.empty() || engine() % 3 != 0)
        {
            const uint32 count = 1 + engine() % 20;
            auto allocation = random.Allocate(count);
            if (!allocation)
            {
                random.AddHeap();
                allocation = random.Allocate(count);
            }
            valid = allocation.has_value() && allocation->PageCount == count && allocation->FirstPage + count <= 64;
            for (const auto& other : live) valid = valid && !Overlaps(*allocation, other);
            if (valid) live.push_back(*allocation);
        }
        else
        {
            const size_t index = engine() % live.size();
            valid = random.Free(live[index]);
            live[index] = live.back();
            live.pop_back();
        }
    }
    Expect(valid, "random use must keep allocations separate");
    uint32 heapCount = 0;
    for (const auto& allocation : live)
    {
        heapCount = std::max(heapCount, allocation.Heap + 1);
        valid = random.Free(allocation) && valid;
    }
    for (uint32 heap = 0; heap < heapCount; ++heap) valid = valid && random.IsHeapEmpty(heap);
    Expect(valid, "every page must be returned");

    return g_Failures == 0 ? 0 : 1;
}
