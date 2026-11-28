#include "TerrainUtility.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <vector>
#include "TerrainComponent.h"
#include "TerrainDataFile.h"
#include "IDComponent.h"
#include "TerrainLayerAsset.h"
#include "ReflectionRegistry.h"
#include "ResourceManager.h"
#include "Functions.h"
#include <filesystem>
#include <fstream>
#include <glm/gtc/matrix_transform.hpp>

namespace
{
	int Fail(const char* message) { std::cerr << message << '\n'; return 1; }
	bool Near(float left, float right) { return std::abs(left - right) < 0.00001f; }
	Engine::TerrainComponent MakeBrushTerrain()
	{
		Engine::TerrainComponent terrain;
		terrain.Width = 16.f;
		terrain.Length = 16.f;
		terrain.MaxHeight = 10.f;
		terrain.Resolution = 16;
		terrain.HeightData.assign(17 * 17, 0.25f);
		return terrain;
	}

	bool ChangedHeightsCovered(const std::vector<Engine::f32>& before, const Engine::TerrainComponent& terrain)
	{
		if (!terrain.IsDirty || before.size() != terrain.HeightData.size())
			return false;

		const auto axis = terrain.Resolution + 1;
		const auto& region = terrain.HeightDirtyRegion;

		if (region && (region->IsEmpty() || region->maxX > axis || region->maxY > axis))
			return false;

		bool changed = false;
		for (std::size_t index = 0; index < before.size(); ++index)
		{
			if (before[index] == terrain.HeightData[index]) continue;
			changed = true;

			const auto x = index % axis;
			const auto y = index / axis;

			if (region && (x < region->minX || x >= region->maxX ||
				y < region->minY || y >= region->maxY))
				return false;
		}

		return changed;
	}

	int CheckHeightDirtyRegions()
	{
		using namespace Engine;

		TerrainBrushParams brush;
		brush.Mode = ETerrainBrushMode::Raise;
		brush.Radius = 1.5f;
		brush.Strength = 2.f;
		const glm::mat4 world(1.f);

		// 한 번의 업로드 전에 떨어진 두 위치를 편집해도 변경이 모두 포함되어야 합니다.
		auto terrain = MakeBrushTerrain();
		const auto before = terrain.HeightData;

		if (!TerrainUtility::ApplyBrush(terrain, vec3(-5.f, 0.f, -5.f), world, brush, 0.25f) ||
			!TerrainUtility::ApplyBrush(terrain, vec3(3.f, 0.f, 4.f), world, brush, 0.25f) ||
			!terrain.HeightDirtyRegion || !ChangedHeightsCovered(before, terrain))
			return Fail("Merged brush region omitted changed heights.");

		// 전체 갱신 요청은 뒤이은 부분 편집으로 축소되면 안 됩니다.
		const auto beforeFull = terrain.HeightData;
		terrain.MarkHeightDirty();

		if (!TerrainUtility::ApplyBrush(terrain, vec3(0.f), world, brush, 0.25f) ||
			terrain.HeightDirtyRegion || !ChangedHeightsCovered(beforeFull, terrain))
			return Fail("Partial edit replaced a pending full upload.");

		// 마지막 행과 열을 포함한 네 모서리에서 최대 좌표 처리 오류를 검사합니다.
		const vec3 corners[] = {
			vec3(-8.f, 0.f, -8.f), vec3(8.f, 0.f, -8.f),
			vec3(-8.f, 0.f, 8.f), vec3(8.f, 0.f, 8.f)
		};

		for (const auto& corner : corners)
		{
			auto edgeTerrain = MakeBrushTerrain();
			const auto edgeBefore = edgeTerrain.HeightData;

			if (!TerrainUtility::ApplyBrush(edgeTerrain, corner, world, brush, 0.25f) ||
				!edgeTerrain.HeightDirtyRegion || !ChangedHeightsCovered(edgeBefore, edgeTerrain))
				return Fail("Corner brush region omitted changed heights.");
		}

		// 이미 최대 높이라 실제 변화가 없다면 업로드를 요청하지 않아야 합니다.
		auto saturated = MakeBrushTerrain();
		std::fill(saturated.HeightData.begin(), saturated.HeightData.end(), 1.f);

		if (TerrainUtility::ApplyBrush(saturated, vec3(0.f), world, brush, 0.25f) ||
			saturated.IsDirty || saturated.HeightDirtyRegion)
			return Fail("Unchanged terrain requested an upload.");

		return 0;
	}

	int CheckRectangularGridMesh()
	{
		using namespace Engine;

		const auto release = [](Mesh* mesh)
			{
				mesh->Free();
				delete mesh;
			};

		std::unique_ptr<Mesh, decltype(release)> grid(TerrainUtility::CreateGridMesh(2.0f, 3.0f, 2, 3, true), release);
		if (!grid || grid->GetVertexCount() != 12 || grid->GetIndexCount() != 36 ||
			grid->GetIndexStride() != sizeof(uint32))
			return Fail("Rectangular grid has incorrect buffer dimensions.");

		const auto& raw = grid->GetRawIndexData();
		if (raw.size() != 36 * sizeof(uint32))
			return Fail("Rectangular grid index buffer has an incorrect byte count.");

		std::array<uint32, 6> lastCell{};
		std::memcpy(lastCell.data(), raw.data() + 30 * sizeof(uint32), sizeof(lastCell));
		if (lastCell != std::array<uint32, 6>{ 7, 8, 10, 8, 11, 10 })
			return Fail("Rectangular grid row stride or reversed winding is incorrect.");

		std::unique_ptr<Mesh, decltype(release)> legacy(TerrainUtility::CreateGridMesh(2.0f, 2.0f, 2), release);
		if (!legacy || legacy->GetVertexCount() != 9 || legacy->GetIndexCount() != 24 ||
			legacy->GetRawIndexData().size() != 24 * sizeof(uint32))
			return Fail("Legacy square grid dimensions changed.");

		std::array<uint32, 6> firstCell{};
		std::memcpy(firstCell.data(), legacy->GetRawIndexData().data(), sizeof(firstCell));
		if (firstCell != std::array<uint32, 6>{ 0, 3, 1, 1, 3, 4 })
			return Fail("Legacy square grid winding changed.");

		return 0;
	}
}

// .bamterrain 저장 왕복: 한글 경로, R16 양자화 오차, 손상·불일치 입력 거부
int CheckTerrainDataFile()
{
	using namespace Engine;
	const auto folder = std::filesystem::temp_directory_path() / L"BamTerrainDataCheck";
	std::error_code ec;
	std::filesystem::remove_all(folder, ec);
	std::filesystem::create_directories(folder, ec);
	const auto path = folder / L"지형_데이터.bamterrain";

	const uint32 resolution = 4;
	const std::size_t samples = 25;
	std::vector<f32> heights(samples);
	std::vector<uint8> splat(samples * 4);
	for (std::size_t i = 0; i < samples; ++i)
	{
		heights[i] = static_cast<f32>(i) / static_cast<f32>(samples - 1);
		splat[i * 4] = static_cast<uint8>(i * 10);
		splat[i * 4 + 3] = 200;
	}
	heights[3] = 1.5f; // 범위 밖 값은 저장할 때 잘린다

	if (!TerrainDataFile::Write(path, { resolution, heights, splat })) return Fail("Terrain data write failed for a UTF-8 path.");
	if (std::filesystem::exists(std::filesystem::path(path) += L".tmp")) return Fail("Terrain data write left its temporary file.");

	TerrainDataContent content;
	if (!TerrainDataFile::Read(path, content) || content.Resolution != resolution || content.Heights.size() != samples || content.Splat != splat)
		return Fail("Terrain data round trip changed the layout or splat weights.");
	for (std::size_t i = 0; i < samples; ++i)
	{
		const f32 expected = std::clamp(heights[i], 0.0f, 1.0f);
		if (std::abs(content.Heights[i] - expected) > 1.0f / 65535.0f)
			return Fail("Terrain height round trip exceeded R16 precision.");
	}

	if (TerrainDataFile::Write(folder / L"bad.bamterrain", { resolution, std::vector<f32>(samples - 1), splat }))
		return Fail("Terrain data write accepted a height count that does not match the resolution.");
	if (TerrainDataFile::Write(folder / L"bad.bamterrain", { resolution, heights, std::vector<uint8>(samples * 3) }))
		return Fail("Terrain data write accepted a splat size that does not match the resolution.");

	// 잘린 파일과 매직이 다른 파일은 거부하고 out을 바꾸지 않는다.
	const auto truncated = folder / L"truncated.bamterrain";
	std::filesystem::copy_file(path, truncated, ec);
	std::filesystem::resize_file(truncated, std::filesystem::file_size(truncated) - 1, ec);
	const auto corrupt = folder / L"corrupt.bamterrain";
	std::filesystem::copy_file(path, corrupt, ec);
	{
		std::fstream file(corrupt, std::ios::binary | std::ios::in | std::ios::out);
		file.put('X');
	}
	TerrainDataContent untouched;
	untouched.Resolution = 99;
	if (TerrainDataFile::Read(truncated, untouched) || TerrainDataFile::Read(corrupt, untouched) ||
		TerrainDataFile::Read(folder / L"missing.bamterrain", untouched) || untouched.Resolution != 99)
		return Fail("Corrupt, truncated or missing terrain data was accepted or changed the destination.");

	const auto defaultPath = TerrainDataFile::MakeDefaultPath(folder / L"Level.bamscene.json", 0xABCull);
	if (defaultPath.filename() != L"Level_Terrain_0000000000000ABC.bamterrain")
		return Fail("Default terrain data path must use the scene name and entity ID.");

	std::filesystem::remove_all(folder, ec);
	return 0;
}

// T2-a: 8레이어·속성 맵 저장 왕복, v1 파일 호환, 해상도 상한, 이동·회전한 지형의 페인트 위치
int CheckLayersAndAttributes()
{
	using namespace Engine;
	const auto folder = std::filesystem::temp_directory_path() / L"BamTerrainLayerCheck";
	std::error_code ec;
	std::filesystem::remove_all(folder, ec);
	std::filesystem::create_directories(folder, ec);

	// 해상도 상한: 1024까지 허용, 1025는 거부
	uint32 axis = 0;
	std::size_t samples = 0;
	if (!TerrainUtility::TryGetGridLayout(TerrainComponent::MaxResolution, axis, samples) || axis != 1025 ||
		TerrainUtility::TryGetGridLayout(TerrainComponent::MaxResolution + 1, axis, samples))
		return Fail("Terrain resolution must allow 1024 divisions (1025 vertices) and reject more.");

	// v2 왕복: 레이어 4~7과 속성 맵(Blocked·Hole 포함)이 보존된다
	TerrainDataContent content{ 2, std::vector<f32>(9, 0.25f), std::vector<uint8>(36, 10), std::vector<uint8>(36, 20), std::vector<uint8>(9, 0) };
	content.Attributes[4] = static_cast<uint8>(ETerrainAttribute::Blocked);
	content.Attributes[5] = static_cast<uint8>(ETerrainAttribute::Hole);
	content.SplatExtra[7] = 99;
	TerrainDataContent read;
	if (!TerrainDataFile::Write(folder / L"v2.bamterrain", content) || !TerrainDataFile::Read(folder / L"v2.bamterrain", read) ||
		read.Splat != content.Splat || read.SplatExtra != content.SplatExtra || read.Attributes != content.Attributes)
		return Fail("Terrain data v2 must round-trip layers 4-7 and the attribute map.");

	// v1 파일(채널 4, 속성 없음)을 직접 만들어 읽는다: 레이어 4~7·속성은 0
	{
		std::ofstream v1(folder / L"v1.bamterrain", std::ios::binary);
		const char magic[8] = { 'B', 'A', 'M', 'T', 'E', 'R', 'R', '\0' };
		const uint32 fields[4] = { 1, 1, 1, 4 }; // 버전 1, 해상도 1, R16, 채널 4
		v1.write(magic, 8);
		v1.write(reinterpret_cast<const char*>(fields), sizeof(fields));
		const uint16 heights[4] = { 0, 65535, 0, 65535 };
		v1.write(reinterpret_cast<const char*>(heights), sizeof(heights));
		const uint8 splat[16] = { 255, 0, 0, 0, 0, 255, 0, 0, 255, 0, 0, 0, 0, 0, 255, 0 };
		v1.write(reinterpret_cast<const char*>(splat), sizeof(splat));
	}
	if (!TerrainDataFile::Read(folder / L"v1.bamterrain", read) || read.Resolution != 1 || read.Splat.size() != 16 || read.Splat[5] != 255 ||
		read.SplatExtra != std::vector<uint8>(16, 0) || read.Attributes != std::vector<uint8>(4, 0) || std::abs(read.Heights[1] - 1.0f) > 1e-6f)
		return Fail("Terrain data v1 files must still load with empty layers 4-7 and attributes.");

	// 페인트: 지형을 (100, 0, 50)으로 옮기고 Y축으로 90도 돌린 뒤 로컬 (+4, 0, 0) 위치에 레이어 6을 칠한다
	TerrainComponent terrain;
	terrain.Width = 16.f;
	terrain.Length = 16.f;
	terrain.Resolution = 4; // 정점 간격 4
	terrain.HeightData.assign(25, 0.f);
	const glm::mat4 world = glm::rotate(glm::translate(glm::mat4(1.0f), glm::vec3(100.f, 0.f, 50.f)), glm::radians(90.0f), glm::vec3(0.f, 1.f, 0.f));
	const glm::vec3 hit = glm::vec3(world * glm::vec4(4.f, 0.f, 0.f, 1.f));
	if (!TerrainUtility::ApplyPaint(terrain, hit, world, 6, 1.0f, 1.0f, 1.0f))
		return Fail("Painting a moved and rotated terrain must change the vertex under the cursor.");
	const std::size_t underCursor = 2 * 5 + 3; // 로컬 x=+4 → 열 3, z=0 → 행 2
	const std::size_t origin = 2 * 5 + 2;
	// 레이어 0이 255에서 시작하므로 레이어 6에 255를 더한 뒤 정규화하면 둘 다 약 128이 된다.
	if (terrain.SplatDataExtra.size() != 100 || terrain.SplatDataExtra[underCursor * 4 + 2] < 120 || terrain.SplatData[underCursor * 4] > 135 ||
		terrain.SplatDataExtra[origin * 4 + 2] != 0 || terrain.SplatData[origin * 4] != 255 || !terrain.IsDataDirty)
		return Fail("Layer 6 paint must land on the local vertex under the cursor, not the world-space position.");
	int sum = 0;
	for (int c = 0; c < 4; ++c) sum += terrain.SplatData[underCursor * 4 + c] + terrain.SplatDataExtra[underCursor * 4 + c];
	if (std::abs(sum - 255) > 4)
		return Fail("Painted layer weights across 8 layers must stay normalized to 255.");

	if (!TerrainUtility::ApplyAttributePaint(terrain, hit, world, static_cast<uint8>(ETerrainAttribute::Water), 1.0f) ||
		terrain.AttributeData.size() != 25 || terrain.AttributeData[underCursor] != static_cast<uint8>(ETerrainAttribute::Water) || terrain.AttributeData[origin] != 0 ||
		!terrain.IsAttributeDirty)
		return Fail("Attribute paint must mark only the vertices under the cursor.");
	if (TerrainUtility::ApplyAttributePaint(terrain, hit, world, static_cast<uint8>(ETerrainAttribute::Water), 1.0f))
		return Fail("Repainting the same attribute must report no change.");

	// ETerrainAttribute::Hole 페인트 및 재페인트 판정
	terrain.IsAttributeDirty = false;
	if (!TerrainUtility::ApplyAttributePaint(terrain, hit, world, static_cast<uint8>(ETerrainAttribute::Hole), 1.0f) ||
		terrain.AttributeData[underCursor] != static_cast<uint8>(ETerrainAttribute::Hole) || terrain.AttributeData[origin] != 0 ||
		!terrain.IsAttributeDirty || !terrain.IsDataDirty)
		return Fail("Hole attribute paint must mark only the vertices under the cursor and set dirty flags.");
	if (TerrainUtility::ApplyAttributePaint(terrain, hit, world, static_cast<uint8>(ETerrainAttribute::Hole), 1.0f))
		return Fail("Repainting the Hole attribute must report no change.");

	// 해상도가 바뀌면 레이어 4~7·속성 맵도 새 크기로 맞춘다
	terrain.Resolution = 8;
	TerrainUtility::EnsureLayerData(terrain);
	if (terrain.SplatData.size() != 324 || terrain.SplatDataExtra.size() != 324 || terrain.AttributeData.size() != 81)
		return Fail("Layer planes and the attribute map must follow a resolution change.");

	std::filesystem::remove_all(folder, ec);
	return 0;
}

// 지형 레이어 에셋: 설정 저장 왕복(한글 파일명), 파일의 키·경로가 등록 식별자를 덮어쓰지 않음, 표시 이름·확장자 판정
int CheckTerrainLayerAsset()
{
	using namespace Engine;
	struct Scope
	{
		bool Reflection = ReflectionRegistry::Create() != nullptr;
		bool Resources = ResourceManager::Create();
		~Scope()
		{
			if (Resources) ResourceManager::Destroy();
			if (Reflection) ReflectionRegistry::Destroy();
		}
	} scope;
	if (!scope.Reflection || !scope.Resources) return Fail("Reflection or resource manager creation failed.");

	const auto folder = std::filesystem::temp_directory_path() / L"BamTerrainLayerAssetCheck";
	std::error_code ec;
	std::filesystem::remove_all(folder, ec);
	std::filesystem::create_directories(folder, ec);
	const auto path = folder / L"암석.bamterrainlayer.json";

	TerrainLayerSettings settings;
	settings.Roughness = 0.42f;
	settings.Metallic = 0.25f;
	settings.Tiling = 3.5f;
	settings.Walkable = false;
	settings.NavCost = 7.0f;
	settings.SurfaceType = "Rock";

	auto* source = TerrainLayerAsset::CreateEmpty();
	source->SetSettings(settings);
	const bool saved = source->SaveToFile(path);
	ResourceManager::Get().DestroyResource(source);
	if (!saved) return Fail("Terrain layer asset save failed for a UTF-8 path.");

	// ResourceManager 로더와 같이 키·경로를 정해 읽는다. 파일 안의 m_Key·m_Path로 바뀌면 안 된다.
	TerrainLayerCreateDesc desc;
	desc.Key = L"Project://Layers/Rock";
	desc.Path = path.wstring();
	auto* loaded = TerrainLayerAsset::Create(&desc);
	if (!loaded) return Fail("Terrain layer asset load failed.");
	const bool sameSettings = loaded->GetSettings() == settings;
	const bool keptIdentity = loaded->GetKey() == NormalizePath(desc.Key);
	const std::string displayName = loaded->GetDisplayName();
	ResourceManager::Get().DestroyResource(loaded);
	if (!sameSettings) return Fail("Terrain layer settings must round-trip through the asset file.");
	if (!keptIdentity) return Fail("Loading a terrain layer must keep the key assigned by the loader.");
	if (displayName != Engine::Unicode::WideToUtf8(L"암석")) return Fail("Terrain layer display name must be the file name without its suffix.");

	// 구형 .bamterrainlayer.json(PBR 필드 누락) 하위 호환: ALLOW_MISSING으로 기본값(Roughness=0.85, Metallic=0.0)을 유지한다.
	const auto legacyPath = folder / L"legacy.bamterrainlayer.json";
	{
		std::ifstream savedFile(path);
		std::string jsonText((std::istreambuf_iterator<char>(savedFile)), std::istreambuf_iterator<char>());
		const std::size_t keyPos = jsonText.find("\"m_Settings\"");
		const std::size_t openBrace = keyPos != std::string::npos ? jsonText.find('{', keyPos) : std::string::npos;
		if (openBrace == std::string::npos) return Fail("Saved terrain layer JSON must contain m_Settings.");
		int depth = 0;
		std::size_t closeBrace = std::string::npos;
		for (std::size_t i = openBrace; i < jsonText.size(); ++i)
		{
			if (jsonText[i] == '{') ++depth;
			else if (jsonText[i] == '}' && --depth == 0) { closeBrace = i; break; }
		}
		if (closeBrace == std::string::npos) return Fail("Saved terrain layer JSON had malformed m_Settings.");
		jsonText.replace(openBrace, closeBrace - openBrace + 1,
			R"({"Tiling":6.0,"Walkable":true,"NavCost":1.5,"SurfaceType":"Grass"})");
		std::ofstream legacyFile(legacyPath, std::ios::trunc);
		legacyFile << jsonText;
	}
	auto* legacyLoaded = TerrainLayerAsset::CreateEmpty();
	const bool legacyOk = legacyLoaded && legacyLoaded->LoadFromFile(legacyPath);
	const TerrainLayerSettings legacySettings = legacyOk ? legacyLoaded->GetSettings() : TerrainLayerSettings{};
	if (legacyLoaded) ResourceManager::Get().DestroyResource(legacyLoaded);
	if (!legacyOk || !Near(legacySettings.Tiling, 6.0f) || !Near(legacySettings.Roughness, 0.85f) ||
		!Near(legacySettings.Metallic, 0.0f) || legacySettings.SurfaceType != "Grass")
		return Fail("Legacy terrain layer JSON without PBR fields must load with default Roughness=0.85 and Metallic=0.0.");

	if (!TerrainLayerAsset::IsTerrainLayerFile(L"A.BAMTERRAINLAYER.JSON") || TerrainLayerAsset::IsTerrainLayerFile(L"A.bamtimeline.json"))
		return Fail("Terrain layer file detection must ignore case and reject other assets.");

	std::filesystem::remove_all(folder, ec);
	return 0;
}

// 하이트맵 16비트 PNG·RAW 내보내기 → 가져오기 왕복(한글 경로), 잘못된 입력은 기존 높이를 바꾸지 않는다.
int CheckHeightmapImportExport()
{
	using namespace Engine;
	const auto folder = std::filesystem::temp_directory_path() / L"BamHeightmapCheck";
	std::error_code ec;
	std::filesystem::remove_all(folder, ec);
	std::filesystem::create_directories(folder, ec);

	TerrainComponent source;
	source.Resolution = 8;
	source.HeightData.resize(81);
	for (std::size_t i = 0; i < source.HeightData.size(); ++i)
		source.HeightData[i] = static_cast<f32>((i * 7919) % 1000) / 999.0f; // 8비트로는 구분할 수 없는 간격을 섞는다

	for (const auto* name : { L"높이맵.png", L"높이맵.r16" })
	{
		const auto path = folder / name;
		if (!TerrainUtility::SaveHeightmapToFile(source, path.wstring()))
			return Fail("Heightmap export failed for a UTF-8 path.");
		TerrainComponent imported;
		imported.Resolution = 8;
		if (!TerrainUtility::LoadHeightmapFromFile(imported, path.wstring()) || imported.HeightData.size() != source.HeightData.size() || !imported.IsDataDirty)
			return Fail("Heightmap import failed or did not mark the data edited.");
		for (std::size_t i = 0; i < source.HeightData.size(); ++i)
			if (std::abs(imported.HeightData[i] - source.HeightData[i]) > 1.0f / 65535.0f)
				return Fail("Heightmap round trip lost 16-bit precision.");
	}

	TerrainComponent untouched;
	untouched.Resolution = 8;
	untouched.HeightData.assign(81, 0.5f);
	{
		std::ofstream odd(folder / L"odd.raw", std::ios::binary);
		odd.write("abc", 3);
	}
	if (TerrainUtility::LoadHeightmapFromFile(untouched, (folder / L"odd.raw").wstring()) ||
		TerrainUtility::LoadHeightmapFromFile(untouched, (folder / L"missing.png").wstring()) ||
		untouched.HeightData != std::vector<f32>(81, 0.5f) || untouched.IsDataDirty)
		return Fail("A malformed or missing heightmap must be rejected without changing heights.");

	std::filesystem::remove_all(folder, ec);
	return 0;
}

// PIE 메모리 사본과 복구 스냅샷 임시 사본: 저장하지 않은 편집이 엔티티 ID로 돌아오고 원본 DataPath는 바뀌지 않는다.
int CheckUnsavedTerrainPreservation()
{
	using namespace Engine;
	const auto folder = std::filesystem::temp_directory_path() / L"BamTerrainRecoveryCheck";
	std::error_code ec;
	std::filesystem::remove_all(folder, ec);
	std::filesystem::create_directories(folder, ec);

	const auto makeTerrain = [](entt::registry& registry, uint64 id, f32 height) -> TerrainComponent&
		{
			const auto entity = registry.create();
			registry.emplace<IDComponent>(entity).id = id;
			auto& terrain = registry.emplace<TerrainComponent>(entity);
			terrain.Resolution = 1;
			terrain.HeightData.assign(4, height);
			terrain.SplatData.assign(16, 7);
			terrain.DataPath = "Project://Saved.bamterrain";
			terrain.LoadedDataPath = terrain.DataPath;
			terrain.IsDataDirty = true;
			return terrain;
		};

	// PIE: 원래 씬에서 사본을 잡고, 아카이브로 복원한 것처럼 데이터 없이 다시 만든 씬에 되돌린다.
	entt::registry before;
	makeTerrain(before, 42, 0.75f);
	auto snapshot = TerrainDataFile::CaptureMemory(before);
	entt::registry restored;
	auto& restoredTerrain = makeTerrain(restored, 42, 0.0f);
	restoredTerrain.HeightData.clear();
	restoredTerrain.IsDataDirty = false;
	restoredTerrain.LoadedDataPath.clear();
	TerrainDataFile::RestoreMemory(restored, std::move(snapshot));
	if (restoredTerrain.HeightData != std::vector<f32>(4, 0.75f) || !restoredTerrain.IsDataDirty || restoredTerrain.LoadedDataPath != restoredTerrain.DataPath)
		return Fail("PIE restore must bring back unsaved heights and keep the saved file from overwriting them.");

	// 복구 스냅샷: 사본을 쓰는 동안만 RecoveryDataPath가 채워진다.
	entt::registry live;
	auto& liveTerrain = makeTerrain(live, 7, 0.5f);
	std::string recoveryPath;
	{
		const TerrainDataFile::ScopedRecoveryCopies copies(live, folder, L"AutoSave_Level");
		recoveryPath = liveTerrain.RecoveryDataPath;
		if (!copies.Succeeded() || recoveryPath.empty() || !std::filesystem::exists(Utf8ToPath(recoveryPath)))
			return Fail("Recovery copies must write a temporary terrain file and record it.");
	}
	if (!liveTerrain.RecoveryDataPath.empty() || liveTerrain.DataPath != "Project://Saved.bamterrain")
		return Fail("Recovery copies must clear RecoveryDataPath afterwards and never change DataPath.");

	// 복구한 씬: 임시 사본을 읽고 편집된 상태로 표시한다.
	TerrainComponent recovered;
	recovered.Resolution = 1;
	recovered.DataPath = "Project://Saved.bamterrain";
	recovered.RecoveryDataPath = recoveryPath;
	TerrainDataFile::LoadIfChanged(recovered);
	if (recovered.HeightData.size() != 4 || std::abs(recovered.HeightData[0] - 0.5f) > 1.0f / 65535.0f ||
		!recovered.IsDataDirty || !recovered.RecoveryDataPath.empty() || recovered.LoadedDataPath != recovered.DataPath)
		return Fail("Loading a recovered terrain must use the temporary copy, mark it edited and keep DataPath.");

	std::filesystem::remove_all(folder, ec);
	return 0;
}

// 지형 확장 2단계: 청크 분할·UV 서브영역·AABB·LOD 메시·프러스텀 컬링·히스테리시스 LOD 선택·스트리밍 임계치
int CheckChunkLodAndCulling()
{
	using namespace Engine;

	// 1. 청크 분할 및 UV 서브영역 계산
	uint32 minX = 0, maxX = 0, minZ = 0, maxZ = 0;
	vec2 uvOffset(0.0f), uvScale(0.0f);
	if (!TerrainUtility::ComputeChunkRange(16, 4, 0, 0, minX, maxX, minZ, maxZ, uvOffset, uvScale) ||
		minX != 0 || maxX != 4 || minZ != 0 || maxZ != 4 ||
		!Near(uvOffset.x, 0.0f) || !Near(uvOffset.y, 0.0f) || !Near(uvScale.x, 0.25f) || !Near(uvScale.y, 0.25f))
		return Fail("Chunk (0,0) vertex range or UV sub-rect is incorrect.");

	uint32 nextMinX = 0, nextMaxX = 0, nextMinZ = 0, nextMaxZ = 0;
	if (!TerrainUtility::ComputeChunkRange(16, 4, 1, 2, nextMinX, nextMaxX, nextMinZ, nextMaxZ, uvOffset, uvScale) ||
		nextMinX != maxX || nextMaxX != 8 || nextMinZ != 8 || nextMaxZ != 12 ||
		!Near(uvOffset.x, 0.25f) || !Near(uvOffset.y, 0.5f) || !Near(uvScale.x, 0.25f) || !Near(uvScale.y, 0.25f))
		return Fail("Chunk (1,2) must share boundary with previous chunk and compute exact UV.");

	if (TerrainUtility::ComputeChunkRange(0, 4, 0, 0, minX, maxX, minZ, maxZ, uvOffset, uvScale) ||
		TerrainUtility::ComputeChunkRange(16, 0, 0, 0, minX, maxX, minZ, maxZ, uvOffset, uvScale) ||
		TerrainUtility::ComputeChunkRange(16, 4, 4, 0, minX, maxX, minZ, maxZ, uvOffset, uvScale))
		return Fail("Invalid chunk range parameters must be rejected.");

	// 2. 청크 로컬·월드 AABB 계산
	TerrainComponent terrain;
	terrain.Width = 16.0f;
	terrain.Length = 16.0f;
	terrain.MaxHeight = 20.0f;
	terrain.Resolution = 16;
	terrain.HeightData.assign(17 * 17, 0.1f);
	terrain.HeightData[2 * 17 + 2] = 0.8f; // 청크 (0,0) 내부 봉우리: Y = 16.0

	BoundingBox localBounds00;
	if (!TerrainUtility::ComputeChunkLocalBounds(terrain, 0, 4, 0, 4, localBounds00) ||
		!Near(localBounds00.min.x, -8.0f) || !Near(localBounds00.max.x, -4.0f) ||
		!Near(localBounds00.min.y, 2.0f) || !Near(localBounds00.max.y, 16.0f) ||
		!Near(localBounds00.min.z, -8.0f) || !Near(localBounds00.max.z, -4.0f))
		return Fail("Chunk (0,0) local AABB did not reflect chunk height range.");

	BoundingBox localBounds11;
	if (!TerrainUtility::ComputeChunkLocalBounds(terrain, 4, 8, 4, 8, localBounds11) ||
		!Near(localBounds11.min.y, 2.0f) || !Near(localBounds11.max.y, 2.0f))
		return Fail("Flat chunk (1,1) local AABB height must match flat height.");

	const glm::mat4 world = glm::translate(glm::mat4(1.0f), glm::vec3(100.0f, 10.0f, -50.0f));
	BoundingBox worldBounds00;
	if (!TerrainUtility::ComputeChunkWorldBounds(localBounds00, world, worldBounds00) ||
		!Near(worldBounds00.min.x, 92.0f) || !Near(worldBounds00.max.x, 96.0f) ||
		!Near(worldBounds00.min.y, 12.0f) || !Near(worldBounds00.max.y, 26.0f) ||
		!Near(worldBounds00.min.z, -58.0f) || !Near(worldBounds00.max.z, -54.0f))
		return Fail("Chunk world AABB transform is incorrect.");

	// 3. 청크별 LOD 0/1/2 메시 생성
	const auto releaseMesh = [](Mesh* mesh) { mesh->Free(); delete mesh; };
	std::unique_ptr<Mesh, decltype(releaseMesh)> lod0(TerrainUtility::CreateChunkGridMesh(16.0f, 16.0f, 16, 0, 4, 0, 4, 1), releaseMesh);
	std::unique_ptr<Mesh, decltype(releaseMesh)> lod1(TerrainUtility::CreateChunkGridMesh(16.0f, 16.0f, 16, 0, 4, 0, 4, 2), releaseMesh);
	std::unique_ptr<Mesh, decltype(releaseMesh)> lod2(TerrainUtility::CreateChunkGridMesh(16.0f, 16.0f, 16, 0, 4, 0, 4, 4), releaseMesh);
	if (!lod0 || lod0->GetVertexCount() != 25 || lod0->GetIndexCount() != 96 ||
		!lod1 || lod1->GetVertexCount() != 9 || lod1->GetIndexCount() != 24 ||
		!lod2 || lod2->GetVertexCount() != 4 || lod2->GetIndexCount() != 6)
		return Fail("Chunk LOD 0/1/2 mesh vertex or index counts are incorrect.");
	if (!Near(lod0->GetMin().x, -8.0f) || !Near(lod0->GetMax().x, -4.0f) ||
		!Near(lod2->GetMin().z, -8.0f) || !Near(lod2->GetMax().z, -4.0f))
		return Fail("Chunk LOD mesh bounds do not match chunk span.");

	// 4. 프러스텀 컬링
	const glm::mat4 view = glm::lookAtRH(glm::vec3(0.0f, 10.0f, 20.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
	const glm::mat4 proj = glm::perspectiveRH_ZO(glm::radians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
	const Frustum frustum = FrustumCuller::ExtractFrustum(proj * view);
	const BoundingBox frontBox(vec3(-4.0f, 0.0f, -4.0f), vec3(4.0f, 5.0f, 4.0f));
	const BoundingBox behindBox(vec3(-4.0f, 0.0f, 40.0f), vec3(4.0f, 5.0f, 50.0f));
	const BoundingBox sideBox(vec3(200.0f, 0.0f, -4.0f), vec3(210.0f, 5.0f, 4.0f));
	if (!TerrainUtility::IsChunkVisibleInFrustum(frustum, frontBox) ||
		TerrainUtility::IsChunkVisibleInFrustum(frustum, behindBox) ||
		TerrainUtility::IsChunkVisibleInFrustum(frustum, sideBox))
		return Fail("Chunk frustum culling misclassified front, behind, or side bounds.");

	// 5. 거리 계산 및 히스테리시스 기반 LOD 선택
	if (!Near(TerrainUtility::ComputeDistanceToBoundingBox(vec3(0.0f, 2.0f, 0.0f), frontBox), 0.0f) ||
		!Near(TerrainUtility::ComputeDistanceToBoundingBox(vec3(7.0f, 2.0f, 0.0f), frontBox), 3.0f))
		return Fail("Distance to chunk AABB is incorrect.");

	if (TerrainUtility::SelectChunkLOD(50.0f, 3u, 80.0f, 180.0f, 350.0f, 8.0f) != 0u ||
		TerrainUtility::SelectChunkLOD(120.0f, 3u, 80.0f, 180.0f, 350.0f, 8.0f) != 1u ||
		TerrainUtility::SelectChunkLOD(250.0f, 3u, 80.0f, 180.0f, 350.0f, 8.0f) != 2u)
		return Fail("Initial distance-based LOD selection is incorrect.");

	// 히스테리시스 구간 [72, 88], [172, 188]에서는 기존 LOD를 유지해야 한다.
	if (TerrainUtility::SelectChunkLOD(85.0f, 0u, 80.0f, 180.0f, 350.0f, 8.0f) != 0u ||
		TerrainUtility::SelectChunkLOD(89.0f, 0u, 80.0f, 180.0f, 350.0f, 8.0f) != 1u ||
		TerrainUtility::SelectChunkLOD(75.0f, 1u, 80.0f, 180.0f, 350.0f, 8.0f) != 1u ||
		TerrainUtility::SelectChunkLOD(71.0f, 1u, 80.0f, 180.0f, 350.0f, 8.0f) != 0u ||
		TerrainUtility::SelectChunkLOD(185.0f, 1u, 80.0f, 180.0f, 350.0f, 8.0f) != 1u ||
		TerrainUtility::SelectChunkLOD(189.0f, 1u, 80.0f, 180.0f, 350.0f, 8.0f) != 2u ||
		TerrainUtility::SelectChunkLOD(175.0f, 2u, 80.0f, 180.0f, 350.0f, 8.0f) != 2u ||
		TerrainUtility::SelectChunkLOD(171.0f, 2u, 80.0f, 180.0f, 350.0f, 8.0f) != 1u)
		return Fail("LOD hysteresis failed to prevent boundary popping.");

	// 6. 스트리밍 로드/언로드 임계치 판정
	if (!TerrainUtility::ShouldChunkBeLoaded(9999.0f, false, 0.0f, 10.0f) ||
		!TerrainUtility::ShouldChunkBeLoaded(199.0f, false, 200.0f, 10.0f) ||
		TerrainUtility::ShouldChunkBeLoaded(201.0f, false, 200.0f, 10.0f) ||
		!TerrainUtility::ShouldChunkBeLoaded(205.0f, true, 200.0f, 10.0f) ||
		TerrainUtility::ShouldChunkBeLoaded(211.0f, true, 200.0f, 10.0f))
		return Fail("Chunk streaming load/unload threshold with hysteresis is incorrect.");

	return 0;
}

int main()
{
	using namespace Engine;

	const std::vector<f32> slope{ 0.0f, 0.5f, 0.5f, 1.0f };
	std::vector<f32> result;
	if (!TerrainUtility::ResampleHeightData(slope, 1, 2, result) || result.size() != 9)
		return Fail("Heightmap enlargement failed.");

	for (uint32 z = 0; z < 3; ++z)
		for (uint32 x = 0; x < 3; ++x)
			if (!Near(result[z * 3 + x], static_cast<float>(x + z) * 0.25f))
				return Fail("Resampling changed a planar slope.");

	const auto enlarged = result;
	if (!TerrainUtility::ResampleHeightData(enlarged, 2, 1, result) || result != slope)
		return Fail("Heightmap reduction lost the corner heights.");

	const std::vector<f32> flat(4, 0.4f);
	if (!TerrainUtility::ResampleHeightData(flat, 1, 4, result) || result.size() != 25 ||
		std::any_of(result.begin(), result.end(), [](f32 value) { return !Near(value, 0.4f); }))
		return Fail("Resampling changed a constant height.");

	auto aliased = slope;
	if (!TerrainUtility::ResampleHeightData(aliased, 1, 2, aliased) || aliased != enlarged)
		return Fail("Aliased source and destination failed.");

	const std::vector<f32> sentinel{ -7.0f };
	result = sentinel;
	const auto rejected = [&](const std::vector<f32>& data, uint32 from, uint32 to)
		{
			return !TerrainUtility::ResampleHeightData(data, from, to, result) && result == sentinel;
		};

	const uint32 invalidResolution = (std::numeric_limits<uint32>::max)();
	if (!rejected({ 0.0f, 0.5f, 1.0f }, 1, 2) ||
		!rejected(slope, 0, 2) || !rejected(slope, 1, 0) ||
		!rejected(slope, invalidResolution, 2) || !rejected(slope, 1, invalidResolution))
		return Fail("Invalid dimensions were accepted or changed the destination.");

	auto invalid = slope;
	invalid[1] = std::numeric_limits<f32>::quiet_NaN();
	if (!rejected(invalid, 1, 2)) return Fail("NaN height was accepted.");
	invalid[1] = std::numeric_limits<f32>::infinity();
	if (!rejected(invalid, 1, 2)) return Fail("Infinite height was accepted.");

	if (const int checkResult = CheckHeightDirtyRegions(); checkResult != 0)
		return checkResult;

	if (const int checkResult = CheckRectangularGridMesh(); checkResult != 0)
		return checkResult;

	if (const int checkResult = CheckTerrainDataFile(); checkResult != 0)
		return checkResult;

	if (const int checkResult = CheckUnsavedTerrainPreservation(); checkResult != 0)
		return checkResult;

	if (const int checkResult = CheckHeightmapImportExport(); checkResult != 0)
		return checkResult;

	if (const int checkResult = CheckLayersAndAttributes(); checkResult != 0)
		return checkResult;

	if (const int checkResult = CheckTerrainLayerAsset(); checkResult != 0)
		return checkResult;

	if (const int checkResult = CheckChunkLodAndCulling(); checkResult != 0)
		return checkResult;

	std::cout << "Terrain data checks passed.\n";
	return 0;
}