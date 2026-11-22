#include "SpriteAtlas.h"
#include "ResourceManager.h"
#include <algorithm>
#include <limits>
#include <cstring>

namespace Engine
{
	struct BinRect
	{
		int32 x = 0;
		int32 y = 0;
		int32 width = 0;
		int32 height = 0;

		bool Contains(const BinRect& other) const
		{
			return other.x >= x && other.y >= y &&
			       other.x + other.width <= x + width &&
			       other.y + other.height <= y + height;
		}

		bool Overlaps(const BinRect& other) const
		{
			return x < other.x + other.width && x + width > other.x &&
			       y < other.y + other.height && y + height > other.y;
		}
	};

	bool SpriteAtlas::PackMaxRects(
		int32 atlasWidth, int32 atlasHeight, int32 padding,
		const std::vector<AtlasInputItem>& items,
		std::vector<AtlasEntry>& outEntries)
	{
		outEntries.clear();
		if (atlasWidth <= 0 || atlasHeight <= 0 || items.empty())
			return false;

		// 1. 항목 인덱스를 면적 내림차순(Best Area)으로 정렬하여 배치 성공률 극대화
		std::vector<size_t> sortedIndices(items.size());
		for (size_t i = 0; i < items.size(); ++i) sortedIndices[i] = i;

		std::sort(sortedIndices.begin(), sortedIndices.end(), [&](size_t a, size_t b)
		{
			const int32 areaA = items[a].width * items[a].height;
			const int32 areaB = items[b].width * items[b].height;
			return areaA > areaB;
		});

		// 2. 초기 단일 빈 캔버스 사각형 생성
		std::vector<BinRect> freeRects;
		freeRects.push_back(BinRect{ 0, 0, atlasWidth, atlasHeight });

		outEntries.resize(items.size());

		for (size_t idx : sortedIndices)
		{
			const auto& item = items[idx];
			const int32 paddedW = item.width + padding * 2;
			const int32 paddedH = item.height + padding * 2;

			int32 bestShortSideFit = std::numeric_limits<int32>::max();
			int32 bestLongSideFit = std::numeric_limits<int32>::max();
			int32 bestRectIndex = -1;

			// BSSF (Best Short Side Fit) 휴리스틱으로 최적 위치 탐색
			for (size_t f = 0; f < freeRects.size(); ++f)
			{
				const auto& fr = freeRects[f];
				if (fr.width >= paddedW && fr.height >= paddedH)
				{
					const int32 remX = fr.width - paddedW;
					const int32 remY = fr.height - paddedH;
					const int32 shortSide = std::min(remX, remY);
					const int32 longSide = std::max(remX, remY);

					if (shortSide < bestShortSideFit || (shortSide == bestShortSideFit && longSide < bestLongSideFit))
					{
						bestShortSideFit = shortSide;
						bestLongSideFit = longSide;
						bestRectIndex = static_cast<int32>(f);
					}
				}
			}

			// 공간 부족으로 패킹 실패
			if (bestRectIndex < 0)
				return false;

			const BinRect targetFree = freeRects[bestRectIndex];
			BinRect placedNode;
			placedNode.x = targetFree.x;
			placedNode.y = targetFree.y;
			placedNode.width = paddedW;
			placedNode.height = paddedH;

			// 패킹된 엔트리 기록 (실제 스프라이트 영역은 패딩 안쪽)
			AtlasEntry entry;
			entry.name = item.name;
			entry.pixelRegion = Rect(
				static_cast<f32>(placedNode.x + padding),
				static_cast<f32>(placedNode.y + padding),
				static_cast<f32>(item.width),
				static_cast<f32>(item.height));

			entry.uvTransform = vec4(
				entry.pixelRegion.left / static_cast<f32>(atlasWidth),
				entry.pixelRegion.top / static_cast<f32>(atlasHeight),
				entry.pixelRegion.width / static_cast<f32>(atlasWidth),
				entry.pixelRegion.height / static_cast<f32>(atlasHeight));

			outEntries[idx] = entry;

			// 3. 배치된 사각형과 겹치는 여유 사각형들을 분할
			const size_t prevCount = freeRects.size();
			for (size_t f = 0; f < prevCount; ++f)
			{
				if (!freeRects[f].Overlaps(placedNode)) continue;

				const BinRect fr = freeRects[f];

				// 새로운 직사각형 4방향 분할
				if (placedNode.x < fr.x + fr.width && placedNode.x + placedNode.width > fr.x)
				{
					// 위쪽 분할
					if (placedNode.y > fr.y && placedNode.y < fr.y + fr.height)
					{
						BinRect newNode = fr;
						newNode.height = placedNode.y - newNode.y;
						freeRects.push_back(newNode);
					}

					// 아래쪽 분할
					if (placedNode.y + placedNode.height < fr.y + fr.height)
					{
						BinRect newNode = fr;
						newNode.y = placedNode.y + placedNode.height;
						newNode.height = fr.y + fr.height - (placedNode.y + placedNode.height);
						freeRects.push_back(newNode);
					}
				}

				if (placedNode.y < fr.y + fr.height && placedNode.y + placedNode.height > fr.y)
				{
					// 왼쪽 분할
					if (placedNode.x > fr.x && placedNode.x < fr.x + fr.width)
					{
						BinRect newNode = fr;
						newNode.width = placedNode.x - newNode.x;
						freeRects.push_back(newNode);
					}

					// 오른쪽 분할
					if (placedNode.x + placedNode.width < fr.x + fr.width)
					{
						BinRect newNode = fr;
						newNode.x = placedNode.x + placedNode.width;
						newNode.width = fr.x + fr.width - (placedNode.x + placedNode.width);
						freeRects.push_back(newNode);
					}
				}

				// 기존 겹친 사각형 무효화
				freeRects.erase(freeRects.begin() + f);
				--f;
			}

			// 4. 중복 사각형 가지치기 (Pruning)
			for (size_t i = 0; i < freeRects.size(); ++i)
			{
				for (size_t j = i + 1; j < freeRects.size(); ++j)
				{
					if (freeRects[j].Contains(freeRects[i]))
					{
						freeRects.erase(freeRects.begin() + i);
						--i;
						break;
					}
					if (freeRects[i].Contains(freeRects[j]))
					{
						freeRects.erase(freeRects.begin() + j);
						--j;
					}
				}
			}
		}

		return true;
	}

	bool SpriteAtlas::BakeAtlasPixels(
		int32 atlasWidth, int32 atlasHeight, int32 padding,
		const std::vector<AtlasInputItem>& items,
		const std::vector<AtlasEntry>& entries,
		std::vector<uint8>& outRgbaBuffer)
	{
		outRgbaBuffer.clear();
		if (atlasWidth <= 0 || atlasHeight <= 0 || items.empty())
			return false;

		const size_t totalBytes = static_cast<size_t>(atlasWidth) * static_cast<size_t>(atlasHeight) * 4;
		outRgbaBuffer.assign(totalBytes, 0);

		// 이름 기반 엔트리 매핑
		std::unordered_map<std::string, const AtlasEntry*> entryMap;
		for (const auto& entry : entries)
		{
			entryMap[entry.name] = &entry;
		}

		for (const auto& item : items)
		{
			if (!item.rgbaPixels || item.width <= 0 || item.height <= 0)
				continue;

			auto it = entryMap.find(item.name);
			if (it == entryMap.end() || !it->second)
				continue;

			const auto& entry = *it->second;
			const int32 dstX = static_cast<int32>(entry.pixelRegion.left);
			const int32 dstY = static_cast<int32>(entry.pixelRegion.top);
			const int32 w = item.width;
			const int32 h = item.height;

			if (dstX < 0 || dstY < 0 || dstX + w > atlasWidth || dstY + h > atlasHeight)
				continue;

			// 본체 픽셀 복사
			for (int32 row = 0; row < h; ++row)
			{
				const uint8* srcRow = item.rgbaPixels + static_cast<size_t>(row) * static_cast<size_t>(w) * 4;
				uint8* dstRow = outRgbaBuffer.data() + (static_cast<size_t>(dstY + row) * static_cast<size_t>(atlasWidth) + static_cast<size_t>(dstX)) * 4;
				std::memcpy(dstRow, srcRow, static_cast<size_t>(w) * 4);
			}

			// 패딩 영역 처리: 가장자리 픽셀 복제로 번짐 방지
			if (padding > 0)
			{
				// 상단 패딩
				for (int32 p = 1; p <= padding; ++p)
				{
					const int32 py = dstY - p;
					if (py >= 0)
					{
						const uint8* srcRow = outRgbaBuffer.data() + (static_cast<size_t>(dstY) * static_cast<size_t>(atlasWidth) + static_cast<size_t>(dstX)) * 4;
						uint8* dstRow = outRgbaBuffer.data() + (static_cast<size_t>(py) * static_cast<size_t>(atlasWidth) + static_cast<size_t>(dstX)) * 4;
						std::memcpy(dstRow, srcRow, static_cast<size_t>(w) * 4);
					}
				}
				// 하단 패딩
				for (int32 p = 0; p < padding; ++p)
				{
					const int32 py = dstY + h + p;
					if (py < atlasHeight)
					{
						const uint8* srcRow = outRgbaBuffer.data() + (static_cast<size_t>(dstY + h - 1) * static_cast<size_t>(atlasWidth) + static_cast<size_t>(dstX)) * 4;
						uint8* dstRow = outRgbaBuffer.data() + (static_cast<size_t>(py) * static_cast<size_t>(atlasWidth) + static_cast<size_t>(dstX)) * 4;
						std::memcpy(dstRow, srcRow, static_cast<size_t>(w) * 4);
					}
				}
				// 좌측 및 우측 패딩 (모서리 포함)
				const int32 startY = std::max(0, dstY - padding);
				const int32 endY = std::min(atlasHeight - 1, dstY + h + padding - 1);
				for (int32 y = startY; y <= endY; ++y)
				{
					const uint8* leftPixel = outRgbaBuffer.data() + (static_cast<size_t>(y) * static_cast<size_t>(atlasWidth) + static_cast<size_t>(dstX)) * 4;
					for (int32 p = 1; p <= padding; ++p)
					{
						const int32 px = dstX - p;
						if (px >= 0)
						{
							uint8* dst = outRgbaBuffer.data() + (static_cast<size_t>(y) * static_cast<size_t>(atlasWidth) + static_cast<size_t>(px)) * 4;
							std::memcpy(dst, leftPixel, 4);
						}
					}

					const uint8* rightPixel = outRgbaBuffer.data() + (static_cast<size_t>(y) * static_cast<size_t>(atlasWidth) + static_cast<size_t>(dstX + w - 1)) * 4;
					for (int32 p = 0; p < padding; ++p)
					{
						const int32 px = dstX + w + p;
						if (px < atlasWidth)
						{
							uint8* dst = outRgbaBuffer.data() + (static_cast<size_t>(y) * static_cast<size_t>(atlasWidth) + static_cast<size_t>(px)) * 4;
							std::memcpy(dst, rightPixel, 4);
						}
					}
				}
			}
		}

		return true;
	}

	bool SpriteAtlas::BakeAtlasPixels(const std::vector<AtlasInputItem>& items, std::vector<uint8>& outRgbaBuffer) const
	{
		return BakeAtlasPixels(m_Width, m_Height, m_Padding, items, m_Entries, outRgbaBuffer);
	}

	Texture* SpriteAtlas::CreateTextureFromBakedPixels(const std::vector<uint8>& rgbaPixels, const std::wstring& textureKey)
	{
		if (rgbaPixels.size() != static_cast<size_t>(m_Width) * static_cast<size_t>(m_Height) * 4)
			return nullptr;

		TextureBinaryHeader header{};
		header.width = static_cast<uint32>(m_Width);
		header.height = static_cast<uint32>(m_Height);
		header.depth = 1;
		header.mipLevels = 1;
		header.arraySize = 1;
		header.format = ETextureFormat::R8G8B8A8_UNORM;
		header.dimension = ETextureDimension::Texture2D;
		header.dataSize = static_cast<uint32>(rgbaPixels.size());

		return Texture::Create(header, rgbaPixels, textureKey);
	}

	bool SpriteAtlas::CreateAndAssignTexture(const std::vector<AtlasInputItem>& items, const std::wstring& textureKey)
	{
		if (m_BakedPixels.empty())
		{
			if (!BakeAtlasPixels(items, m_BakedPixels))
				return false;
		}

		const std::wstring effectiveKey = textureKey.empty()
			? (L"SpriteAtlas_" + std::to_wstring(reinterpret_cast<uintptr_t>(this)))
			: textureKey;

		Texture* tex = CreateTextureFromBakedPixels(m_BakedPixels, effectiveKey);
		if (!tex)
			return false;

		if (ResourceManager::IsCreated())
		{
			m_Texture = ResourceManager::Get().AddResource<Texture>(effectiveKey, tex);
		}
		IncreaseVersion();
		return true;
	}

	EResult SpriteAtlas::Initialize(void* arg)
	{
		if (arg)
		{
			const auto* desc = static_cast<DESC*>(arg);
			m_Width = desc->width;
			m_Height = desc->height;
			m_Padding = desc->padding;

			if (!desc->items.empty())
			{
				if (!PackMaxRects(m_Width, m_Height, m_Padding, desc->items, m_Entries))
				{
					return EResult::Fail;
				}

				m_EntryLookup.clear();
				for (size_t i = 0; i < m_Entries.size(); ++i)
				{
					m_EntryLookup[m_Entries[i].name] = i;
				}

				bool hasPixels = false;
				for (const auto& item : desc->items)
				{
					if (item.rgbaPixels)
					{
						hasPixels = true;
						break;
					}
				}

				if (hasPixels)
				{
					CreateAndAssignTexture(desc->items);
				}
			}
		}

		return EResult::Success;
	}

	SpriteAtlas* SpriteAtlas::Create(void* arg)
	{
		auto* instance = new SpriteAtlas();
		if (IsFailure(instance->Initialize(arg)))
		{
			instance->Free();
			delete instance;
			return nullptr;
		}
		return instance;
	}

	void SpriteAtlas::Free()
	{
		m_Entries.clear();
		m_EntryLookup.clear();
		m_Texture = {};
		m_BakedPixels.clear();
	}

	bool SpriteAtlas::TryGetEntry(const std::string& name, AtlasEntry& outEntry) const
	{
		auto it = m_EntryLookup.find(name);
		if (it != m_EntryLookup.end() && it->second < m_Entries.size())
		{
			outEntry = m_Entries[it->second];
			return true;
		}
		return false;
	}

	bool SpriteAtlas::HasEntry(const std::string& name) const
	{
		return m_EntryLookup.find(name) != m_EntryLookup.end();
	}
}
