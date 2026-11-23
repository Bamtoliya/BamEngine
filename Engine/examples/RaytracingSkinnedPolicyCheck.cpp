#include "RaytracingSkinnedPolicy.h"
#include "RaytracingInstanceData.h"

#include <cstdio>

using namespace Engine;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "RaytracingSkinnedPolicyCheck failed: %s\n", message);
        ++g_Failures;
    }
}

int main()
{
    using Action = ESkinnedBottomLevelAction;
    constexpr std::uint32_t Interval = 300;

    // raytracing.hlsli의 RaytracedInstance가 같은 위치에서 법선 맵 번호를 읽는다.
    static_assert(sizeof(RaytracingInstanceData) == 96 && offsetof(RaytracingInstanceData, NormalTexture) == 80);
    Expect(RaytracingInstanceData{}.NormalTexture == 0xFFFFFFFFu, "a missing normal map must use the invalid bindless index");

    // BLAS가 없거나 기하가 바뀌면 새로 만든다(refit은 삼각형 구성을 바꾸지 못한다).
    Expect(ChooseSkinnedBottomLevelAction(false, true, 0, Interval) == Action::Build, "a mesh without a BLAS must build");
    Expect(ChooseSkinnedBottomLevelAction(true, false, 10, Interval) == Action::Build, "changed geometry must build, not refit");
    Expect(ChooseSkinnedBottomLevelAction(false, false, 999, Interval) == Action::Build, "build must win over the rebuild interval");

    // 같은 기하는 refit하고, 오래 이어지면 품질 회복을 위해 다시 만든다.
    Expect(ChooseSkinnedBottomLevelAction(true, true, 0, Interval) == Action::Refit, "the same geometry must refit");
    Expect(ChooseSkinnedBottomLevelAction(true, true, Interval - 1, Interval) == Action::Refit, "refit must continue below the interval");
    Expect(ChooseSkinnedBottomLevelAction(true, true, Interval, Interval) == Action::Rebuild, "the interval must trigger a rebuild");
    Expect(ChooseSkinnedBottomLevelAction(true, true, 100000, 0) == Action::Refit, "interval 0 must disable periodic rebuilds");

    // 한도까지만 차지하고, Reset 뒤 다시 채운다.
    RaytracingSkinnedLimit limit(2);
    Expect(limit.TryAcquire() && limit.TryAcquire(), "the limit must admit up to its size");
    Expect(!limit.TryAcquire() && limit.GetCount() == 2, "the limit must reject past its size without counting");
    limit.Reset();
    Expect(limit.GetCount() == 0 && limit.TryAcquire(), "reset must free every slot");

    RaytracingSkinnedLimit none(0);
    Expect(!none.TryAcquire(), "a zero limit must admit nothing");

    return g_Failures == 0 ? 0 : 1;
}
