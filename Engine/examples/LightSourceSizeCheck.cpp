#include "Light.h"
#include "LightSourceSize.h"
#include "ShadowLightAssignment.h"

#include <cmath>
#include <cstdio>
#include <limits>

using namespace Engine;

namespace
{
	int g_Failures = 0;

	void Expect(bool condition, const char* message)
	{
		if (condition) return;
		std::fprintf(stderr, "LightSourceSizeCheck failed: %s\n", message);
		++g_Failures;
	}

	bool Near(f32 a, f32 b, f32 tolerance = 1e-5f)
	{
		return std::abs(a - b) <= tolerance;
	}
}

int main()
{
	// GPU 광원 버퍼에서 빈 칸이던 자리를 쓰므로 크기와 위치가 셰이더(light.hlsli)와 같아야 한다.
	static_assert(sizeof(GPULight) == 80);
	static_assert(offsetof(GPULight, SourceSize) == 76);

	// 광원 크기: 방향광은 반각의 탄젠트, 점·스포트는 반지름
	Expect(GetLightSourceSize(ELightType::Directional, 0.0f, 1.0f) == 0.0f, "a directional light without an angle must stay sharp");
	Expect(Near(GetLightSourceSize(ELightType::Directional, 0.53f, 0.0f), std::tan(glm::radians(0.265f))), "the sun angle must become the tangent of half the angle");
	Expect(Near(GetLightSourceSize(ELightType::Directional, 90.0f, 0.0f), std::tan(glm::radians(MaxLightSourceAngle * 0.5f))), "a large angle must be clamped");
	Expect(GetLightSourceSize(ELightType::Directional, -1.0f, 0.0f) == 0.0f, "a negative angle must become sharp");
	Expect(GetLightSourceSize(ELightType::Point, 30.0f, 0.25f) == 0.25f, "a point light must use its radius, not the angle");
	Expect(GetLightSourceSize(ELightType::Spot, 0.0f, 100.0f) == MaxLightSourceRadius, "a large radius must be clamped");
	Expect(GetLightSourceSize(ELightType::Point, 0.0f, std::numeric_limits<f32>::quiet_NaN()) == 0.0f, "an invalid radius must become sharp");
	Expect(GetLightSourceSize(ELightType::Sky, 10.0f, 1.0f) == 0.0f, "a sky light has no size");

	// 반그림자 반지름
	Expect(GetPenumbraRadius(true, 0.0f, 5.0f, 0.0f) == 0.0f, "a light without size must have no penumbra");
	Expect(GetPenumbraRadius(true, 0.01f, 0.0f, 0.0f) == 0.0f, "a blocker at the receiver must have no penumbra");
	Expect(Near(GetPenumbraRadius(true, 0.01f, 4.0f, 0.0f), 0.04f), "the directional penumbra must grow with the blocker distance");
	// 반지름 0.5m 광원이 10m 위에 있고 가리는 물체가 받는 면에서 2m 위에 있으면 0.5 * 2 / 8
	Expect(Near(GetPenumbraRadius(false, 0.5f, 2.0f, 10.0f), 0.125f), "the local penumbra must follow similar triangles");
	Expect(std::isfinite(GetPenumbraRadius(false, 0.5f, 10.0f, 10.0f)), "a blocker at the light must not divide by zero");

	// 크기가 있는 광원이 채널을 받았을 때만 누적·필터한다.
	ShadowLightSet set;
	set.Lights = { { 2, true, 0.0f }, { 5, true, 0.1f }, { 7, true, 0.0f } };
	set.CascadeLight = 5;
	RaytracedShadowAssignment assignment;
	assignment.Lights = { 2, 7, INVALID_SHADOW_LIGHT, INVALID_SHADOW_LIGHT };
	Expect(!HasSoftRaytracedShadow(set, assignment), "sharp lights alone must not need denoising");
	assignment.Lights[2] = 5;
	Expect(HasSoftRaytracedShadow(set, assignment), "a traced light with size must need denoising");
	Expect(!HasSoftRaytracedShadow(set, {}), "an empty mask must not need denoising");

	if (g_Failures == 0)
		std::printf("LightSourceSizeCheck passed\n");
	return g_Failures == 0 ? 0 : 1;
}
