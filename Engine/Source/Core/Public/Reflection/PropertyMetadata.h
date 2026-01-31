#pragma once
#include "Base.h"

enum class EUIWidget
{
	Default,
	Slider,
	Drag,
	Color,
	Directory,
	BitFlag,
};

struct PropertyMetadata
{
	string DisplayName;
	string Tooltip;

	EUIWidget WidgetType = { EUIWidget::Default };

	f32 Min = { 0.0f };
	f32 Max = { 0.0f };
	f32 Speed = { 0.1f };
	bool ReadOnly = { false };

	vector<pair<string, uint32>> BitFlags;

	PropertyMetadata() = default;
};