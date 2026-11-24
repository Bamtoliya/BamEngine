#pragma once

#include "Types.h"
#include <bit>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Engine
{
	// 모션 벡터 패스가 뷰마다 그리는 물체의 한도. 인스턴스 링 버퍼를 다른 패스와 함께 쓰므로 넘친 물체는 카메라 이동만 반영한다.
	inline constexpr uint32 MaxMotionStaticInstances = 16384u;
	inline constexpr uint32 MaxMotionSkinnedDraws = 128u;
	inline constexpr uint32 MaxMotionTerrainDraws = 16u;

	// 월드 행렬 밖에서 물체의 형상(정점, 배치 인스턴스, 높이맵)이 바뀌었는지 비교하는 값. 값이 같으면 같은 형상으로 본다.
	inline uint64 CombineMotionStamp(uint64 seed, uint64 value)
	{
		return seed ^ (value + 0x9E3779B97F4A7C15ull + (seed << 6u) + (seed >> 2u));
	}

	// 형상의 주인(메시, 텍스처 등)과 그 형상의 버전
	inline uint64 MakeMotionStamp(const void* source, uint64 version)
	{
		return CombineMotionStamp(CombineMotionStamp(0ull, static_cast<uint64>(reinterpret_cast<std::uintptr_t>(source))), version);
	}

	// 높이맵 지형. 높이 배열이 바뀌면 지형은 그 프레임을 그리지 않으므로, 여기서는 텍스처와 크기만 비교한다.
	inline uint64 MakeHeightfieldMotionStamp(const void* heightmap, f32 maxHeight, f32 cellSpacingX, f32 cellSpacingZ)
	{
		uint64 stamp = MakeMotionStamp(heightmap, 0ull);
		stamp = CombineMotionStamp(stamp, std::bit_cast<uint32>(maxHeight));
		stamp = CombineMotionStamp(stamp, std::bit_cast<uint32>(cellSpacingX));
		return CombineMotionStamp(stamp, std::bit_cast<uint32>(cellSpacingZ));
	}

	// 뷰 하나가 마지막으로 그린 물체들의 변환. 뷰마다 그리는 주기가 달라 뷰별로 따로 둔다.
	class MotionHistory
	{
	public:
		struct Pose
		{
			mat4 World{ 1.0f };
			std::vector<mat4> Bones; // 정적 메시는 비어 있다.
			uint64 Stamp = 0; // 모양 표식(MakeMotionStamp). 다르면 정점이 바뀐 것이라 변환만으로 이동량을 구할 수 없다.
		};

		void Begin()
		{
			++m_Frame;
			m_RecordedCount = 0;
		}

		// 이번 프레임의 변환을 기록한다. 한 물체의 여러 서브메시는 같은 변환·모양 표식으로 여러 번 불러도 된다.
		void Record(uint64 id, const mat4& world, const std::vector<mat4>* bones = nullptr, uint64 stamp = 0)
		{
			Entry& entry = m_Entries[id];
			if (entry.Frame == m_Frame)
			{
				// 한 번호가 서로 다른 변환으로 그려지면 어느 쪽의 이전 변환인지 알 수 없다.
				if (entry.Source != bones || entry.Current.World != world || entry.Current.Stamp != stamp) entry.Ambiguous = true;
				return;
			}
			std::swap(entry.Previous, entry.Current);
			entry.HasPrevious = entry.Frame != 0u && entry.Frame + 1u == m_Frame && !entry.Ambiguous;
			entry.Current.World = world;
			entry.Current.Stamp = stamp;
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
			if (entry.Previous.Bones.size() != entry.Current.Bones.size() || entry.Previous.Stamp != entry.Current.Stamp) return nullptr;
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
