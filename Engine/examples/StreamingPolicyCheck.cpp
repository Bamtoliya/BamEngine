#include "StreamingDemand.h"
#include "StreamingPolicy.h"

#include <array>
#include <cmath>
#include <cstdio>

using namespace Engine;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "StreamingPolicyCheck failed: %s\n", message);
        ++g_Failures;
    }

    // 단계 0: 100, 1: 25, 2: 5, 3(최소): 1
    constexpr std::array<uint64, 4> TextureBytes{ 100, 25, 5, 1 };
    constexpr std::array<uint64, 2> MeshBytes{ 40, 0 };

    StreamingItem Item(uint32 id, std::span<const uint64> bytes, uint32 level, uint32 wanted, f32 priority, bool needed = true)
    {
        StreamingItem item;
        item.Id = id;
        item.LevelBytes = bytes;
        item.Level = level;
        item.Wanted = wanted;
        item.Priority = priority;
        item.Needed = needed;
        return item;
    }

    bool Has(const std::vector<StreamingAction>& actions, uint32 id, uint32 level)
    {
        for (const auto& action : actions)
            if (action.Id == id && action.Level == level) return true;
        return false;
    }
}

int main()
{
    {
        const auto plan = PlanStreaming({ Item(0, TextureBytes, 3, 0, 0.2f), Item(1, MeshBytes, 1, 0, 0.9f), Item(2, TextureBytes, 3, 1, 0.5f) }, 1000, 8);
        Expect(plan.Loads.size() == 3 && plan.Loads[0].Id == 1 && plan.Loads[1].Id == 2 && plan.Loads[2].Id == 0, "important resources must load first");
        Expect(Has(plan.Loads, 2, 1) && Has(plan.Loads, 0, 0) && plan.Reductions.empty() && plan.UsedBytes == 165, "only the requested detail must load");
        Expect(PlanStreaming({ Item(0, TextureBytes, 3, 0, 0.2f), Item(1, MeshBytes, 1, 0, 0.9f) }, 1000, 1).Loads.size() == 1, "the number of loads must be limited");
    }
    {
        // 한도 안에서는 쓰지 않거나 필요보다 자세해도 내리지 않는다.
        const auto plan = PlanStreaming({ Item(0, TextureBytes, 0, 3, 0.1f), Item(1, MeshBytes, 0, 0, 0.0f, false) }, 140, 8);
        Expect(plan.Reductions.empty() && plan.Loads.empty() && plan.UsedBytes == 140, "resources must stay within the budget");
    }
    {
        // 쓰지 않는 것, 필요보다 자세한 것 순으로 자리를 만든다.
        const std::vector<StreamingItem> items{ Item(0, TextureBytes, 0, 1, 0.1f), Item(1, MeshBytes, 0, 0, 0.8f, false), Item(2, MeshBytes, 1, 0, 0.5f) };
        auto plan = PlanStreaming(items, 140, 8);
        Expect(Has(plan.Reductions, 1, 1) && plan.Reductions.size() == 1 && Has(plan.Loads, 2, 0), "unused resources must be reduced first");
        plan = PlanStreaming(items, 70, 8);
        Expect(Has(plan.Reductions, 1, 1) && Has(plan.Reductions, 0, 1) && Has(plan.Loads, 2, 0) && plan.UsedBytes == 65, "excess detail must be reduced next");
    }
    {
        // 두 배 이상 중요할 때만 쓰고 있는 것을 줄여, 서로 번갈아 내리지 않는다.
        auto plan = PlanStreaming({ Item(0, MeshBytes, 0, 0, 0.3f), Item(1, MeshBytes, 1, 0, 0.5f) }, 40, 8);
        Expect(plan.Reductions.empty() && plan.Loads.empty() && plan.UnmetCount == 1, "similar resources must not replace each other");
        plan = PlanStreaming({ Item(0, MeshBytes, 0, 0, 0.2f), Item(1, MeshBytes, 1, 0, 0.5f) }, 40, 8);
        Expect(Has(plan.Reductions, 0, 1) && Has(plan.Loads, 1, 0), "a much more important resource must take the space");
    }
    {
        // 전부 들어가지 않으면 들어가는 단계까지만 올린다.
        const auto plan = PlanStreaming({ Item(0, TextureBytes, 3, 0, 1.0f) }, 30, 8);
        Expect(Has(plan.Loads, 0, 1) && plan.UnmetCount == 1, "a texture must load the largest level that fits");
        Expect(PlanStreaming({ Item(0, TextureBytes, 1, 0, 1.0f) }, 30, 8).Loads.empty(), "the same level must not be loaded again");
    }
    {
        // 한도를 줄이면 쓰고 있는 것도 덜 중요한 것부터 한 단계씩 줄인다.
        const auto plan = PlanStreaming({ Item(0, TextureBytes, 0, 0, 0.9f), Item(1, TextureBytes, 0, 0, 0.1f) }, 150, 8);
        Expect(Has(plan.Reductions, 1, 1) && plan.Reductions.size() == 1 && plan.Loads.empty() && plan.UsedBytes == 125, "a lower budget must reduce the least important resource");
        Expect(PlanStreaming({ Item(0, TextureBytes, 3, 3, 0.0f, false) }, 0, 8).Reductions.empty(), "the smallest level must always stay");
    }
    {
        // 바꾸는 중인 것은 건드리지 않고 끝난 뒤의 크기로 센다.
        auto busy = Item(0, TextureBytes, 3, 0, 1.0f);
        busy.Busy = true;
        busy.Target = 0;
        const auto plan = PlanStreaming({ busy, Item(1, MeshBytes, 1, 0, 0.5f) }, 130, 8);
        Expect(plan.UsedBytes == 100 && plan.Loads.empty() && plan.Reductions.empty(), "pending changes must reserve their space");

        // 내리는 중인 것은 이미 내린 것으로 세어 더 내리지 않는다.
        auto reducing = Item(0, TextureBytes, 0, 0, 0.1f);
        reducing.Busy = true;
        reducing.Target = 1;
        Expect(PlanStreaming({ reducing, Item(1, TextureBytes, 0, 0, 0.9f) }, 150, 8).Reductions.empty(), "pending reductions must not reduce more resources");
    }
    {
        // 계획을 반복해도 안정된다.
        std::vector<StreamingItem> items;
        for (uint32 i = 0; i < 12; ++i) items.push_back(Item(i, TextureBytes, 3, 0, 0.1f + 0.05f * static_cast<f32>(i)));
        size_t changes = 1;
        int steps = 0;
        for (; steps < 64 && changes > 0; ++steps)
        {
            const auto plan = PlanStreaming(items, 400, 4);
            changes = plan.Loads.size() + plan.Reductions.size();
            for (const auto* actions : { &plan.Reductions, &plan.Loads })
                for (const auto& action : *actions) items[action.Id].Level = action.Level;
            uint64 used = 0;
            for (const auto& item : items) used += item.LevelBytes[item.Level];
            Expect(used <= 400, "the plan must respect the budget");
        }
        Expect(changes == 0 && steps < 64, "repeated plans must settle");
        Expect(items[11].Level == 0, "the most important resource must be detailed");
    }

    StreamingView view;
    view.ProjectionScaleY = 1.0f;
    view.ViewHeight = 1000.0f;
    const BoundingBox bounds(vec3(-1.0f, -1.0f, 9.0f), vec3(1.0f, 1.0f, 11.0f));
    Expect(ComputeBoundsDistance(view.CameraPosition, bounds) == 9.0f && ComputeBoundsDistance(vec3(0.0f, 0.0f, 10.0f), bounds) == 0.0f, "distance must use the nearest point of the bounds");
    const f32 pixels = ComputePixelsPerUV(view, bounds, 1.0f, 1.0f);
    Expect(std::abs(pixels - 500.0f / 9.0f) < 0.01f, "pixels per UV must follow the distance");
    Expect(std::abs(ComputePixelsPerUV(view, bounds, 2.0f, 1.0f) - pixels * 2.0f) < 0.01f && std::abs(ComputePixelsPerUV(view, bounds, 1.0f, 4.0f) - pixels / 4.0f) < 0.01f,
        "pixels per UV must follow the scale and the UV density");
    Expect(ComputePixelsPerUV(view, bounds, 1.0f, 0.0f) == 0.0f && ComputePixelsPerUV(view, bounds, 0.0f, 1.0f) == 0.0f, "unknown UVs must ask for full detail");
    Expect(ComputeStreamingPriority(view, bounds, 0.4f) == 0.4f, "visible resources must keep their priority");
    view.Forward = vec3(0.0f, 0.0f, -1.0f);
    Expect(ComputeStreamingPriority(view, bounds, 0.4f) == 0.1f, "resources behind the camera must wait");
    view.Orthographic = true;
    Expect(ComputePixelsPerUV(view, bounds, 1.0f, 1.0f) == 500.0f, "orthographic views must not depend on the distance");

    return g_Failures == 0 ? 0 : 1;
}
