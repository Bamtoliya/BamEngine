#include "AutoExposure.h"

#include <array>
#include <cmath>
#include <cstdio>
#include <limits>

using namespace Engine;
using namespace Engine::AutoExposure;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "AutoExposureCheck failed: %s\n", message);
        ++g_Failures;
    }

    bool Near(f32 a, f32 b, f32 epsilon = 1e-3f) { return std::abs(a - b) <= epsilon; }

    using Histogram = std::array<uint32, HistogramBinCount>;

    void CheckBins()
    {
        Expect(LuminanceToBin(0.0f) == 0, "black goes to the first bin");
        Expect(LuminanceToBin(-1.0f) == 0, "negative luminance goes to the first bin");
        Expect(LuminanceToBin(std::numeric_limits<f32>::quiet_NaN()) == 0, "NaN goes to the first bin");
        Expect(LuminanceToBin(std::numeric_limits<f32>::infinity()) == HistogramBinCount - 1, "infinity goes to the last bin");
        Expect(LuminanceToBin(1e9f) == HistogramBinCount - 1, "values above the range clamp to the last bin");
        Expect(LuminanceToBin(1e-9f) == 0, "values below the range clamp to the first bin");

        uint32 previous = 0;
        for (f32 luminance = 0.001f; luminance < 1000.0f; luminance *= 1.5f)
        {
            const uint32 bin = LuminanceToBin(luminance);
            Expect(bin >= previous, "bins must not decrease as luminance grows");
            Expect(std::abs(BinToLogLuminance(bin) - std::log2(luminance)) <= (MaxLogLuminance - MinLogLuminance) / HistogramBinCount,
                "bin center must be within one bin of the luminance");
            previous = bin;
        }
    }

    void CheckAverage()
    {
        Histogram empty{};
        Expect(Near(AverageLogLuminance(empty), std::log2(MiddleGray)), "empty histogram falls back to middle gray");
        Expect(Near(TargetExposureEV(AverageLogLuminance(empty), -4.0f, 4.0f, 0.0f), 0.0f), "empty histogram gives 0 EV");

        Histogram uniform{};
        const uint32 bin = LuminanceToBin(1.0f);
        uniform[bin] = 1000;
        Expect(Near(AverageLogLuminance(uniform), BinToLogLuminance(bin)), "single bin average is that bin");

        // 화면의 5%를 차지하는 아주 밝은 광원은 상위 10% 제외로 평균에 들어가지 않는다.
        Histogram withSun = uniform;
        withSun[HistogramBinCount - 1] = 50;
        Expect(Near(AverageLogLuminance(withSun), BinToLogLuminance(bin)), "small bright area must be ignored");

        // 화면의 절반이 4 EV 밝으면 평균도 밝아진다.
        Histogram half{};
        half[LuminanceToBin(1.0f)] = 500;
        half[LuminanceToBin(16.0f)] = 500;
        const f32 average = AverageLogLuminance(half);
        Expect(average > BinToLogLuminance(LuminanceToBin(1.0f)) + 1.0f && average < BinToLogLuminance(LuminanceToBin(16.0f)) - 1.0f,
            "half bright scene averages between the two");
    }

    void CheckTarget()
    {
        Expect(Near(TargetExposureEV(std::log2(MiddleGray), -4.0f, 4.0f, 0.0f), 0.0f), "middle gray needs no exposure");
        Expect(Near(TargetExposureEV(std::log2(MiddleGray) + 2.0f, -4.0f, 4.0f, 0.0f), -2.0f), "4x brighter scene needs -2 EV");
        Expect(Near(TargetExposureEV(std::log2(MiddleGray) - 10.0f, -4.0f, 4.0f, 0.0f), 4.0f), "dark scene clamps to max EV");
        Expect(Near(TargetExposureEV(std::log2(MiddleGray) + 10.0f, -4.0f, 4.0f, 0.0f), -4.0f), "bright scene clamps to min EV");
        Expect(Near(TargetExposureEV(std::log2(MiddleGray) + 10.0f, -4.0f, 4.0f, 1.0f), -3.0f), "compensation is added after clamping");
        Expect(Near(TargetExposureEV(std::log2(MiddleGray) - 10.0f, 4.0f, -4.0f, 0.0f), 4.0f), "swapped min and max still clamp");
    }

    void CheckAdapt()
    {
        Expect(Near(AdaptExposureEV(1.0f, -3.0f, 0.0f, 3.0f, 1.0f), 1.0f), "zero time keeps the exposure");
        Expect(Near(AdaptExposureEV(1.0f, -3.0f, -0.1f, 3.0f, 1.0f), 1.0f), "negative time keeps the exposure");
        Expect(Near(AdaptExposureEV(1.0f, -3.0f, 100.0f, 3.0f, 1.0f), -3.0f), "long time reaches the target");
        Expect(Near(AdaptExposureEV(0.0f, 2.0f, 1.0f, 3.0f, 1.0f), 2.0f * (1.0f - std::exp(-1.0f))), "one time constant moves 63%");

        // 밝아질 때(노출을 낮출 때)는 SpeedToLight, 어두워질 때는 SpeedToDark를 쓴다.
        const f32 toLight = 0.0f - AdaptExposureEV(0.0f, -2.0f, 0.1f, 3.0f, 1.0f);
        const f32 toDark = AdaptExposureEV(0.0f, 2.0f, 0.1f, 3.0f, 1.0f) - 0.0f;
        Expect(toLight > toDark, "adapting to light must be faster with the default speeds");

        // 프레임을 잘게 나눠도 같은 시간 동안 같은 만큼 움직인다.
        f32 stepped = 0.0f;
        for (int frame = 0; frame < 60; ++frame)
            stepped = AdaptExposureEV(stepped, 2.0f, 1.0f / 60.0f, 3.0f, 1.0f);
        Expect(Near(stepped, AdaptExposureEV(0.0f, 2.0f, 1.0f, 3.0f, 1.0f)), "adaptation must not depend on frame rate");

        f32 value = 4.0f;
        for (int frame = 0; frame < 1000; ++frame)
        {
            const f32 next = AdaptExposureEV(value, -1.0f, 1.0f / 30.0f, 3.0f, 1.0f);
            Expect(next <= value && next >= -1.0f, "adaptation must approach the target without overshoot");
            value = next;
        }
    }
}

int main()
{
    CheckBins();
    CheckAverage();
    CheckTarget();
    CheckAdapt();

    if (g_Failures == 0)
        std::printf("AutoExposureCheck passed.\n");
    return g_Failures == 0 ? 0 : 1;
}
