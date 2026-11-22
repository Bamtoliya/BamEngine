#include "LODSelection.h"

#include <cstdio>

using namespace Engine;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "LODSelectionCheck failed: %s\n", message);
        ++g_Failures;
    }
}

int main()
{
    LODThresholds thresholds;
    thresholds.HasLOD1 = true;
    thresholds.HasLOD2 = true;
    Expect(SelectLODIndex(0.5f, 0, thresholds) == 0, "large objects must use LOD0");
    Expect(SelectLODIndex(0.2f, 0, thresholds) == 1, "medium objects must use LOD1");
    Expect(SelectLODIndex(0.05f, 0, thresholds) == 2, "small objects must use LOD2");

    // 경계 구간에서는 같은 뷰의 현재 LOD를 유지한다.
    Expect(SelectLODIndex(0.09f, 2, thresholds) == 2 && SelectLODIndex(0.09f, 1, thresholds) == 1, "LOD2 boundary must keep the current LOD");
    Expect(SelectLODIndex(0.26f, 1, thresholds) == 1 && SelectLODIndex(0.26f, 0, thresholds) == 0, "LOD1 boundary must keep the current LOD");

    thresholds.HasLOD2 = false;
    Expect(SelectLODIndex(0.01f, 0, thresholds) == 1, "a missing LOD2 must fall back to LOD1");
    thresholds.HasLOD1 = false;
    Expect(SelectLODIndex(0.01f, 0, thresholds) == 0, "missing LODs must fall back to LOD0");

    // 읽는 중인 LOD는 더 단순한 LOD, 그다음 더 자세한 LOD로 대신한다.
    Expect(ResolveResidentLOD(0, 0b111) == 0 && ResolveResidentLOD(2, 0b111) == 2, "a resident LOD must be used as is");
    Expect(ResolveResidentLOD(0, 0b100) == 2 && ResolveResidentLOD(0, 0b110) == 1, "a missing LOD must use the next coarser one");
    Expect(ResolveResidentLOD(2, 0b011) == 1 && ResolveResidentLOD(1, 0b001) == 0, "a missing coarse LOD must use the next finer one");
    Expect(ResolveResidentLOD(0, 0) == -1, "nothing must be drawn without a resident LOD");

    // 두 뷰가 번갈아 갱신해도 각 뷰의 LOD는 바뀌지 않는다.
    thresholds.HasLOD1 = thresholds.HasLOD2 = true;
    ViewLODState state;
    state.Set(10, 1);
    state.Set(20, 2);
    for (int frame = 0; frame < 8; ++frame)
    {
        state.Set(10, SelectLODIndex(0.09f, state.Get(10), thresholds));
        state.Set(20, SelectLODIndex(0.02f, state.Get(20), thresholds));
    }
    Expect(state.Get(10) == 1 && state.Get(20) == 2, "views must not overwrite each other's LOD");
    Expect(state.Get(30) == 0, "an unknown view must start at LOD0");
    for (std::uint32_t view = 100; view < 120; ++view)
        state.Set(view, 1);
    Expect(state.Get(119) == 1 && state.Get(10) == 0, "old views must be dropped first");

    Expect(ComputeScreenCoverage(1.0f, 2.0f, 10.0f) == 0.2f, "coverage must scale with radius and projection");
    Expect(ComputeScreenCoverage(5.0f, 2.0f, 0.0f) == 1.0f, "coverage must clamp when the camera is inside the bounds");
    Expect(ComputeScreenCoverage(1.0f, 2.0f, 5.0f) > ComputeScreenCoverage(1.0f, 2.0f, 20.0f), "coverage must shrink with distance");

    return g_Failures == 0 ? 0 : 1;
}
