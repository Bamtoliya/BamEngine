#pragma once

#include "Types.h"
#include <unordered_map>
#include <utility>
#include <vector>

namespace Engine
{
	// 뷰 하나가 마지막으로 그린 물체들의 변환. 뷰마다 그리는 주기가 달라 뷰별로 따로 둔다.
	class MotionHistory
	{
	public:
		struct Pose
		{
			mat4 World{ 1.0f };
			std::vector<mat4> Bones; // 정적 메시는 비어 있다.
		};

		void Begin()
		{
			++m_Frame;
			m_RecordedCount = 0;
		}

		// 이번 프레임의 변환을 기록한다. 한 물체의 여러 서브메시는 같은 변환으로 여러 번 불러도 된다.
		void Record(uint64 id, const mat4& world, const std::vector<mat4>* bones = nullptr)
		{
			Entry& entry = m_Entries[id];
			if (entry.Frame == m_Frame)
			{
				// 한 번호가 서로 다른 변환으로 그려지면 어느 쪽의 이전 변환인지 알 수 없다.
				if (entry.Source != bones || entry.Current.World != world) entry.Ambiguous = true;
				return;
			}
			std::swap(entry.Previous, entry.Current);
			entry.HasPrevious = entry.Frame != 0u && entry.Frame + 1u == m_Frame && !entry.Ambiguous;
			entry.Current.World = world;
			if (bones) entry.Current.Bones.assign(bones->begin(), bones->end());
			else entry.Current.Bones.clear();
			entry.Source = bones;
			entry.Ambiguous = false;
			entry.Frame = m_Frame;
			++m_RecordedCount;
		}

		// 직전 프레임에도 그려졌고 그 사이 움직인 물체의 이전 변환. 없으면 nullptr.
		const Pose* FindMoved(uint64 id) const
		{
			const auto found = m_Entries.find(id);
			if (found == m_Entries.end()) return nullptr;
			const Entry& entry = found->second;
			if (entry.Frame != m_Frame || !entry.HasPrevious || entry.Ambiguous) return nullptr;
			if (entry.Previous.Bones.size() != entry.Current.Bones.size()) return nullptr;
			const bool moved = entry.Previous.World != entry.Current.World || entry.Previous.Bones != entry.Current.Bones;
			return moved ? &entry.Previous : nullptr;
		}

		// 더 그리지 않는 물체가 쌓이면 지운다.
		void End()
		{
			if (m_Entries.size() > m_RecordedCount * 2u + 1024u)
				std::erase_if(m_Entries, [this](const auto& item) { return item.second.Frame != m_Frame; });
		}

		void Reset() { m_Entries.clear(); }
		size_t GetEntryCount() const { return m_Entries.size(); }

	private:
		struct Entry
		{
			Pose Current;
			Pose Previous;
			const std::vector<mat4>* Source = nullptr;
			uint32 Frame = 0;
			bool HasPrevious = false;
			bool Ambiguous = false;
		};

		std::unordered_map<uint64, Entry> m_Entries;
		size_t m_RecordedCount = 0;
		uint32 m_Frame = 0;
	};
}
