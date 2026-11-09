#pragma once

#include <cstdint>

struct PanelLifecycleProbeViewV1
{
	std::int32_t Frame;
	std::uint32_t DrawCount;
};

#if defined(BAM_EDITOR_PANEL_PROBE_EXPORTS)
#define BAM_EDITOR_PANEL_PROBE_API __declspec(dllexport)
#else
#define BAM_EDITOR_PANEL_PROBE_API
#endif

extern "C" BAM_EDITOR_PANEL_PROBE_API void BamEditorPanelProbeDrawV1(
	std::int32_t frame, PanelLifecycleProbeViewV1* output) noexcept;

#undef BAM_EDITOR_PANEL_PROBE_API