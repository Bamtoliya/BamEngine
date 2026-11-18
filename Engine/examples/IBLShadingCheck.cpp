#include <cassert>
#include <iostream>
#include <vector>
#include <cmath>
#include <algorithm>
#include <glm/glm.hpp>

#include "Types.h"
#include "IBLManager.h"
#include "Texture.h"
#include "SkyAtmosphereComponent.h"

using namespace Engine;

// ==============================================================================
// [Test 1] BRDF Split-Sum Numerical Integration & Energy Conservation
// ==============================================================================
void CheckBRDFIntegrationAndEnergyConservation()
{
	std::cout << "[Test 1] BRDF Split-Sum Numerical Integration & Energy Conservation" << std::endl;

	// 경계 조건 1: 완전 매끄러운 표면(Roughness = 0) & 수직 시선(NdotV = 1.0)
	// 입사광이 완벽하게 스펙큘러 반사되므로 scale은 1.0에 근접하고 bias는 0에 근접해야 함
	vec2 smoothPerpendicular = IBLManager::IntegrateBRDF(1.0f, 0.0f, 1024);
	assert(smoothPerpendicular.x >= 0.98f && smoothPerpendicular.x <= 1.01f);
	assert(smoothPerpendicular.y >= 0.0f && smoothPerpendicular.y <= 0.02f);

	// 경계 조건 2: 극단적 경사 시선(Grazing Angle, NdotV -> 0.05) & Roughness = 0.0
	// 프레넬 효과에 의해 비스듬할수록 스펙큘러 반사가 강해짐
	vec2 smoothGrazing = IBLManager::IntegrateBRDF(0.05f, 0.0f, 1024);
	assert(smoothGrazing.x >= 0.0f && smoothGrazing.x <= 1.01f);
	assert(smoothGrazing.y >= 0.0f && smoothGrazing.y <= 1.01f);

	// 물리적 에너지 보존 법칙(Energy Conservation) 검증:
	// 임의의 완전 반사체(F0 = 1.0)에 대해서도 스펙큘러 반사율 F0 * scale + bias <= 1.0 이어야 에너지가 증폭되지 않음.
	constexpr int ndotvSteps = 10;
	constexpr int roughnessSteps = 10;
	for (int i = 1; i <= ndotvSteps; ++i)
	{
		float NdotV = static_cast<float>(i) / static_cast<float>(ndotvSteps);
		for (int j = 0; j <= roughnessSteps; ++j)
		{
			float roughness = static_cast<float>(j) / static_cast<float>(roughnessSteps);
			vec2 brdf = IBLManager::IntegrateBRDF(NdotV, roughness, 512);

			assert(brdf.x >= 0.0f && brdf.x <= 1.001f);
			assert(brdf.y >= 0.0f && brdf.y <= 1.001f);

			float totalSpecular = brdf.x + brdf.y; // F0 = 1.0 일 때의 최대 반사율
			assert(totalSpecular <= 1.001f && "PBR Energy Conservation Violated (total reflection > 1.0)!");
		}
	}

	std::cout << "  -> BRDF numerical integration and physical energy conservation assertions passed." << std::endl;
}

// ==============================================================================
// [Test 2] DefaultBRDFLUT Texture Generation & Specifications
// ==============================================================================
void CheckDefaultBRDFLUTTexture()
{
	std::cout << "[Test 2] DefaultBRDFLUT Texture Generation & Specifications" << std::endl;

	IBLManager& iblMgr = IBLManager::Get();
	Texture* lutTex = iblMgr.GetDefaultBRDFLUT();
	assert(lutTex != nullptr && "Failed to generate DefaultBRDFLUT texture!");

	// 멱등성 및 캐싱 검증 (재호출 시 동일 인스턴스 반환)
	Texture* cachedTex = iblMgr.GetDefaultBRDFLUT();
	assert(lutTex == cachedTex && "DefaultBRDFLUT must be cached as a singleton instance!");

	// 텍스처 규격 단언 (256x256, R16G16_FLOAT, Texture2D, 256KB)
	const TextureBinaryHeader& header = lutTex->GetCachedHeader();
	assert(header.width == 256);
	assert(header.height == 256);
	assert(header.depth == 1);
	assert(header.mipLevels == 1);
	assert(header.dimension == ETextureDimension::Texture2D);
	assert(header.format == ETextureFormat::R16G16_FLOAT);
	assert(header.dataSize == 256 * 256 * sizeof(uint32)); // 256 * 256 * 4B = 262,144 Bytes (256 KB)

	const std::vector<uint8>& rawData = lutTex->GetTempData();
	assert(rawData.size() == header.dataSize);

	std::cout << "  -> DefaultBRDFLUT texture specifications (256x256 R16G16_FLOAT, 256KB) verified." << std::endl;
}

// ==============================================================================
// [Test 3] Hemispheric Sky IBL & Shadow Ambient Floor Shading Simulation
// ==============================================================================
void CheckHemisphericSkyIBLAndShadowFloor()
{
	std::cout << "[Test 3] Hemispheric Sky IBL & Shadow Ambient Floor Shading Simulation" << std::endl;

	// SkyAtmosphereComponent의 기본 앰비언트 및 하늘색 프로퍼티 검증
	SkyAtmosphereComponent skyComp;
	assert(skyComp.ShadowAmbientFloor >= 0.05f && skyComp.ShadowAmbientFloor <= 0.30f);
	assert(skyComp.ZenithColor.b > skyComp.ZenithColor.r); // 하늘색은 푸른색 성분이 우세함

	const vec3 zenithColor = skyComp.ZenithColor;
	const float skyIntensity = skyComp.SkyIntensity;
	const float shadowAmbientFloor = skyComp.ShadowAmbientFloor;

	// HLSL lightingPS.hlsl 시뮬레이션:
	// 반구형 하늘 IBL 계산식:
	// float hemiFactor = saturate(N.z * 0.5 + 0.5);
	// float3 hemiSky = lerp(gLightingData.SkyZenithColor * 0.2, gLightingData.SkyZenithColor, hemiFactor) * gLightingData.SkyIntensity;
	auto computeHemisphericSky = [&](const vec3& N) -> vec3
	{
		float hemiFactor = glm::clamp(N.z * 0.5f + 0.5f, 0.0f, 1.0f);
		vec3 groundColor = zenithColor * 0.2f;
		return glm::mix(groundColor, zenithColor, hemiFactor) * skyIntensity;
	};

	vec3 skyUp = computeHemisphericSky(vec3(0.0f, 0.0f, 1.0f));
	vec3 skyHorizon = computeHemisphericSky(vec3(1.0f, 0.0f, 0.0f));
	vec3 skyDown = computeHemisphericSky(vec3(0.0f, 0.0f, -1.0f));

	// 상향 법선은 하늘 천정색에 완전히 일치해야 함
	assert(glm::distance(skyUp, zenithColor * skyIntensity) < 0.0001f);
	// 하향 법선은 지면 반사광(0.2배)이어야 함
	assert(glm::distance(skyDown, zenithColor * 0.2f * skyIntensity) < 0.0001f);
	// 지평선은 중간 밝기여야 함
	assert(skyHorizon.b > skyDown.b && skyHorizon.b < skyUp.b);

	// 그림자 음영부 밸런싱 검증 (직사광 완전 차폐: shadowFactor = 0.0)
	// float ambientFloor = max(gLightingData.ShadowAmbientFloor, 0.02f);
	// float3 ambient = (diffuseIBL + hemiSky * 0.3) * albedo;
	// ambient = max(ambient, albedo * ambientFloor);
	const vec3 albedo = vec3(0.8f, 0.4f, 0.2f); // 임의의 붉은 계열 재질
	const float ambientFloor = std::max(shadowAmbientFloor, 0.02f);

	vec3 diffuseIBL(0.0f); // 큐브맵 텍스처가 없는 기본 씬 (HasIBLMaps = 0)
	vec3 ambient = (diffuseIBL + skyUp * 0.3f) * albedo;
	ambient = glm::max(ambient, albedo * ambientFloor);

	// 직사광이 0일 때도 최소 ambientFloor에 의해 물체의 알베도 색상이 온전히 보존되는지 검증
	assert(ambient.x >= albedo.x * ambientFloor - 0.0001f);
	assert(ambient.y >= albedo.y * ambientFloor - 0.0001f);
	assert(ambient.z >= albedo.z * ambientFloor - 0.0001f);
	assert(ambient.x > 0.0f && ambient.y > 0.0f && ambient.z > 0.0f && "Shadow region must not be pitch black!");

	std::cout << "  -> Hemispheric sky IBL and shadow ambient floor (pitch-black prevention) verified." << std::endl;
}

// ==============================================================================
// Main Entry Point
// ==============================================================================
int main()
{
	std::cout << "=== Running IBLShadingCheck ===" << std::endl;

	CheckBRDFIntegrationAndEnergyConservation();
	CheckDefaultBRDFLUTTexture();
	CheckHemisphericSkyIBLAndShadowFloor();

	// 리소스 정리
	IBLManager::Get().Free();

	std::cout << "=== All IBLShadingCheck Tests Passed Successfully! ===" << std::endl;
	return 0;
}
