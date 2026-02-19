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
	string Category;

	bool bHasRange = { false };
	f32 Min = { 0.0f };
	f32 Max = { 0.0f };
	f32 Speed = { 0.1f };

	bool bIsReadOnly = { false };

	bool bIsFilePath = { false };
	bool bIsDirectory = { false };
	string FileFilter;
	string DialogPath;

	vector<pair<string, uint32>> BitFlags;

	std::any DefaultValue;
	bool bHasDefault = { false };

	string EditCondition;

	bool bEditConditionBit = { false };
	string EditCondtionBit;
	uint64 EditConditionMask = { 0 };
	bool bEditConditionExact = { false };

	PropertyMetadata() = default;
};