#pragma once

#include "Editor_Includes.h"
#include <commdlg.h>
#pragma comment(lib, "comdlg32.lib")

BEGIN(Editor)

struct FileDialogFilter
{
	std::wstring Name;
	std::wstring Pattern;
};

class FileDialogs
{
public:
	static bool SaveFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters = {}, wstring_view defaultFileName = L"", wstring_view defaultExt = L"");
	static bool OpenFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters = {}, wstring_view initialDir = L"");
private:
	static vector<wchar> FilterBuffer(const vector<FileDialogFilter>& filters);
};
END