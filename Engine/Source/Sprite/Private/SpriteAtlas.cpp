#include "SpriteAtlas.h"
#include <algorithm>
#include <limits>

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
