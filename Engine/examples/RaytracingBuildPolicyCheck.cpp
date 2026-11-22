#include "RaytracingBuildPolicy.h"

#include <cstdio>

using namespace Engine;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "RaytracingBuildPolicyCheck failed: %s\n", message);
        ++g_Failures;
    }
}

int main()
{
    RaytracingBuildBudget countLimited(2, 1000);
    Expect(countLimited.TryConsume(1) && countLimited.TryConsume(1), "builds within the count limit must pass");
    Expect(!countLimited.TryConsume(1), "the count limit must stop further builds");
    countLimited.Reset();
    Expect(countLimited.TryConsume(1), "reset must restore the budget");

    RaytracingBuildBudget triangleLimited(16, 1000);
    Expect(triangleLimited.TryConsume(600), "the first build must pass");
    Expect(!triangleLimited.TryConsume(401), "a build exceeding the triangle limit must wait");
    Expect(triangleLimited.TryConsume(400), "a build that exactly fills the triangle limit must pass");
    Expect(!triangleLimited.TryConsume(1), "a full triangle budget must stop further builds");

    RaytracingBuildBudget oversized(16, 1000);
    Expect(oversized.TryConsume(5000), "an oversized mesh must be allowed as the first build of a frame");
    Expect(!oversized.TryConsume(1), "nothing else may follow an oversized build");
    oversized.Reset();
    Expect(oversized.TryConsume(10) && !oversized.TryConsume(5000), "an oversized mesh must wait when it is not first");

    // 준비된 메시는 예산을 쓰지 않으므로, 프레임이 지나면 앞선 메시가 빠지고 밀린 큰 메시도 빌드된다.
    const std::uint64_t meshes[] = { 10, 5000, 20 };
    bool ready[] = { false, false, false };
    int frames = 0;
    RaytracingBuildBudget drained(16, 1000);
    while (frames < 10 && !(ready[0] && ready[1] && ready[2]))
    {
        drained.Reset();
        for (int i = 0; i < 3; ++i)
            ready[i] = ready[i] || drained.TryConsume(meshes[i]);
        ++frames;
    }
    Expect(frames == 2, "pending meshes must drain without starving the oversized one");

    RaytracedCasterSet casters;
    Expect(!casters.Contains(0), "an unused set must be empty");
    casters.Begin();
    casters.Add(3);
    Expect(casters.Contains(3), "an added caster must be found");
    Expect(!casters.Contains(2) && !casters.Contains(4) && !casters.Contains(100000), "other entities must not be found");
    casters.Begin();
    Expect(!casters.Contains(3), "a new build must drop the previous casters");
    casters.Add(0);
    Expect(casters.Contains(0) && !casters.Contains(3), "only casters of the current build must remain");

    return g_Failures == 0 ? 0 : 1;
}
