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

struct Category
{
	string Text;
	Category(const char* _text) : Text(_text) {}
	Category(const string& _text) : Text(_text) {}
	Category(const wstring& _text) : Text(WStrToStr(_text)) {}
};

struct FilePath
{
	string Filter;
	FilePath(const char* _filter = "") : Filter(_filter) {}
	FilePath(const string& _filter = "") : Filter(_filter) {}
	FilePath(const wstring& _filter = L"") : Filter(WStrToStr(_filter)) {}
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
	f32 Speed;
	Range(f32 _min, f32 _max) : Min(_min), Max(_max), Speed(1.f) {}
	Range(f32 _min, f32 _max, f32 _speed = 1.f) : Min(_min), Max(_max), Speed(_speed) {}
};

struct ReadOnly
{
	bool bEnable;
	ReadOnly(bool enable = true) : bEnable(enable) {}
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

struct Directory
{
	string Path;
	Directory() = default;
	Directory(const char* _path) : Path(_path) {}
	Directory(const string& _path) : Path(_path) {}
	Directory(const wstring& _path) : Path(WStrToStr(_path)) {}
};

struct Default
{
	std::any Value;
	template<typename T>
	Default(T&& _value) : Value(_value) {}
};


END