#include "StrategyRpgHelpers.h"
#include "UIProjection.h"

#include <algorithm>
#include <cmath>

namespace Engine
{
	namespace StrategyRpgDetail
	{
		constexpr f32 kMinSpanEpsilon = 1e-6f;

		bool IsFiniteVec2(const vec2& v) noexcept
		{
			return std::isfinite(v.x) && std::isfinite(v.y);
		}

		bool TryNormalizeBounds(const vec2& worldMin, const vec2& worldMax, vec2& outMin, vec2& outSpan) noexcept
		{
			if (!IsFiniteVec2(worldMin) || !IsFiniteVec2(worldMax))
			{
				return false;
			}

			outMin = vec2(std::min(worldMin.x, worldMax.x), std::min(worldMin.y, worldMax.y));
			const vec2 maxBound(std::max(worldMin.x, worldMax.x), std::max(worldMin.y, worldMax.y));
			outSpan = maxBound - outMin;

			return outSpan.x > kMinSpanEpsilon && outSpan.y > kMinSpanEpsilon;
		}
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// DragBoxSelector
	// ═══════════════════════════════════════════════════════════════════════════

	std::vector<uint64> DragBoxSelector::SelectInScreenRect(
		std::span<const DragBoxCandidate> candidates,
		const mat4& viewProjMatrix,
		const vec2& viewportSize,
		const vec2& rectMin,
		const vec2& rectMax)
	{
		std::vector<uint64> selectedIds;
		if (candidates.empty())
		{
			return selectedIds;
		}

		if (!StrategyRpgDetail::IsFiniteVec2(viewportSize) || viewportSize.x <= 0.0f || viewportSize.y <= 0.0f)
		{
			return selectedIds;
		}

		if (!StrategyRpgDetail::IsFiniteVec2(rectMin) || !StrategyRpgDetail::IsFiniteVec2(rectMax))
		{
			return selectedIds;
		}

		const vec2 minCorner(std::min(rectMin.x, rectMax.x), std::min(rectMin.y, rectMax.y));
		const vec2 maxCorner(std::max(rectMin.x, rectMax.x), std::max(rectMin.y, rectMax.y));

		for (const DragBoxCandidate& candidate : candidates)
		{
			const UIWorldToScreenResult projected =
				UIProjection::WorldToScreen(candidate.worldPosition, viewProjMatrix, viewportSize);

			if (!projected.isInFront)
			{
				continue;
			}

			const vec2& pos = projected.screenPosition;
			if (pos.x >= minCorner.x && pos.x <= maxCorner.x &&
				pos.y >= minCorner.y && pos.y <= maxCorner.y)
			{
				selectedIds.push_back(candidate.id);
			}
		}

		return selectedIds;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// MinimapProjection
	// ═══════════════════════════════════════════════════════════════════════════

	vec2 MinimapProjection::WorldToMinimapUV(
		const vec2& worldXZ,
		const vec2& worldMin,
		const vec2& worldMax,
		bool clampToBounds) noexcept
	{
		if (!StrategyRpgDetail::IsFiniteVec2(worldXZ))
		{
			return vec2(0.0f);
		}

		vec2 minBound{};
		vec2 span{};
		if (!StrategyRpgDetail::TryNormalizeBounds(worldMin, worldMax, minBound, span))
		{
			return vec2(0.0f);
		}

		vec2 uv = (worldXZ - minBound) / span;
		if (clampToBounds)
		{
			uv.x = std::clamp(uv.x, 0.0f, 1.0f);
			uv.y = std::clamp(uv.y, 0.0f, 1.0f);
		}

		return uv;
	}

	vec2 MinimapProjection::WorldToMinimapPixel(
		const vec2& worldXZ,
		const vec2& worldMin,
		const vec2& worldMax,
		const vec2& minimapSize,
		bool clampToBounds) noexcept
	{
		if (!StrategyRpgDetail::IsFiniteVec2(minimapSize) || minimapSize.x <= 0.0f || minimapSize.y <= 0.0f)
		{
			return vec2(0.0f);
		}

		const vec2 uv = WorldToMinimapUV(worldXZ, worldMin, worldMax, clampToBounds);
		return uv * minimapSize;
	}

	vec2 MinimapProjection::MinimapPixelToWorldXZ(
		const vec2& minimapPixel,
		const vec2& worldMin,
		const vec2& worldMax,
		const vec2& minimapSize,
		bool clampToBounds) noexcept
	{
		if (!StrategyRpgDetail::IsFiniteVec2(minimapPixel) ||
			!StrategyRpgDetail::IsFiniteVec2(minimapSize) ||
			minimapSize.x <= 0.0f || minimapSize.y <= 0.0f)
		{
			return vec2(0.0f);
		}

		vec2 minBound{};
		vec2 span{};
		if (!StrategyRpgDetail::TryNormalizeBounds(worldMin, worldMax, minBound, span))
		{
			return vec2(0.0f);
		}

		vec2 uv = minimapPixel / minimapSize;
		if (clampToBounds)
		{
			uv.x = std::clamp(uv.x, 0.0f, 1.0f);
			uv.y = std::clamp(uv.y, 0.0f, 1.0f);
		}

		return minBound + uv * span;
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// InventoryContainer
	// ═══════════════════════════════════════════════════════════════════════════

	InventoryContainer::InventoryContainer(size_t fixedSlotCount) noexcept
		: m_IsFixedSize(fixedSlotCount > 0)
	{
		if (m_IsFixedSize)
		{
			m_Slots.resize(fixedSlotCount);
		}
	}

	uint32 InventoryContainer::AddItem(uint64 itemId, uint32 count, uint32 maxStack)
	{
		if (count == 0 || maxStack == 0)
		{
			return 0;
		}

		uint32 remaining = count;

		// 1단계: 기존 동일 아이템 스택 채우기
		for (InventorySlot& slot : m_Slots)
		{
			if (slot.IsEmpty() || slot.itemId != itemId || slot.count >= maxStack)
			{
				continue;
			}

			const uint32 space = maxStack - slot.count;
			const uint32 toAdd = std::min(remaining, space);
			slot.count += toAdd;
			remaining -= toAdd;
			if (remaining == 0)
			{
				return count;
			}
		}

		// 2단계: 빈 슬롯 채우기
		for (InventorySlot& slot : m_Slots)
		{
			if (!slot.IsEmpty())
			{
				continue;
			}

			const uint32 toAdd = std::min(remaining, maxStack);
			slot.itemId = itemId;
			slot.count = toAdd;
			remaining -= toAdd;
			if (remaining == 0)
			{
				return count;
			}
		}

		// 3단계: 가변 슬롯인 경우 새 슬롯 추가
		if (!m_IsFixedSize)
		{
			while (remaining > 0)
			{
				const uint32 toAdd = std::min(remaining, maxStack);
				m_Slots.push_back(InventorySlot{ itemId, toAdd });
				remaining -= toAdd;
			}
		}

		return count - remaining;
	}

	uint32 InventoryContainer::RemoveItem(uint64 itemId, uint32 count)
	{
		if (count == 0)
		{
			return 0;
		}

		uint32 remaining = count;
		for (InventorySlot& slot : m_Slots)
		{
			if (slot.IsEmpty() || slot.itemId != itemId)
			{
				continue;
			}

			const uint32 toRemove = std::min(remaining, slot.count);
			slot.count -= toRemove;
			remaining -= toRemove;

			if (slot.count == 0)
			{
				slot.itemId = 0;
			}

			if (remaining == 0)
			{
				break;
			}
		}

		if (!m_IsFixedSize)
		{
			std::erase_if(m_Slots, [](const InventorySlot& slot) { return slot.IsEmpty(); });
		}

		return count - remaining;
	}

	uint32 InventoryContainer::GetItemCount(uint64 itemId) const noexcept
	{
		uint32 total = 0;
		for (const InventorySlot& slot : m_Slots)
		{
			if (!slot.IsEmpty() && slot.itemId == itemId)
			{
				total += slot.count;
			}
		}
		return total;
	}

	void InventoryContainer::Clear() noexcept
	{
		if (m_IsFixedSize)
		{
			std::fill(m_Slots.begin(), m_Slots.end(), InventorySlot{});
		}
		else
		{
			m_Slots.clear();
		}
	}

	// ═══════════════════════════════════════════════════════════════════════════
	// QuestTracker
	// ═══════════════════════════════════════════════════════════════════════════

	bool QuestTracker::StartQuest(uint64 questId, uint32 targetCount)
	{
		if (m_Quests.contains(questId))
		{
			return false;
		}

		QuestProgress progress{};
		progress.targetCount = targetCount;
		progress.currentCount = 0;
		progress.state = (targetCount == 0) ? QuestState::Completed : QuestState::Active;

		m_Quests.emplace(questId, progress);
		return true;
	}

	bool QuestTracker::AdvanceQuest(uint64 questId, uint32 delta)
	{
		if (delta == 0)
		{
			return false;
		}

		const auto it = m_Quests.find(questId);
		if (it == m_Quests.end() || it->second.state != QuestState::Active)
		{
			return false;
		}

		QuestProgress& progress = it->second;
		const uint32 remaining = progress.targetCount - progress.currentCount;
		progress.currentCount += std::min(delta, remaining);

		if (progress.currentCount >= progress.targetCount)
		{
			progress.state = QuestState::Completed;
		}

		return true;
	}

	bool QuestTracker::FailQuest(uint64 questId)
	{
		const auto it = m_Quests.find(questId);
		if (it == m_Quests.end() || it->second.state != QuestState::Active)
		{
			return false;
		}

		it->second.state = QuestState::Failed;
		return true;
	}

	QuestState QuestTracker::GetQuestState(uint64 questId) const noexcept
	{
		const auto it = m_Quests.find(questId);
		if (it == m_Quests.end())
		{
			return QuestState::NotStarted;
		}
		return it->second.state;
	}

	QuestProgress QuestTracker::GetProgress(uint64 questId) const noexcept
	{
		const auto it = m_Quests.find(questId);
		if (it == m_Quests.end())
		{
			return QuestProgress{};
		}
		return it->second;
	}

	void QuestTracker::Clear() noexcept
	{
		m_Quests.clear();
	}
}
