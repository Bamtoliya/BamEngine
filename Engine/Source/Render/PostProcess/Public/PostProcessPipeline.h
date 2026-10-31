#pragma once

#include "PostProcessInterface.h"
#include <archive/ArchiveInterface.h>

BEGIN(Engine)

class ENGINE_API PostProcessPipeline
{
public:
	static constexpr uint32 MAX_PP_PASSES = 16;

public:
	PostProcessPipeline() = default;
	~PostProcessPipeline();

public:
	void Initialize(const wstring& prefix);
	void Free();

	void PrepareResources(uint32 width, uint32 height);
	EResult Resize(uint32 width, uint32 height);

	wstring Execute(f32 dt, const wstring& inputRT);
	void AddEffect(IPostProcessEffect* effect);

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

private:
	wstring m_Prefix;
	vector<IPostProcessEffect*> m_Effects;
	vector<RenderPassID> m_PassIDs;
	wstring m_PPRTNames[2];

	uint32 m_Width = 0;
	uint32 m_Height = 0;
};

END