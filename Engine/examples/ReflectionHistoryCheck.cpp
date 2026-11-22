#include "ReflectionHistory.h"

#include <cstdio>

using namespace Engine;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "ReflectionHistoryCheck failed: %s\n", message);
        ++g_Failures;
    }

    CameraBuffer Camera(float x)
    {
        CameraBuffer camera{};
        camera.viewProjMatrix = mat4(1.0f + x);
        camera.cameraPosition = vec3(x, 0.0f, 0.0f);
        return camera;
    }
}

int main()
{
    ReflectionHistory history;
    const int sceneA = 0;
    const int sceneB = 0;

    const auto first = history.Begin(Camera(0.0f), &sceneA, 640, 480);
    Expect(!first.Valid && first.ReadIndex != first.WriteIndex, "the first frame must not use history");
    history.Commit();

    const auto second = history.Begin(Camera(1.0f), &sceneA, 640, 480);
    Expect(second.CameraMoved, "a different view must be reported");
    Expect(second.Valid && second.ReadIndex == first.WriteIndex && second.WriteIndex == first.ReadIndex, "the next frame must read what was written");
    Expect(second.PrevCameraPosition.x == 0.0f && second.PrevViewProjection == mat4(1.0f) && second.FrameIndex == first.FrameIndex + 1, "the previous camera must be kept");
    history.Commit();

    const auto still = history.Begin(Camera(1.0f), &sceneA, 640, 480);
    Expect(still.Valid && !still.CameraMoved, "the same view must be reported as still");
    history.Commit();
    auto shifted = Camera(1.0f);
    shifted.viewProjMatrix[3][0] += 0.001f;
    Expect(history.Begin(shifted, &sceneA, 640, 480).CameraMoved, "small view changes must be reported");
    history.Commit();

    Expect(!history.Begin(Camera(1.0f), &sceneB, 640, 480).Valid, "another scene must drop the history");
    history.Commit();
    Expect(!history.Begin(Camera(1.0f), &sceneB, 800, 480).Valid, "another size must drop the history");
    history.Commit();
    Expect(!history.Begin(Camera(1.0f + ReflectionHistory::MaxCameraStep), &sceneB, 800, 480).Valid, "a camera cut must drop the history");
    history.Commit();
    Expect(history.Begin(Camera(1.5f + ReflectionHistory::MaxCameraStep), &sceneB, 800, 480).Valid, "small camera moves must keep the history");

    // 중간 패스가 실패하면 다음 프레임이 반쯤 쓴 결과를 이어 쓰지 않는다.
    Expect(!history.Begin(Camera(1.5f + ReflectionHistory::MaxCameraStep), &sceneB, 800, 480).Valid, "an unfinished frame must not be reused");
    history.Commit();
    history.Reset();
    Expect(!history.Begin(Camera(1.5f + ReflectionHistory::MaxCameraStep), &sceneB, 800, 480).Valid, "a reset must drop the history");

    return g_Failures == 0 ? 0 : 1;
}
