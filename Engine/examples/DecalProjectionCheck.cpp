#include "DecalProjection.h"

#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <cstdio>

using namespace Engine;

namespace
{
    int g_Failures = 0;

    void Expect(bool condition, const char* message)
    {
        if (condition) return;
        std::fprintf(stderr, "DecalProjectionCheck failed: %s\n", message);
        ++g_Failures;
    }

    // 셰이더는 이 표로 상자를 만들고 바깥면을 버린다. 감김이 하나라도 틀리면 그 면에서 데칼이 두 번 섞이거나 사라진다.
    void CheckBoxWinding()
    {
        int faceCount[6] = {};
        for (std::size_t i = 0; i < DecalBoxIndices.size(); i += 3)
        {
            const vec3 a = GetDecalBoxCorner(DecalBoxIndices[i]);
            const vec3 b = GetDecalBoxCorner(DecalBoxIndices[i + 1]);
            const vec3 c = GetDecalBoxCorner(DecalBoxIndices[i + 2]);
            const vec3 center = (a + b + c) / 3.0f;
            vec3 outward(0.0f);
            int axis = 0;
            for (int k = 1; k < 3; ++k)
                if (std::abs(center[k]) > std::abs(center[axis])) axis = k;
            outward[axis] = center[axis] > 0.0f ? 1.0f : -1.0f;
            Expect(std::abs(a[axis] - b[axis]) < 1e-6f && std::abs(a[axis] - c[axis]) < 1e-6f, "each triangle must lie on one face");
            Expect(glm::dot(glm::cross(b - a, c - a), outward) > 0.0f, "box triangles must use the engine front-face winding");
            ++faceCount[axis * 2 + (outward[axis] > 0.0f ? 0 : 1)];
        }
        for (const int count : faceCount)
            Expect(count == 2, "every face must have two triangles");
    }

    void CheckProjection()
    {
        // 지면 데칼: x축으로 90도 돌려 상자의 z가 아래(-y)를 향한다.
        const mat4 world = glm::translate(mat4(1.0f), vec3(3.0f, 1.0f, -2.0f)) * glm::rotate(mat4(1.0f), glm::radians(90.0f), vec3(1.0f, 0.0f, 0.0f));
        const mat4 box = MakeDecalBoxMatrix(world, vec3(4.0f, 2.0f, 1.0f));
        mat4 worldToBox;
        Expect(TryGetDecalWorldToBox(box, worldToBox), "a box with volume must be accepted");
        Expect(!IsDecalBoxMirrored(box), "a positive scale must not be mirrored");

        const vec3 local = vec3(worldToBox * vec4(3.0f + 1.0f, 1.0f, -2.0f, 1.0f));
        Expect(IsInsideDecalBox(local) && std::abs(local.x - 0.25f) < 1e-5f, "a point on the ground must map inside the box");
        Expect(!IsInsideDecalBox(vec3(worldToBox * vec4(3.0f + 2.5f, 1.0f, -2.0f, 1.0f))), "a point past the width must be outside");
        Expect(!IsInsideDecalBox(vec3(worldToBox * vec4(3.0f, 1.6f, -2.0f, 1.0f))), "a point past the projection depth must be outside");

        const vec3 projectionAxis = glm::normalize(vec3(box[2]));
        Expect(std::abs(std::abs(projectionAxis.y) - 1.0f) < 1e-5f, "a rotated box must project along world y");

        const vec2 uv = GetDecalUV(vec3(-0.5f, 0.5f, 0.0f));
        Expect(uv == vec2(0.0f, 0.0f) && GetDecalUV(vec3(0.5f, -0.5f, 0.0f)) == vec2(1.0f, 1.0f), "box +y must be the top of the texture");

        mat4 unused;
        Expect(!TryGetDecalWorldToBox(MakeDecalBoxMatrix(world, vec3(1.0f, 0.0f, 1.0f)), unused), "a flat box must be rejected");
        Expect(IsDecalBoxMirrored(MakeDecalBoxMatrix(world, vec3(-1.0f, 1.0f, 1.0f))), "a negative scale must be mirrored");
    }

    void CheckFades()
    {
        const vec2 range = GetDecalAngleFadeRange(70.0f);
        Expect(GetDecalAngleFade(1.0f, range) == 1.0f && GetDecalAngleFade(-1.0f, range) == 1.0f, "surfaces facing the axis from either side must receive");
        Expect(GetDecalAngleFade(std::cos(glm::radians(80.0f)), range) == 0.0f, "surfaces past the maximum angle must not receive");
        const f32 middle = GetDecalAngleFade(std::cos(glm::radians(62.5f)), range);
        Expect(middle > 0.0f && middle < 1.0f, "the angle fade must be smooth inside its band");
        Expect(GetDecalAngleFade(std::cos(glm::radians(50.0f)), range) == 1.0f, "surfaces inside the angle must not fade");
        Expect(GetDecalAngleFade(0.0f, GetDecalAngleFadeRange(90.0f)) == 0.0f, "even 90 degrees must hide perpendicular surfaces");
        Expect(GetDecalAngleFade(1.0f, GetDecalAngleFadeRange(0.0f)) == 1.0f, "a zero angle must still receive facing surfaces");
        Expect(GetDecalAngleFadeRange(std::nanf("")).x < 1e-6f, "an invalid angle must fall back to 90 degrees");

        Expect(GetDecalDepthFade(0.0f, 0.1f) == 1.0f, "the box center must not fade");
        Expect(GetDecalDepthFade(0.5f, 0.1f) == 0.0f && GetDecalDepthFade(-0.5f, 0.1f) == 0.0f, "both box ends must fade out");
        Expect(std::abs(GetDecalDepthFade(0.475f, 0.1f) - 0.5f) < 1e-5f, "the depth fade must be linear in its band");
        Expect(GetDecalDepthFade(0.5f, 0.0f) == 1.0f && GetDecalDepthFade(0.6f, 0.0f) == 0.0f, "no depth fade must keep the whole box");

        Expect(GetDecalTrackAlpha(0.5f, 2.0f, 1.0f) == 1.0f, "a track must be opaque before its fade starts");
        Expect(std::abs(GetDecalTrackAlpha(1.5f, 2.0f, 1.0f) - 0.5f) < 1e-5f, "a track must fade linearly");
        Expect(GetDecalTrackAlpha(3.0f, 2.0f, 1.0f) == 0.0f, "a track must be gone after its duration");
        Expect(GetDecalTrackAlpha(1.0f, 1.0f, 2.0f) == 1.0f, "a fade after the duration must not hide the track");
    }

    void CheckSort()
    {
        std::vector<DecalDrawData> decals(4);
        decals[0].SortOrder = 2;
        decals[1].SortOrder = -1;
        decals[2].SortOrder = 2;
        decals[3].SortOrder = 0;
        decals[0].Opacity = 0.1f;
        decals[2].Opacity = 0.2f;
        SortDecals(decals);
        Expect(decals[0].SortOrder == -1 && decals[1].SortOrder == 0 && decals[3].SortOrder == 2, "lower sort orders must draw first");
        Expect(decals[2].Opacity == 0.1f && decals[3].Opacity == 0.2f, "equal sort orders must keep their collected order");

        std::vector<DecalDrawData> many(MaxDecalsPerView + 3);
        many.back().SortOrder = -5;
        SortDecals(many);
        Expect(many.size() == MaxDecalsPerView && many.front().SortOrder == -5, "the view limit must drop the last decals in draw order");
    }
}

int main()
{
    CheckBoxWinding();
    CheckProjection();
    CheckFades();
    CheckSort();
    return g_Failures == 0 ? 0 : 1;
}
