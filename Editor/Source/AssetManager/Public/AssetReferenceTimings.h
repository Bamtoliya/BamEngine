#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace Editor
{
    enum class AssetReferenceTimingKind
    {
        Initial,
        Manual,
        Automatic,
        AfterRepair,
        DeleteImpact,
        MovePreparation,
        MoveRecheck,
        Incremental,
        Count
    };

    struct AssetReferenceTiming
    {
        std::uint64_t Samples = 0;
        double LastMs = 0.0;
        double TotalMs = 0.0;
        double PeakMs = 0.0;

        double AverageMs() const
        {
            return Samples == 0 ? 0.0 : TotalMs / static_cast<double>(Samples);
        }
    };

    struct AssetReferenceScanTiming
    {
        AssetReferenceTimingKind Kind = AssetReferenceTimingKind::Count;
        int Frame = -1;
        std::size_t Directories = 0;
        std::size_t Documents = 0;
        std::size_t References = 0;
        std::size_t Issues = 0;
        double TraversalMs = 0.0;
        double ReadParseMs = 0.0;
        double ResolveMs = 0.0;
        double RefreshViewMs = 0.0;
    };

    // Record samples on the editor thread; only synchronous work contributes to per-frame totals.
    struct AssetReferenceTimings
    {
        static constexpr std::size_t KindCount = static_cast<std::size_t>(AssetReferenceTimingKind::Count);
        std::array<AssetReferenceTiming, KindCount> ByKind{};
        int LastFrame = -1;
        double LastFrameMs = 0.0;
        double PeakFrameMs = 0.0;
        AssetReferenceScanTiming LastScan;

        void Record(AssetReferenceTimingKind kind, int frame, double elapsedMs,
            const AssetReferenceScanTiming* scan = nullptr, bool includeFrame = true)
        {
            const auto index = static_cast<std::size_t>(kind);
            if (index >= ByKind.size())
                return;

            auto& sample = ByKind[index];
            ++sample.Samples;
            sample.LastMs = elapsedMs;
            sample.TotalMs += elapsedMs;
            sample.PeakMs = (std::max)(sample.PeakMs, elapsedMs);

            if (scan)
            {
                LastScan = *scan;
                LastScan.Kind = kind;
                LastScan.Frame = frame;
            }

            if (!includeFrame)
                return;

            if (LastFrame != frame)
            {
                LastFrame = frame;
                LastFrameMs = 0.0;
            }
            LastFrameMs += elapsedMs;
            PeakFrameMs = (std::max)(PeakFrameMs, LastFrameMs);
        }

        void RecordBackground(AssetReferenceTimingKind kind, double elapsedMs)
        {
            Record(kind, -1, elapsedMs, nullptr, false);
        }
    };
}
