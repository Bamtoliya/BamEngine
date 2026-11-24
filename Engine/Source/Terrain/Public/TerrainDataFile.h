#pragma once

#include "Engine_API.h"
#include "Types.h"
#include <entt/entity/registry.hpp>
#include <filesystem>
#include <string>
#include <utility>
#include <vector>

BEGIN(Engine)

struct TerrainComponent;

// 지형 데이터 파일(.bamterrain)에서 읽은 내용. 높이는 0~1 정규화 값이다.
struct TerrainDataContent
{
	uint32 Resolution = 0;
	std::vector<f32> Heights;
	std::vector<uint8> Splat;      // 레이어 0~3, 정점당 RGBA8
	std::vector<uint8> SplatExtra; // 레이어 4~7, 정점당 RGBA8 (쓸 때 비어 있으면 0)
	std::vector<uint8> Attributes; // 정점당 ETerrainAttribute (쓸 때 비어 있으면 0)
};

// 높이·스플랫 데이터는 씬 JSON이 아니라 이 파일이 소유한다(TerrainSystemExpansionPlan.md 3.1절).
// 형식(리틀 엔디언): 매직 "BAMTERR\0", uint32 버전, uint32 해상도(분할 수, 최대 1024), uint32 높이 형식(1 = R16_UNORM),
// uint32 스플랫 채널 수, 높이 N개 uint16(N = (해상도+1)^2), 이어서
//   v1: 채널 4 → 레이어 0~3 N*4바이트
//   v2: 채널 8 → 레이어 0~3 N*4바이트, 레이어 4~7 N*4바이트, 속성 맵 N바이트
// 쓰기는 항상 v2이고, 읽기는 v1(레이어 4~7·속성은 0)과 v2를 받는다.
class ENGINE_API TerrainDataFile
{
public:
	static constexpr uint32 Version = 2;

	// 임시 파일에 쓴 뒤 교체한다. 실패하면 기존 파일은 그대로 남는다.
	[[nodiscard]] static bool Write(const std::filesystem::path& path, const TerrainDataContent& content);
	// 매직·버전·형식·크기가 모두 맞을 때만 true다. 실패하면 out은 바뀌지 않는다.
	[[nodiscard]] static bool Read(const std::filesystem::path& path, TerrainDataContent& out);

	// 씬 파일 옆의 기본 경로: <씬 이름>_Terrain_<엔티티 ID 16진수>.bamterrain
	static std::filesystem::path MakeDefaultPath(const std::filesystem::path& scenePath, uint64 entityId);

	// 씬 저장 직전에 호출한다. 데이터가 있는 모든 지형을 쓰고, 경로가 비어 있으면 기본 경로를 정해 DataPath에 넣는다.
	// 하나라도 실패하면 false이며, 호출부는 씬 저장도 실패로 처리해야 한다(씬이 없는 파일을 가리키지 않게).
	[[nodiscard]] static bool SaveSceneTerrains(entt::registry& registry, const std::filesystem::path& scenePath);

	// RecoveryDataPath가 있으면 먼저 그 파일을 읽고(복구 직후, 편집된 상태로 표시) 비운다.
	// 그 외에는 DataPath가 바뀌었을 때 파일을 읽어 HeightData·SplatData를 채운다. 해상도가 다르면 TerrainSystem이 리샘플링한다.
	// 읽기에 실패하면 경고하고 현재 데이터를 유지한다. 같은 경로는 한 번만 시도한다.
	static void LoadIfChanged(TerrainComponent& terrain);

	// ── 저장하지 않은 편집 보존 ──
	// PIE: 시작 전 메모리 사본을 잡고, 씬 아카이브로 복원한 뒤 IDComponent 값으로 맞춰 되돌린다.
	struct MemorySnapshot
	{
		struct Entry { TerrainDataContent Content; bool IsDataDirty = false; };
		std::vector<std::pair<uint64, Entry>> Entries;
	};
	static MemorySnapshot CaptureMemory(const entt::registry& registry);
	static void RestoreMemory(entt::registry& registry, MemorySnapshot&& snapshot);

	// 복구 스냅샷: 지형 데이터를 directory에 임시 파일로 쓰고 RecoveryDataPath를 채운다. 소멸 시 RecoveryDataPath를 비운다.
	// 씬 아카이브는 이 객체가 살아 있는 동안 써야 한다. 하나라도 쓰지 못하면 Succeeded()가 false다.
	class ENGINE_API ScopedRecoveryCopies
	{
	public:
		ScopedRecoveryCopies(entt::registry& registry, const std::filesystem::path& directory, const std::wstring& prefix);
		~ScopedRecoveryCopies();
		ScopedRecoveryCopies(const ScopedRecoveryCopies&) = delete;
		ScopedRecoveryCopies& operator=(const ScopedRecoveryCopies&) = delete;
		bool Succeeded() const { return m_Succeeded; }
	private:
		entt::registry& m_Registry;
		std::vector<entt::entity> m_Entities;
		bool m_Succeeded = true;
	};
};

END
