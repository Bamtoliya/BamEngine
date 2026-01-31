#pragma once
#include "Base.h"

BEGIN(Engine)

struct Name
{
	string Text;
	Name(const char* _text) : Text(_text) {}
	Name(const string& _text) : Text(_text) {}
	Name(const wstring& _text) : Text(WStrToStr(_text)) {}
};

struct Tooltip
{
	string Text;
	Tooltip(const char* _text) : Text(_text) {}
	Tooltip(const string& _text) : Text(_text) {}
	Tooltip(const wstring& _text) : Text(WStrToStr(_text)) {}
};

struct Range
{
	f32 Min;
	f32 Max;
	Range(f32 _min, f32 _max) : Min(_min), Max(_max) {}
};

struct ReadOnly
{
	bool Value;
	ReadOnly(bool _value = true) : Value(_value) {}
};

struct Color
{
	vec4 RGBA;
	Color(const vec4& _color) : RGBA(_color) {}
};

struct Flags
{
	vector<pair<string, uint32>> Items;
	Flags(initializer_list<pair<string, uint32>> _items)
	{
		for (auto& item : _items)
		{
			Items.push_back(item);
		}
	}

	Flags(initializer_list<pair<wstring, uint32>> _items)
	{
		for (auto& item : _items)
		{
			Items.push_back({WStrToStr(item.first), item.second});
		}
	}
};


END