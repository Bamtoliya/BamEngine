#include "MotionHistory.h"
#include "RenderCommands.h"
#include "TemporalAA.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace Engine;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "TemporalAACheck failed: %s\n", message);
        ++g_Failures;
    }

    vec2 ToPixels(const mat4& viewProjection, const vec3& position, float width, float height)
    {
        const vec4 clip = viewProjection * vec4(position, 1.0f);
        return vec2((clip.x / clip.w * 0.5f + 0.5f) * width, (0.5f - clip.y / clip.w * 0.5f) * height);
    }

    void CheckJitter()
    {
        vec2 sum(0.0f);
        for (uint32 frame = 0; frame < TemporalJitterCount; ++frame)
        {
            const vec2 jitter = TemporalJitterPixels(frame);
            Expect(std::abs(jitter.x) <= 0.5f && std::abs(jitter.y) <= 0.5f, "jitter must stay inside the pixel");
            for (uint32 other = 0; other < frame; ++other)
                Expect(TemporalJitterPixels(other) != jitter, "jitter samples must differ");
            sum += jitter;
        }
        sum /= static_cast<float>(TemporalJitterCount);
        Expect(std::abs(sum.x) < 0.1f && std::abs(sum.y) < 0.1f, "jitter must be centered");
        Expect(TemporalJitterPixels(0) == TemporalJitterPixels(TemporalJitterCount), "jitter must repeat");
    }

    void CheckCamera()
    {
        const vec3 eye(1.0f, 2.0f, -5.0f);
        const mat4 view = glm::lookAtLH(eye, vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
        const vec2 jitter(0.25f, -0.375f);
        const vec3 points[] = { vec3(0.0f), vec3(1.0f, -0.5f, 2.0f), vec3(-2.0f, 1.0f, 30.0f) };
        for (const bool leftHanded : { true, false })
        {
            const mat4 projection = leftHanded ? glm::perspectiveLH_ZO(1.0f, 16.0f / 9.0f, 0.1f, 100.0f) : glm::perspectiveRH_ZO(1.0f, 16.0f / 9.0f, 0.1f, 100.0f);
            const mat4 handView = leftHanded ? view : glm::lookAtRH(eye, vec3(0.0f), vec3(0.0f, 1.0f, 0.0f));
            const auto camera = CameraBuffer::FromMatrices(handView, projection, eye, 0.0f);
            Expect(SupportsTemporalAA(camera), "perspective cameras must be supported");
            const auto jittered = MakeJitteredCamera(camera, jitter, 1920, 1080);
            for (const vec3& point : points)
            {
                const vec2 moved = ToPixels(jittered.viewProjMatrix, point, 1920.0f, 1080.0f) - ToPixels(camera.viewProjMatrix, point, 1920.0f, 1080.0f);
                Expect(glm::distance(moved, jitter) < 0.01f, "every depth must move by the jitter");

                // 격자는 지터 카메라의 NDC에서 이 값을 빼 원래 카메라의 방향을 찾는다.
                const vec4 clip = camera.viewProjMatrix * vec4(point, 1.0f);
                const vec4 jitteredClip = jittered.viewProjMatrix * vec4(point, 1.0f);
                const vec2 recovered = vec2(jitteredClip) / jitteredClip.w - TemporalJitterNDC(jitter, 1920, 1080);
                Expect(glm::distance(recovered, vec2(clip) / clip.w) < 1e-5f, "the NDC jitter must undo the jittered projection");
                Expect(std::abs(jitteredClip.z / jitteredClip.w - clip.z / clip.w) < 1e-6f, "jitter must not change depth");
            }
            Expect(jittered.cameraPosition == camera.cameraPosition && jittered.viewMatrix == camera.viewMatrix && jittered.projMatrix[1][1] == camera.projMatrix[1][1], "jitter must not move or zoom the camera");
        }
        const auto orthographic = CameraBuffer::FromMatrices(view, glm::orthoLH_ZO(-1.0f, 1.0f, -1.0f, 1.0f, 0.1f, 100.0f), eye, 0.0f);
        Expect(!SupportsTemporalAA(orthographic), "orthographic cameras must be excluded");
    }

    mat4 At(float x) { return glm::translate(mat4(1.0f), vec3(x, 0.0f, 0.0f)); }

    void CheckMotion()
    {
        MotionHistory history;
        history.Begin();
        history.Record(1, At(0.0f));
        history.Record(2, At(0.0f));
        Expect(!history.FindMoved(1), "new objects must not have motion");
        history.End();

        history.Begin();
        history.Record(1, At(1.0f));
        history.Record(1, At(1.0f));
        history.Record(2, At(0.0f));
        const auto* moved = history.FindMoved(1);
        Expect(moved && moved->World == At(0.0f), "submeshes must share the previous transform");
        Expect(!history.FindMoved(2) && !history.FindMoved(3), "still and unknown objects must not have motion");
        history.End();

        // 한 번호가 여러 변환으로 그려지면 다음 프레임까지 쓰지 않는다.
        history.Begin();
        history.Record(1, At(2.0f));
        history.Record(1, At(9.0f));
        Expect(!history.FindMoved(1), "ambiguous objects must not have motion");
        history.End();
        history.Begin();
        history.Record(1, At(3.0f));
        Expect(!history.FindMoved(1), "ambiguous objects must not be reused");
        history.End();

        // 건너뛴 프레임이 있으면 오래된 변환을 쓰지 않는다.
        history.Begin();
        history.End();
        history.Begin();
        history.Record(1, At(4.0f));
        Expect(!history.FindMoved(1), "objects that were not drawn last frame must not have motion");
        history.End();

        std::vector<mat4> bones(2, mat4(1.0f));
        history.Begin();
        history.Record(5, At(0.0f), &bones);
        history.End();
        bones[1] = At(0.5f);
        history.Begin();
        history.Record(5, At(0.0f), &bones);
        const auto* posed = history.FindMoved(5);
        Expect(posed && posed->Bones.size() == 2 && posed->Bones[1] == mat4(1.0f), "a pose change must be motion and keep the previous bones");
        history.End();
        bones.push_back(mat4(1.0f));
        history.Begin();
        history.Record(5, At(0.0f), &bones);
        Expect(!history.FindMoved(5), "a different skeleton must not have motion");
        history.End();

        history.Begin();
        for (uint64 id = 100; id < 5000; ++id) history.Record(id, mat4(1.0f));
        history.End();
        history.Begin();
        history.Record(1, At(0.0f));
        history.End();
        Expect(history.GetEntryCount() == 1, "objects that are no longer drawn must be removed");
        history.Reset();
        Expect(history.GetEntryCount() == 0, "a reset must drop every object");
    }

    void CheckShapeStamp()
    {
        // 정점이 바뀔 수 있는 메시: 형상 값이 같은 동안만 월드 행렬 이동을 움직임으로 쓴다.
        const int mesh = 0;
        const int otherMesh = 0;
        const uint64 stampV1 = MakeMotionStamp(&mesh, 1u);
        Expect(stampV1 == MakeMotionStamp(&mesh, 1u), "the same shape must have the same stamp");
        Expect(stampV1 != MakeMotionStamp(&mesh, 2u), "a new geometry version must change the stamp");
        Expect(stampV1 != MakeMotionStamp(&otherMesh, 1u), "a different mesh must change the stamp");
        Expect(MakeHeightfieldMotionStamp(&mesh, 10.0f, 1.0f, 1.0f) != MakeHeightfieldMotionStamp(&mesh, 20.0f, 1.0f, 1.0f), "a different height range must change the heightfield stamp");
        Expect(MakeHeightfieldMotionStamp(&mesh, 10.0f, 1.0f, 1.0f) != MakeHeightfieldMotionStamp(&mesh, 10.0f, 1.0f, 2.0f), "a different cell size must change the heightfield stamp");
        Expect(MakeHeightfieldMotionStamp(&mesh, 10.0f, 1.0f, 1.0f) == MakeHeightfieldMotionStamp(&mesh, 10.0f, 1.0f, 1.0f), "the same heightfield must have the same stamp");

        MotionHistory history;
        history.Begin();
        history.Record(1, At(0.0f), nullptr, stampV1);
        history.End();

        history.Begin();
        history.Record(1, At(1.0f), nullptr, stampV1);
        const auto* moved = history.FindMoved(1);
        Expect(moved && moved->World == At(0.0f), "a moved mesh with the same shape must have motion");
        history.End();

        // 정점이 바뀐 프레임은 이전 월드 행렬로 이동량을 구할 수 없다.
        history.Begin();
        history.Record(1, At(2.0f), nullptr, MakeMotionStamp(&mesh, 2u));
        Expect(!history.FindMoved(1), "a changed shape must not have motion");
        history.End();
        // 다음 프레임에는 새 형상이 기준이 된다.
        history.Begin();
        history.Record(1, At(3.0f), nullptr, MakeMotionStamp(&mesh, 2u));
        Expect(history.FindMoved(1) != nullptr, "the new shape must become the baseline");
        history.End();

        // 한 번호가 다른 형상으로 같은 프레임에 두 번 그려지면 모호하다.
        history.Begin();
        history.Record(1, At(4.0f), nullptr, stampV1);
        history.Record(1, At(4.0f), nullptr, MakeMotionStamp(&mesh, 5u));
        Expect(!history.FindMoved(1), "one id drawn with two shapes must be ambiguous");
        history.End();
    }

    void CheckObjectKeys()
    {
        RenderObjectId base{ 7u, 42u };
        Expect(base.GetKey() == ((static_cast<uint64>(7u) << 32u) | 42u), "a sub id of zero must keep the entity key");
        Expect(!RenderObjectId{}.IsValid() && base.IsValid(), "a zero scene id must not be tracked");

        // 한 엔티티의 스플라인 배치 인스턴스는 서로 다른 번호여야 각자 이전 변환을 찾는다.
        std::vector<uint64> keys;
        for (uint32 instance = 0; instance < 4096u; ++instance)
            keys.push_back(RenderObjectId{ 7u, 42u, instance }.GetKey());
        keys.push_back(RenderObjectId{ 7u, 43u }.GetKey());
        std::sort(keys.begin(), keys.end());
        Expect(std::adjacent_find(keys.begin(), keys.end()) == keys.end(), "instances of one entity must have distinct keys");

        MotionHistory history;
        history.Begin();
        for (uint32 instance = 0; instance < 3u; ++instance)
            history.Record(RenderObjectId{ 7u, 42u, instance }.GetKey(), At(static_cast<float>(instance)));
        history.End();
        history.Begin();
        for (uint32 instance = 0; instance < 3u; ++instance)
            history.Record(RenderObjectId{ 7u, 42u, instance }.GetKey(), At(static_cast<float>(instance) + (instance == 1u ? 1.0f : 0.0f)));
        Expect(!history.FindMoved(RenderObjectId{ 7u, 42u, 0u }.GetKey()) && !history.FindMoved(RenderObjectId{ 7u, 42u, 2u }.GetKey()), "still instances must not have motion");
        const auto* moved = history.FindMoved(RenderObjectId{ 7u, 42u, 1u }.GetKey());
        Expect(moved && moved->World == At(1.0f), "each instance must keep its own previous transform");
        history.End();
    }

    void CheckLimits()
    {
        // 인스턴스 링 버퍼를 다른 패스와 나눠 쓰므로 한도를 늘릴 때는 링 버퍼 크기와 함께 확인한다.
        static_assert(MaxMotionStaticInstances == 16384u && MaxMotionSkinnedDraws == 128u && MaxMotionTerrainDraws == 16u);
        Expect(MaxMotionTerrainDraws <= MaxMotionSkinnedDraws, "terrain draws are expensive and must stay below the skinned limit");
    }
}

int main()
{
    CheckJitter();
    CheckCamera();
    CheckMotion();
    CheckShapeStamp();
    CheckObjectKeys();
    CheckLimits();
    return g_Failures == 0 ? 0 : 1;
}
