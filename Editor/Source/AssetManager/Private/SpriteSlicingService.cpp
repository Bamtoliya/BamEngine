#include "SpriteSlicingService.h"
#include <stb_image.h>
#include <fstream>
#include <algorithm>
#include <vector>
#include <cmath>

BEGIN(Editor)

namespace
{
	std::string CleanBaseName(const std::string& rawName)
	{
		std::string result = rawName;
		const std::string badTag = ".bamsprite";
		size_t pos = result.rfind(badTag);
		if (pos != std::string::npos && pos + badTag.size() == result.size())
			result = result.substr(0, pos);
		return result;
	}

	std::string FormatSliceName(const std::string& baseName, int index, int totalCount)
	{
		int digits = 2;
		if (totalCount >= 1000) digits = 4;
		else if (totalCount >= 100) digits = 3;

		char buf[32];
		snprintf(buf, sizeof(buf), "%0*d", digits, index);
		return CleanBaseName(baseName) + "_" + buf;
	}
}
std::vector<SpriteSliceInfo> SpriteSlicingService::SliceGrid(
	int width,
	int height,
	const SpriteSliceParameters& params,
	const std::string& baseName)
{
	std::vector<SpriteSliceInfo> slices;
	if (width <= 0 || height <= 0) return slices;

	if (params.method == ESpriteSliceMethod::GridByCellSize)
	{
		const int cellW = (std::max)(1, params.cellSizeX);
		const int cellH = (std::max)(1, params.cellSizeY);
		const int padX = (std::max)(0, params.paddingX);
		const int padY = (std::max)(0, params.paddingY);

		for (int y = params.offsetY; y + cellH <= height; y += cellH + padY)
		{
			for (int x = params.offsetX; x + cellW <= width; x += cellW + padX)
			{
				SpriteSliceInfo slice;
				slice.region = { static_cast<float>(x), static_cast<float>(y), static_cast<float>(cellW), static_cast<float>(cellH) };
				slice.pivot = params.defaultPivot;
				slices.push_back(slice);
			}
		}
	}
	else if (params.method == ESpriteSliceMethod::GridByCellCount)
	{
		const int cols = (std::max)(1, params.cellCountCols);
		const int rows = (std::max)(1, params.cellCountRows);
		const int padX = (std::max)(0, params.paddingX);
		const int padY = (std::max)(0, params.paddingY);

		const float availW = static_cast<float>(width - params.offsetX - (cols - 1) * padX);
		const float availH = static_cast<float>(height - params.offsetY - (rows - 1) * padY);
		const float cellW = (std::max)(1.0f, availW / cols);
		const float cellH = (std::max)(1.0f, availH / rows);

		for (int r = 0; r < rows; ++r)
		{
			for (int c = 0; c < cols; ++c)
			{
				const float x = params.offsetX + c * (cellW + padX);
				const float y = params.offsetY + r * (cellH + padY);

				SpriteSliceInfo slice;
				slice.region = { x, y, cellW, cellH };
				slice.pivot = params.defaultPivot;
				slices.push_back(slice);
			}
		}
	}

	// 0-패딩 형식의 정돈된 일련번호 이름 부여 (예: uv1_00, uv1_01)
	const int totalCount = static_cast<int>(slices.size());
	for (int i = 0; i < totalCount; ++i)
	{
		slices[i].name = FormatSliceName(baseName, i, totalCount);
	}

	return slices;
}

std::vector<SpriteSliceInfo> SpriteSlicingService::SliceFromMemory(
	const uint8* rgbaPixels,
	int width,
	int height,
	const SpriteSliceParameters& params,
	const std::string& baseName)
{
	if (!rgbaPixels || width <= 0 || height <= 0) return {};

	if (params.method != ESpriteSliceMethod::Automatic)
	{
		return SliceGrid(width, height, params, baseName);
	}

	// Automatic: 투명도(Alpha Threshold) 기반 연결 요소(Connected Components) 검출
	std::vector<uint8> visited(static_cast<size_t>(width) * height, 0);
	std::vector<SpriteSliceInfo> detectedSlices;

	// 8방향 이웃 오프셋 (상하좌우 및 대각선)
	static const int dx[] = { -1, 0, 1, -1, 1, -1, 0, 1 };
	static const int dy[] = { -1, -1, -1, 0, 0, 1, 1, 1 };

	std::vector<std::pair<int, int>> queue;
	queue.reserve(512);

	for (int y = 0; y < height; ++y)
	{
		for (int x = 0; x < width; ++x)
		{
			const size_t startIndex = static_cast<size_t>(y) * width + x;
			if (visited[startIndex]) continue;

			const uint8 alpha = rgbaPixels[startIndex * 4 + 3];
			if (alpha <= params.alphaThreshold) continue;

			// 새 아일랜드 발견: BFS 탐색 시작
			visited[startIndex] = 1;
			queue.clear();
			queue.push_back({ x, y });

			int minX = x;
			int maxX = x;
			int minY = y;
			int maxY = y;
			int pixelCount = 0;

			size_t head = 0;
			while (head < queue.size())
			{
				const auto [cx, cy] = queue[head++];
				pixelCount++;

				if (cx < minX) minX = cx;
				if (cx > maxX) maxX = cx;
				if (cy < minY) minY = cy;
				if (cy > maxY) maxY = cy;

				for (int d = 0; d < 8; ++d)
				{
					const int nx = cx + dx[d];
					const int ny = cy + dy[d];

					if (nx >= 0 && nx < width && ny >= 0 && ny < height)
					{
						const size_t nIdx = static_cast<size_t>(ny) * width + nx;
						if (!visited[nIdx])
						{
							visited[nIdx] = 1;
							if (rgbaPixels[nIdx * 4 + 3] > params.alphaThreshold)
							{
								queue.push_back({ nx, ny });
							}
						}
					}
				}
			}

			// 노이즈 필터: 최소 픽셀 면적 이상일 때만 등록
			if (pixelCount >= (std::max)(1, params.minPixelArea))
			{
				SpriteSliceInfo slice;
				slice.region = {
					static_cast<float>(minX),
					static_cast<float>(minY),
					static_cast<float>(maxX - minX + 1),
					static_cast<float>(maxY - minY + 1)
				};
				slice.pivot = params.defaultPivot;
				detectedSlices.push_back(slice);
			}
		}
	}

	// 슬라이스 시각적 정렬: 상단부터 하단 순서, 같은 행(수직 오차 4px 이내)에서는 좌측부터 우측 순서
	std::sort(detectedSlices.begin(), detectedSlices.end(), [](const SpriteSliceInfo& a, const SpriteSliceInfo& b) {
		if (std::abs(a.region.top - b.region.top) > 4.0f)
			return a.region.top < b.region.top;
		return a.region.left < b.region.left;
	});

	// 0-패딩 형식의 일련번호 이름 부여
	const int totalCount = static_cast<int>(detectedSlices.size());
	for (int i = 0; i < totalCount; ++i)
	{
		detectedSlices[i].name = FormatSliceName(baseName, i, totalCount);
	}

	return detectedSlices;
}

std::vector<SpriteSliceInfo> SpriteSlicingService::SliceFromFile(
	const std::filesystem::path& imagePath,
	const SpriteSliceParameters& params,
	const std::string& baseName)
{
	if (!std::filesystem::exists(imagePath)) return {};

	std::ifstream file(imagePath, std::ios::binary | std::ios::ate);
	if (!file.is_open()) return {};

	const std::streamsize size = file.tellg();
	if (size <= 0) return {};

	file.seekg(0, std::ios::beg);
	std::vector<uint8> buffer(static_cast<size_t>(size));
	if (!file.read(reinterpret_cast<char*>(buffer.data()), size)) return {};

	int width = 0;
	int height = 0;
	int channels = 0;

	unsigned char* pixels = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, &channels, 4);
	if (!pixels) return {};

	auto result = SliceFromMemory(pixels, width, height, params, baseName);
	stbi_image_free(pixels);

	return result;
}

END
