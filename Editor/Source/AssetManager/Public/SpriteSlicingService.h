#pragma once

#include "Engine_Includes.h"
#include "Rect.h"

BEGIN(Editor)

struct SpriteSliceInfo
{
	std::string name;                              // 슬라이스 식별자 (예: "Player_0")
	Engine::Rect region = { 0.f, 0.f, 0.f, 0.f };     // 픽셀 영역 (left, top, width, height)
	vec2 pivot = { 0.5f, 0.5f };                   // 피벗 (0.0 ~ 1.0)
	vec4 border = { 0.f, 0.f, 0.f, 0.f };          // 9-Slice 인셋 (L, R, T, B)
};

enum class ESpriteSliceMethod : uint8
{
	GridByCellSize,
	GridByCellCount,
	Automatic
};

struct SpriteSliceParameters
{
	ESpriteSliceMethod method = ESpriteSliceMethod::GridByCellSize;
	int cellSizeX = 32;
	int cellSizeY = 32;
	int cellCountCols = 4;
	int cellCountRows = 4;
	int offsetX = 0;
	int offsetY = 0;
	int paddingX = 0;
	int paddingY = 0;
	uint8 alphaThreshold = 1;     // Automatic용 알파 임계값 (0~255)
	int minPixelArea = 4;         // Automatic용 최소 면적 필터 (노이즈 방지)
	vec2 defaultPivot = { 0.5f, 0.5f };
	float pixelPerUnit = 100.0f;
};

class SpriteSlicingService
{
public:
	// RGBA8 CPU 픽셀 버퍼로부터 슬라이싱 수행
	static std::vector<SpriteSliceInfo> SliceFromMemory(
		const uint8* rgbaPixels,
		int width,
		int height,
		const SpriteSliceParameters& params,
		const std::string& baseName = "Sprite");

	// 디스크의 이미지 파일(.png 등)로부터 슬라이싱 수행
	static std::vector<SpriteSliceInfo> SliceFromFile(
		const std::filesystem::path& imagePath,
		const SpriteSliceParameters& params,
		const std::string& baseName = "Sprite");

	// 격자/치수만으로 슬라이싱 수행 (GridByCellSize, GridByCellCount 전용 - 픽셀 데이터 불필요)
	static std::vector<SpriteSliceInfo> SliceGrid(
		int width,
		int height,
		const SpriteSliceParameters& params,
		const std::string& baseName = "Sprite");
};

END
