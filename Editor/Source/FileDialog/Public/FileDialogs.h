#pragma once

#include "Editor_Includes.h"
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")

BEGIN(Editor)
TODO("IMGUI 기반 파일 다이얼로그 로 변경 가능성 있음")

enum class EFileDialogContext
{
	General = 0,
	SceneOpen = 1,
	ResourceImport = 2,
	ResourceOpen = 3
};

struct FileDialogFilter
{
	std::wstring Name;
	std::wstring Pattern;
};

class FileDialogs
{
public:
	static bool SaveFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters = {}, wstring_view defaultFileName = L"", wstring_view defaultExt = L"");
	static bool OpenFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters = {}, wstring_view initialDir = L"", EFileDialogContext context = EFileDialogContext::General);
private:
	static vector<wchar> FilterBuffer(const vector<FileDialogFilter>& filters);
};
END