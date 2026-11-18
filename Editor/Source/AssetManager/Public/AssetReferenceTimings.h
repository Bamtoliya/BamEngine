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

    // Synchronous editor operations. Record only non-overlapping scopes on the editor thread.
    struct AssetReferenceTimings
    {
        static constexpr std::size_t KindCount = static_cast<std::size_t>(AssetReferenceTimingKind::Count);
        std::array<AssetReferenceTiming, KindCount> ByKind{};
        int LastFrame = -1;
        double LastFrameMs = 0.0;
        double PeakFrameMs = 0.0;

        void Record(AssetReferenceTimingKind kind, int frame, double elapsedMs)
        {
            const auto index = static_cast<std::size_t>(kind);
            if (index >= ByKind.size())
                return;

            auto& sample = ByKind[index];
            ++sample.Samples;
            sample.LastMs = elapsedMs;
            sample.TotalMs += elapsedMs;
            sample.PeakMs = (std::max)(sample.PeakMs, elapsedMs);

            if (LastFrame != frame)
            {
                LastFrame = frame;
                LastFrameMs = 0.0;
            }
            LastFrameMs += elapsedMs;
            PeakFrameMs = (std::max)(PeakFrameMs, LastFrameMs);
        }
    };
}
