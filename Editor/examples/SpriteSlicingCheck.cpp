#include "SpriteSlicingService.h"
#include <iostream>
#include <cassert>
#include <vector>

using namespace Editor;

int main()
{
	std::cout << "[SpriteSlicingCheck] Starting 2D Sprite Slicing Tests..." << std::endl;

	// 1. GridByCellSize Test
	{
		SpriteSliceParameters params;
		assert(params.pixelPerUnit == 100.0f);
		params.method = ESpriteSliceMethod::GridByCellSize;
		params.cellSizeX = 32;
		params.cellSizeY = 32;
		params.offsetX = 0;
		params.offsetY = 0;
		params.paddingX = 0;
		params.paddingY = 0;

		auto slices = SpriteSlicingService::SliceGrid(64, 64, params, "Player");
		assert(slices.size() == 4);
		assert(slices[0].region.left == 0.0f && slices[0].region.top == 0.0f && slices[0].region.width == 32.0f && slices[0].region.height == 32.0f);
		assert(slices[1].region.left == 32.0f && slices[1].region.top == 0.0f && slices[1].region.width == 32.0f && slices[1].region.height == 32.0f);
		assert(slices[2].region.left == 0.0f && slices[2].region.top == 32.0f && slices[2].region.width == 32.0f && slices[2].region.height == 32.0f);
		assert(slices[3].region.left == 32.0f && slices[3].region.top == 32.0f && slices[3].region.width == 32.0f && slices[3].region.height == 32.0f);
		assert(slices[0].name == "Player_00");
		assert(slices[3].name == "Player_03");
		std::cout << "[PASS] GridByCellSize 64x64 -> 4 slices verified." << std::endl;
	}

	// 2. GridByCellCount Test
	{
		SpriteSliceParameters params;
		params.method = ESpriteSliceMethod::GridByCellCount;
		params.cellCountCols = 4;
		params.cellCountRows = 2;
		params.offsetX = 0;
		params.offsetY = 0;
		params.paddingX = 0;
		params.paddingY = 0;

		auto slices = SpriteSlicingService::SliceGrid(128, 64, params, "Effect");
		assert(slices.size() == 8);
		for (const auto& s : slices)
		{
			assert(s.region.width == 32.0f);
			assert(s.region.height == 32.0f);
		}
		assert(slices[7].name == "Effect_07");
		std::cout << "[PASS] GridByCellCount 128x64 (4x2) -> 8 slices (32x32 each) verified." << std::endl;
	}

	// 3. Automatic Alpha Clustering BFS Test
	{
		const int W = 16;
		const int H = 16;
		std::vector<uint8> pixels(W * H * 4, 0);

		// Island 1: Rect [2..5] x [2..6] -> width 4, height 5 (20 pixels)
		for (int y = 2; y <= 6; ++y)
		{
			for (int x = 2; x <= 5; ++x)
			{
				pixels[(y * W + x) * 4 + 3] = 255; // Alpha
			}
		}

		// Island 2: Rect [10..14] x [8..12] -> width 5, height 5 (25 pixels)
		for (int y = 8; y <= 12; ++y)
		{
			for (int x = 10; x <= 14; ++x)
			{
				pixels[(y * W + x) * 4 + 3] = 200; // Alpha
			}
		}

		// Noise pixel: (0, 15) -> 1 pixel only (should be filtered out by minPixelArea=4)
		pixels[(15 * W + 0) * 4 + 3] = 255;

		SpriteSliceParameters params;
		params.method = ESpriteSliceMethod::Automatic;
		params.alphaThreshold = 10;
		params.minPixelArea = 4;

		auto slices = SpriteSlicingService::SliceFromMemory(pixels.data(), W, H, params, "Monster");
		assert(slices.size() == 2);

		// Slice 0 check (Island 1)
		assert(slices[0].region.left == 2.0f);
		assert(slices[0].region.top == 2.0f);
		assert(slices[0].region.width == 4.0f);
		assert(slices[0].region.height == 5.0f);
		assert(slices[0].name == "Monster_00");

		// Slice 1 check (Island 2)
		assert(slices[1].region.left == 10.0f);
		assert(slices[1].region.top == 8.0f);
		assert(slices[1].region.width == 5.0f);
		assert(slices[1].region.height == 5.0f);
		assert(slices[1].name == "Monster_01");

		std::cout << "[PASS] Automatic Alpha Clustering -> 2 islands detected, noise filtered out." << std::endl;
	}

	std::cout << "[SpriteSlicingCheck] All tests passed successfully!" << std::endl;
	return 0;
}
