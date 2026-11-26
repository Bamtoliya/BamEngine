#pragma once

#include "Engine_Includes.h"
#include <span>
#include <unordered_map>
#include <vector>

namespace Engine
{
	// ── 드래그 박스 선택 후보 ──
	struct ENGINE_API DragBoxCandidate
	{
		uint64 id = 0;
		vec3 worldPosition{ 0.0f };
	};

	// ── 화면 드래그 박스 다중 선택기 ──
	// UIProjection::WorldToScreen을 이용해 카메라 앞쪽(isInFront)에 있고
	// 정규화된 화면 사각형 내부에 들어오는 후보 ID 목록을 반환한다.
	class ENGINE_API DragBoxSelector
	{
	public:
		[[nodiscard]] static std::vector<uint64> SelectInScreenRect(
			std::span<const DragBoxCandidate> candidates,
			const mat4& viewProjMatrix,
			const vec2& viewportSize,
			const vec2& rectMin,
			const vec2& rectMax);
	};

	// ── 미니맵 좌표 투영 유틸리티 ──
	// 월드 XZ 영역(worldMin, worldMax)과 미니맵 픽셀 크기(minimapSize) 간의
	// UV/픽셀 양방향 변환을 제공한다.
	class ENGINE_API MinimapProjection
	{
	public:
		[[nodiscard]] static vec2 WorldToMinimapUV(
			const vec2& worldXZ,
			const vec2& worldMin,
			const vec2& worldMax,
			bool clampToBounds = true) noexcept;

		[[nodiscard]] static vec2 WorldToMinimapPixel(
			const vec2& worldXZ,
			const vec2& worldMin,
			const vec2& worldMax,
			const vec2& minimapSize,
			bool clampToBounds = true) noexcept;

		[[nodiscard]] static vec2 MinimapPixelToWorldXZ(
			const vec2& minimapPixel,
			const vec2& worldMin,
			const vec2& worldMax,
			const vec2& minimapSize,
			bool clampToBounds = true) noexcept;
	};

	// ── 인벤토리 슬롯 ──
	struct ENGINE_API InventorySlot
	{
		uint64 itemId = 0;
		uint32 count = 0;

		[[nodiscard]] bool IsEmpty() const noexcept { return count == 0; }
	};

	// ── 고정/가변 슬롯 기반 인벤토리 컨테이너 ──
	// fixedSlotCount > 0이면 고정 슬롯, 0이면 가변 슬롯으로 동작한다.
	class ENGINE_API InventoryContainer
	{
	public:
		explicit InventoryContainer(size_t fixedSlotCount = 0) noexcept;

		// 실제로 추가된 수량을 반환한다.
		uint32 AddItem(uint64 itemId, uint32 count, uint32 maxStack = 99);
		// 실제로 제거된 수량을 반환한다.
		uint32 RemoveItem(uint64 itemId, uint32 count);

		[[nodiscard]] uint32 GetItemCount(uint64 itemId) const noexcept;
		[[nodiscard]] const std::vector<InventorySlot>& GetSlots() const noexcept { return m_Slots; }
		[[nodiscard]] bool IsFixedSize() const noexcept { return m_IsFixedSize; }
		[[nodiscard]] size_t GetCapacity() const noexcept { return m_Slots.size(); }

		void Clear() noexcept;

	private:
		std::vector<InventorySlot> m_Slots;
		bool m_IsFixedSize = false;
	};

	// ── 퀘스트 상태 ──
	enum class QuestState : uint8
	{
		NotStarted = 0,
		Active,
		Completed,
		Failed
	};

	// ── 퀘스트 진행도 ──
	struct ENGINE_API QuestProgress
	{
		QuestState state = QuestState::NotStarted;
		uint32 currentCount = 0;
		uint32 targetCount = 0;

		[[nodiscard]] f32 GetNormalizedProgress() const noexcept
		{
			if (targetCount == 0)
			{
				return (state == QuestState::Completed) ? 1.0f : 0.0f;
			}
			return std::clamp(static_cast<f32>(currentCount) / static_cast<f32>(targetCount), 0.0f, 1.0f);
		}
	};

	// ── 경량 퀘스트 진행 추적기 ──
	class ENGINE_API QuestTracker
	{
	public:
		bool StartQuest(uint64 questId, uint32 targetCount = 1);
		bool AdvanceQuest(uint64 questId, uint32 delta = 1);
		bool FailQuest(uint64 questId);

		[[nodiscard]] QuestState GetQuestState(uint64 questId) const noexcept;
		[[nodiscard]] QuestProgress GetProgress(uint64 questId) const noexcept;

		void Clear() noexcept;

	private:
		std::unordered_map<uint64, QuestProgress> m_Quests;
	};
}
