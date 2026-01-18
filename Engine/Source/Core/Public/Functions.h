#pragma once

namespace Engine
{
	template<typename T>
	void Safe_Delete(T& Pointer)
	{
		if (nullptr != Pointer)
		{
			delete Pointer;
			Pointer = nullptr;
		}
	}

	template<typename T>
	void Safe_Delete_Array(T& Pointer)
	{
		if (nullptr != Pointer)
		{
			delete[] Pointer;
			Pointer = nullptr;
		}
	}

	template<typename T>
	int32 Safe_AddRef(T& Instance)
	{
		int32 iRefCnt = { 0 };

		if (nullptr != Instance)
			iRefCnt = Instance->AddRef();

		return iRefCnt;
	}

	template<typename T>
	int32 Safe_Release(T& Instance)
	{
		int32 iRefCnt = { 0 };

		if (nullptr != Instance)
		{
			iRefCnt = Instance->Release();
			if (0 == iRefCnt)
				Instance = nullptr;
		}
		return iRefCnt;
	}

	inline wstring StrToWStr(const string& str)
	{
		return filesystem::path(str).wstring();
	}

	inline string WStrToStr(const wstring& wstr)
	{
		return filesystem::path(wstr).string();
	}
}