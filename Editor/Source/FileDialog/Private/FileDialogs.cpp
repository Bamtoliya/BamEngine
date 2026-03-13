#pragma once

#include "FileDialogs.h"

bool FileDialogs::SaveFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters, wstring_view defaultFileName, wstring_view defaultExt)
{
	wchar szFileName[MAX_PATH] = { 0 };
	if (!defaultFileName.empty())
		wcsncpy_s(szFileName, defaultFileName.data(), defaultFileName.size());

	vector<wchar> filterBuffer = FilterBuffer(filters);

	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFilter = filterBuffer.empty() ? nullptr : filterBuffer.data();
	ofn.nFilterIndex = 1;
	ofn.lpstrDefExt = defaultExt.empty() ? nullptr : defaultExt.data();
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;

	if (!GetSaveFileNameW(&ofn))
		return false;

	outPath = szFileName;
	return true;
}

bool FileDialogs::OpenFileDialog(wstring& outPath, const vector<FileDialogFilter>& filters, wstring_view initialDir)
{
	wchar szFileName[MAX_PATH] = { 0 };

	vector<wchar> filterBuffer = FilterBuffer(filters);

	OPENFILENAMEW ofn = {};
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = nullptr;
	ofn.lpstrFile = szFileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFilter = filterBuffer.empty() ? nullptr : filterBuffer.data();
	ofn.nFilterIndex = 1;
	ofn.lpstrInitialDir = initialDir.empty() ? nullptr : initialDir.data();
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

	if (!GetOpenFileNameW(&ofn))
		return false;

	outPath = szFileName;
	return true;
}

vector<wchar> FileDialogs::FilterBuffer(const vector<FileDialogFilter>& filters)
{
	vector<wchar> buffer;

	if (filters.empty())
	{
		constexpr wstring_view allFilesFilter = L"All Files (*.*)\0*.*\0";
		constexpr wstring_view pattern = L"*.*";
		buffer.insert(buffer.end(), allFilesFilter.begin(), allFilesFilter.end());
		buffer.push_back(L'\0');
		buffer.insert(buffer.end(), pattern.begin(), pattern.end());
		buffer.push_back(L'\0');
	}
	else
	{
		for (const auto& filter : filters)
		{
			buffer.insert(buffer.end(), filter.Name.begin(), filter.Name.end());
			buffer.push_back(L'\0');
			buffer.insert(buffer.end(), filter.Pattern.begin(), filter.Pattern.end());
			buffer.push_back(L'\0');
		}
	}
	buffer.push_back(L'\0');
    return buffer;
}
