#include "ProbeGrid.h"

#include <cmath>
#include <cstdio>
#include <set>

using namespace Engine;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "ProbeGridCheck failed: %s\n", message);
        ++g_Failures;
    }

    bool Near(f32 a, f32 b, f32 epsilon = 1e-4f) { return std::abs(a - b) <= epsilon; }
    bool Near(const vec3& a, const vec3& b, f32 epsilon = 1e-4f) { return glm::length(a - b) <= epsilon; }

    void CheckCounts()
    {
        const auto counts = ProbeGrid::ClampCounts(1, 100, -5);
        Expect(counts == ProbeGrid::GridCounts{ 2u, 16u, 2u }, "counts must be clamped per axis");
        Expect(ProbeGrid::ClampCounts(16, 8, 16).Total() == 2048u, "the default grid has 2048 probes");
        Expect(ProbeGrid::ClampRaysPerProbe(0) == ProbeGrid::MinRaysPerProbe && ProbeGrid::ClampRaysPerProbe(9999) == ProbeGrid::MaxRaysPerProbe,
            "rays per probe must be clamped");
    }

    void CheckIndexing()
    {
        const ProbeGrid::GridCounts counts{ 5u, 3u, 4u };
        bool roundTrip = true;
        std::set<uint32> seen;
        for (uint32 index = 0; index < counts.Total(); ++index)
        {
            const auto coord = ProbeGrid::ToCoord(counts, index);
            roundTrip &= coord.X < counts.X && coord.Y < counts.Y && coord.Z < counts.Z && ProbeGrid::ToIndex(counts, coord) == index;
            seen.insert(index);
        }
        Expect(roundTrip && seen.size() == counts.Total(), "index and coordinate must convert both ways");
        Expect(ProbeGrid::ToIndex(counts, { 1u, 2u, 3u }) == 1u + 2u * 5u + 3u * 15u, "x changes fastest, then y, then z");
    }

    void CheckPlacement()
    {
        const auto grid = ProbeGrid::MakeGrid(vec3(10.0f, 2.0f, -4.0f), vec3(8.0f, 4.0f, 6.0f), 5, 3, 4);
        Expect(Near(grid.Origin, vec3(6.0f, 0.0f, -7.0f)), "the first probe sits on the minimum corner");
        Expect(Near(grid.Spacing, vec3(2.0f, 2.0f, 2.0f)), "probes span the whole box including both corners");
        Expect(Near(ProbeGrid::ProbePosition(grid, { 4u, 2u, 3u }), vec3(14.0f, 4.0f, -1.0f)), "the last probe sits on the maximum corner");

        const auto flat = ProbeGrid::MakeGrid(vec3(0.0f), vec3(-4.0f, 0.0f, 4.0f), 2, 2, 2);
        Expect(flat.Spacing.x > 0.0f && flat.Spacing.y > 0.0f, "negative or zero sizes must not give a zero or negative spacing");
    }

    void CheckInterpolation()
    {
        const auto grid = ProbeGrid::MakeGrid(vec3(0.0f), vec3(4.0f, 4.0f, 4.0f), 3, 3, 3); // 간격 2m, 원점 (-2, -2, -2)
        const auto inside = ProbeGrid::LocateCell(grid, vec3(-1.5f, 0.5f, 1.0f));
        Expect(inside.Base == ProbeGrid::GridCoord{ 0u, 1u, 1u }, "the base probe must be the lower corner of the cell");
        Expect(Near(inside.Alpha, vec3(0.25f, 0.25f, 0.5f)), "alpha must be the position inside the cell");

        const auto onMax = ProbeGrid::LocateCell(grid, vec3(2.0f, 2.0f, 2.0f));
        Expect(onMax.Base == ProbeGrid::GridCoord{ 1u, 1u, 1u } && Near(onMax.Alpha, vec3(1.0f)), "the far corner must stay in the last cell");
        const auto outside = ProbeGrid::LocateCell(grid, vec3(-9.0f, 9.0f, 0.0f));
        Expect(outside.Base.X == 0u && outside.Base.Y == 1u && Near(outside.Alpha.x, 0.0f) && Near(outside.Alpha.y, 1.0f),
            "positions outside the box must clamp to the nearest face");

        f32 sum = 0.0f;
        f32 weighted = 0.0f;
        for (uint32 corner = 0; corner < 8u; ++corner)
        {
            const f32 weight = ProbeGrid::TrilinearWeight(inside.Alpha, corner);
            sum += weight;
            weighted += weight * static_cast<f32>(ProbeGrid::CornerOffset(corner).X);
        }
        Expect(Near(sum, 1.0f), "the eight weights must add up to one");
        Expect(Near(weighted, inside.Alpha.x), "weights must interpolate linearly along each axis");
        Expect(Near(ProbeGrid::TrilinearWeight(vec3(0.0f), 0u), 1.0f) && Near(ProbeGrid::TrilinearWeight(vec3(1.0f), 7u), 1.0f),
            "a probe position must take only that probe");
    }

    void CheckVolumeBlend()
    {
        const auto grid = ProbeGrid::MakeGrid(vec3(0.0f), vec3(4.0f), 2, 2, 2);
        Expect(Near(ProbeGrid::VolumeBlendWeight(grid, vec3(0.0f), 1.0f), 1.0f), "inside the box uses only GI");
        Expect(Near(ProbeGrid::VolumeBlendWeight(grid, vec3(2.5f, 0.0f, 0.0f), 1.0f), 0.5f), "the edge blends linearly");
        Expect(Near(ProbeGrid::VolumeBlendWeight(grid, vec3(0.0f, 0.0f, 3.5f), 1.0f), 0.0f), "far outside uses only the old ambient light");
        Expect(Near(ProbeGrid::VolumeBlendWeight(grid, vec3(2.1f, 0.0f, 0.0f), 0.0f), 0.0f), "a zero blend distance cuts at the box");
    }

    void CheckOctahedral()
    {
        bool roundTrip = true;
        bool inRange = true;
        for (uint32 i = 0; i < 512u; ++i)
        {
            const vec3 direction = ProbeGrid::SphericalFibonacci(i, 512u);
            const vec2 encoded = ProbeGrid::EncodeOctahedral(direction);
            inRange &= std::abs(encoded.x) <= 1.0f + 1e-5f && std::abs(encoded.y) <= 1.0f + 1e-5f;
            roundTrip &= Near(ProbeGrid::DecodeOctahedral(encoded), direction, 1e-4f);
        }
        Expect(inRange, "encoded directions must stay in [-1, 1]");
        Expect(roundTrip, "octahedral encoding must round-trip");
        Expect(glm::length(ProbeGrid::EncodeOctahedral(vec3(0.0f, 1.0f, 0.0f))) < 1e-5f, "up maps to the centre");
        Expect(Near(ProbeGrid::DecodeOctahedral(vec2(1.0f, 1.0f)), vec3(0.0f, -1.0f, 0.0f)), "the corners map to down");

        // 경계는 가운데를 기준으로 접힌다. 테두리 복사 규칙이 이 성질을 쓴다.
        bool folded = true;
        for (f32 v = -0.9f; v <= 0.9f; v += 0.3f)
        {
            folded &= Near(ProbeGrid::DecodeOctahedral(vec2(-1.0f, v)), ProbeGrid::DecodeOctahedral(vec2(-1.0f, -v)), 1e-4f);
            folded &= Near(ProbeGrid::DecodeOctahedral(vec2(v, 1.0f)), ProbeGrid::DecodeOctahedral(vec2(-v, 1.0f)), 1e-4f);
        }
        Expect(folded, "each edge of the octahedral map must mirror around its middle");
    }

    void CheckAtlas()
    {
        const ProbeGrid::GridCounts counts{ 4u, 2u, 3u };
        const auto irradiance = ProbeGrid::GetAtlasSize(counts, ProbeGrid::IrradianceTexels);
        Expect(irradiance.Width == 8u * 10u && irradiance.Height == 3u * 10u, "the atlas has one 10x10 tile per probe");
        const uint32 probe = ProbeGrid::ToIndex(counts, { 3u, 1u, 2u });
        const auto origin = ProbeGrid::TileOrigin(counts, probe, ProbeGrid::IrradianceTexels);
        Expect(origin[0] == (3u + 1u * 4u) * 10u && origin[1] == 2u * 10u, "tiles are placed by x + y * countX and z");

        bool insideTile = true;
        for (uint32 i = 0; i < 64u; ++i)
        {
            const vec2 uv = ProbeGrid::AtlasUV(counts, probe, ProbeGrid::IrradianceTexels, ProbeGrid::SphericalFibonacci(i, 64u));
            const vec2 texel = uv * vec2(static_cast<f32>(irradiance.Width), static_cast<f32>(irradiance.Height));
            insideTile &= texel.x >= origin[0] + 1.0f - 1e-3f && texel.x <= origin[0] + 9.0f + 1e-3f;
            insideTile &= texel.y >= origin[1] + 1.0f - 1e-3f && texel.y <= origin[1] + 9.0f + 1e-3f;
        }
        Expect(insideTile, "samples must stay inside the tile's interior");

        bool centres = true;
        for (uint32 y = 1u; y <= ProbeGrid::IrradianceTexels; ++y)
        {
            for (uint32 x = 1u; x <= ProbeGrid::IrradianceTexels; ++x)
            {
                const vec2 uv = ProbeGrid::AtlasUV(counts, 0u, ProbeGrid::IrradianceTexels, ProbeGrid::TexelDirection(x, y, ProbeGrid::IrradianceTexels));
                const vec2 texel = uv * vec2(static_cast<f32>(irradiance.Width), static_cast<f32>(irradiance.Height));
                centres &= Near(texel.x, static_cast<f32>(x) + 0.5f, 1e-3f) && Near(texel.y, static_cast<f32>(y) + 0.5f, 1e-3f);
            }
        }
        Expect(centres, "a texel's direction must sample that texel's centre");
    }

    void CheckBorders()
    {
        constexpr uint32 n = ProbeGrid::IrradianceTexels;
        Expect(!ProbeGrid::IsBorderTexel(1u, n, n) && ProbeGrid::IsBorderTexel(0u, 3u, n) && ProbeGrid::IsBorderTexel(4u, n + 1u, n), "border detection");

        const auto corner = ProbeGrid::BorderSource(0u, 0u, n);
        Expect(corner[0] == n && corner[1] == n, "the top-left corner copies the opposite corner");
        const auto top = ProbeGrid::BorderSource(2u, 0u, n);
        Expect(top[0] == n - 1u && top[1] == 1u, "the top row copies the first row mirrored");
        const auto right = ProbeGrid::BorderSource(n + 1u, 3u, n);
        Expect(right[0] == n && right[1] == n - 2u, "the right column copies the last column mirrored");
        const auto inner = ProbeGrid::BorderSource(4u, 5u, n);
        Expect(inner[0] == 4u && inner[1] == 5u, "interior texels keep their value");

        // 테두리 텍셀은 접힌 맞은편 텍셀과 같은 방향을 이어야 한다: 좌측 테두리 (0, y)의 바깥쪽 이웃은 (1, n+1-y)이다.
        bool continuous = true;
        for (uint32 y = 1u; y <= n; ++y)
        {
            const auto source = ProbeGrid::BorderSource(0u, y, n);
            const f32 v = (static_cast<f32>(y) - 0.5f) / static_cast<f32>(n) * 2.0f - 1.0f;
            const f32 sourceV = (static_cast<f32>(source[1]) - 0.5f) / static_cast<f32>(n) * 2.0f - 1.0f;
            continuous &= source[0] == 1u && Near(ProbeGrid::DecodeOctahedral(vec2(-1.0f, v)), ProbeGrid::DecodeOctahedral(vec2(-1.0f, sourceV)), 1e-4f);
        }
        Expect(continuous, "left border texels must copy the texel on the other side of the fold");
    }

    void CheckSchedule()
    {
        constexpr uint32 total = 10u;
        const uint32 budget = ProbeGrid::ClampUpdateBudget(4, total);
        Expect(budget == 4u && ProbeGrid::ClampUpdateBudget(0, total) == 1u && ProbeGrid::ClampUpdateBudget(99, total) == total &&
            ProbeGrid::ClampUpdateBudget(4, 0u) == 0u, "the budget must be clamped to the probe count");

        std::set<uint32> covered;
        for (uint64 frame = 0; frame < 3u; ++frame)
        {
            for (uint32 i = 0; i < budget; ++i)
                covered.insert(ProbeGrid::ScheduledProbe(frame, i, budget, total));
        }
        Expect(covered.size() == total, "every probe must be updated within ceil(total / budget) frames");
        Expect(ProbeGrid::ScheduledProbe(2u, 3u, budget, total) == 1u, "the schedule wraps around");
    }

    void CheckRayDirections()
    {
        constexpr uint32 count = 128u;
        vec3 mean(0.0f);
        bool unit = true;
        for (uint32 i = 0; i < count; ++i)
        {
            const vec3 direction = ProbeGrid::SphericalFibonacci(i, count);
            unit &= Near(glm::length(direction), 1.0f, 1e-4f);
            mean += direction;
        }
        Expect(unit, "ray directions must be unit length");
        Expect(glm::length(mean / static_cast<f32>(count)) < 0.05f, "ray directions must cover the sphere evenly");
    }

    void CheckMemory()
    {
        const auto estimate = ProbeGrid::EstimateMemory(ProbeGrid::ClampCounts(16, 8, 16), 128u, 512u);
        // 조도 1280×160×8, 거리 2304×288×4, 광선 128×512×8, 상태 2048×8
        Expect(estimate.Irradiance == 1638400u && estimate.Distance == 2654208u && estimate.RayData == 524288u && estimate.ProbeData == 16384u,
            "the default memory estimate changed");
        Expect(estimate.Total() < 8u * 1024u * 1024u, "the default grid must fit in 8 MB per view");
    }
}

int main()
{
    CheckCounts();
    CheckIndexing();
    CheckPlacement();
    CheckInterpolation();
    CheckVolumeBlend();
    CheckOctahedral();
    CheckAtlas();
    CheckBorders();
    CheckSchedule();
    CheckRayDirections();
    CheckMemory();

    if (g_Failures == 0)
        std::printf("ProbeGridCheck passed\n");
    return g_Failures == 0 ? 0 : 1;
}
