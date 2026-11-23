#pragma once

#include "PostProcessInterface.h"
#include <archive/ArchiveInterface.h>

BEGIN(Engine)

class ENGINE_API PostProcessPipeline
{
public:
	static constexpr uint32 MAX_PP_PASSES = 16;
	// 최종 출력은 썸네일 캡처와 화면 출력이 읽으므로 8비트로 두고, 톤매핑 전 효과만 이 형식에 쓴다.
	static constexpr ETextureFormat HdrFormat = ETextureFormat::R16G16B16A16_FLOAT;

public:
	PostProcessPipeline() = default;
	~PostProcessPipeline();

public:
	void Initialize(const wstring& prefix);
	void Free();

	void PrepareResources(uint32 width, uint32 height);
	EResult Resize(uint32 width, uint32 height);

	// 장면 입력은 다음 Execute 한 번에만 쓰이고 지워진다. 매 프레임 Execute 전에 넘긴다.
	void SetSceneInputs(const PostProcessSceneInputs& inputs) { m_SceneInputs = inputs; m_HasSceneInputs = true; }
	wstring Execute(f32 dt, const wstring& inputRT);
	void AddEffect(IPostProcessEffect* effect);
	// 모든 효과의 이전 프레임 값을 버린다(자동 노출 등).
	void ResetHistory();
	// 이번 프레임에 그릴 씬. 이전 프레임과 다른 씬이면 ResetHistory를 호출한다.
	void SetHistoryScene(const void* scene);

	vector<IPostProcessEffect*>& GetEffects() { return m_Effects; }
	const vector<IPostProcessEffect*>& GetEffects() const { return m_Effects; }
	uint32 GetWidth() const { return m_Width; }
	uint32 GetHeight() const { return m_Height; }

public:
	// ── 아카이브 직렬화 / 역직렬화 (순서, 활성화 여부, 모든 파라미터 값 저장/복원) ──
	archive::ArchiveResult WriteArchive(archive::IArchive& ar) const;
	archive::ArchiveResult ReadArchive(const archive::IArchive& ar);

	bool SaveToFile(const wstring& filepath) const;
	bool LoadFromFile(const wstring& filepath);
	bool SaveToString(std::string& outJson) const;
	bool LoadFromString(std::string_view inJson);

	// 파이프라인 동기화 및 변경 감지 인터페이스
	uint64 GetVersion() const { return m_Version; }
	void NotifySettingsChanged() { ++m_Version; }
	void CopySettingsFrom(const PostProcessPipeline& other);
private:
	// 처음 쓸 때 만든다. 만들 수 없으면 빈 이름을 반환한다.
	// HDR 효과가 연달아 오면 읽는 타깃과 쓰는 타깃이 달라야 하므로 두 개를 번갈아 쓴다.
	const wstring& EnsureHdrTarget(uint32 index);

private:
	wstring m_Prefix;
	vector<IPostProcessEffect*> m_Effects;
	vector<RenderPassID> m_PassIDs;
	wstring m_PPRTNames[2];
	wstring m_HdrRTNames[2];
	const void* m_HistoryScene = nullptr;
	PostProcessSceneInputs m_SceneInputs;
	bool m_HasSceneInputs = false;

	uint32 m_Width = 0;
	uint32 m_Height = 0;
	uint64 m_Version = 1;
};

END