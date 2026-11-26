#pragma once

#include "Editor_Includes.h"
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

BEGIN(Engine)
class Model;
END

BEGIN(Editor)

struct SpriteSheetBakeConfig
{
	int32 frameWidth = 64;
	int32 frameHeight = 64;
	int32 columns = 8;
	int32 padding = 2;
	int32 directions = 8;
	int32 framesPerDirection = 1;
	vec2 pivot = { 0.5f, 0.5f };
	float pixelsPerUnit = DEFAULT_PPU;
	float captureFps = 12.0f;
	float yawOffsetDegrees = 0.0f;
	std::filesystem::path outputPngPath;
	std::filesystem::path outputSpriteMetaPath;
};

struct SpriteSheetBakeResult
{
	bool succeeded = false;
	std::string errorMessage;
	int32 atlasWidth = 0;
	int32 atlasHeight = 0;
	int32 totalFrames = 0;
	std::filesystem::path outputPngPath;
	std::filesystem::path outputSpriteMetaPath;
};

class SpriteSheetBaker
{
public:
	using TurntableCaptureCallback = std::function<bool(
		int32 directionIndex,
		float yawDegrees,
		int32 frameIndex,
		int32 framesPerDirection,
		std::vector<uint8>& outRgbaFrame)>;

	// RGBA8 프레임 버퍼 목록을 지정 컬럼/패딩 규격의 단일 아틀라스 RGBA8 버퍼로 합성
	static bool AssembleAtlasRGBA(
		const std::vector<std::vector<uint8>>& frames,
		const SpriteSheetBakeConfig& config,
		std::vector<uint8>& outAtlasPixels,
		int32& outAtlasWidth,
		int32& outAtlasHeight,
		std::string* outError = nullptr);

	// 합성된 아틀라스 버퍼를 PNG와 엔진 규격 .bamsprite.json 메타데이터로 저장
	static bool WriteSpriteSheetOutputs(
		const std::vector<uint8>& atlasPixels,
		int32 atlasWidth,
		int32 atlasHeight,
		int32 frameCount,
		const SpriteSheetBakeConfig& config,
		SpriteSheetBakeResult& outResult);

	// 이미 캡처된 RGBA8 프레임 목록으로부터 아틀라스 합성 및 파일 출력까지 일괄 수행
	static SpriteSheetBakeResult BakeTurntableFromFrames(
		const std::vector<std::vector<uint8>>& frames,
		const SpriteSheetBakeConfig& config);

	// 방향별 턴테이블 캡처 콜백을 구동하여 아틀라스 합성 및 파일 출력까지 수행
	static SpriteSheetBakeResult BakeTurntableFromCapture(
		const SpriteSheetBakeConfig& config,
		const TurntableCaptureCallback& captureFrameFn);

	// ThumbnailRenderer 오프스크린 파이프라인을 통해 모델 1프레임을 렌더링하고 RGBA8 픽셀을 Readback
	static bool CaptureThumbnailFrameRGBA(
		Engine::Model* model,
		std::vector<uint8>& outRgbaPixels,
		int32& outWidth,
		int32& outHeight,
		std::string* outError = nullptr,
		f32 yawDegrees = 0.0f);
};

END
