#pragma once

#include "Editor_Includes.h"
#include "PostProcess.h"

namespace
{
	ENUM()
		enum class EViewportChannelView : uint8
	{
		None = 0,
		R = 1 << 0,
		G = 1 << 1,
		B = 1 << 2,
		A = 1 << 3,
		RGB = R | G | B,
		RGBA = RGB | A
	};
	ENABLE_BITMASK_OPERATORS(EViewportChannelView)

		struct ChannelViewData
	{
		uint32 Flags;
		float pad[3];
	};
}

BEGIN(Editor)
class ChannelFilter
{
#pragma region Constructor& Destructor
public:
	ChannelFilter() = default;
	~ChannelFilter() = default;
public:
	void Initialize(const wstring& ownerPrefix);
	void Free();
#pragma endregion

public:
	void SetChannelView(EViewportChannelView view) { m_ChannelView = view; }
	EViewportChannelView GetChannelView() const { return m_ChannelView; }

	void EnsureChannelPreviewRT(const wstring& sourceRTName);
	void Resize(const wstring& sourceRTName);
	void SubmitChannelPreviewPass(const wstring& sourceRTName, wstring& outRTName);

	const wstring& GetChannelPreviewRTName() const { return m_ChannelPreviewRTName; }
	RenderTarget* GetChannelPreviewRT() const;
	bool IsReady() const { return (m_ChannelPreviewPassID != INVALID_PASS_ID) && (m_ChannelPreviewPipeline != nullptr); }

	
private:
	EViewportChannelView m_ChannelView = EViewportChannelView::RGBA;
	RenderPassID m_ChannelPreviewPassID = INVALID_PASS_ID;
	class RHIPipeline* m_ChannelPreviewPipeline = nullptr;
	wstring m_ChannelPreviewRTName;
};
END