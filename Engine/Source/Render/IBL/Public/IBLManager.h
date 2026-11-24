#pragma once

#include "Engine_API.h"
#include "Types.h"
#include "ResourceHandle.h"
#include "Texture.h"
#include <memory>

BEGIN(Engine)

class RHI;

CLASS()
class ENGINE_API IBLManager final
{
public:
	static IBLManager& Get();

	// Split-Sum PBR용 256x256 2D BRDF LUT 텍스처 반환 (최초 호출 시 프로시저럴 생성 및 캐싱)
	Texture* GetDefaultBRDFLUT();

	// 절차적 BRDF 적분 계산 함수 (Karis 2013 중요도 샘플링, 단위 테스트 및 생성기 공용)
	static vec2 IntegrateBRDF(float NdotV, float roughness, uint32 numSamples = 1024);

	// Equirectangular HDR(RGB32F) 이미지를 DirectX LH 6면 R16G16B16A16_FLOAT 큐브맵(밉체인 포함)으로 변환한다.
	static bool ConvertEquirectangularToCubemap(const float* srcRGB, uint32 srcWidth, uint32 srcHeight,
		uint32 faceSize, bool generateMips, TextureBinaryHeader& outHeader, std::vector<uint8>& outPixels);

	// 리소스 해제
	void Free();

private:
	IBLManager() = default;
	~IBLManager() { Free(); }

	Texture* GenerateBRDFLUT(uint32 size, RHI* rhi);

private:
	ResourceHandle<Texture> m_DefaultBRDFLUTHandle;
	std::unique_ptr<Texture> m_HeadlessBRDFLUT;
};

END
