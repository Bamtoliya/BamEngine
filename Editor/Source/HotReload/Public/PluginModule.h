#pragma once

#include <filesystem>
#if defined(_WIN32)
#include "Engine_Includes.h"
#endif

namespace Editor
{
	class PluginModule
	{
	public:
		using Symbol = void (*)();

		PluginModule() = default;
		~PluginModule();
		PluginModule(const PluginModule&) = delete;
		PluginModule& operator=(const PluginModule&) = delete;

		bool Open(const wchar_t* dllName);
		bool PrepareInstalled(const std::filesystem::path& sourceDll, const std::filesystem::path& shadowBase);
		std::string GetPreparedFingerprint() const;
		bool OpenApproved(std::string_view approvedFingerprint);
		Symbol FindSymbol(const char* name) const;
		void Swap(PluginModule& other) noexcept;
		void Close();

	private:
		bool PrepareShadowCopy(const std::filesystem::path& sourceDll, const std::filesystem::path& shadowBase, bool copyPdb);
		bool LoadPrepared(DWORD searchFlags);

	private:
#if defined(_WIN32)
		std::filesystem::path m_ShadowDll;
		HMODULE m_Handle = nullptr;
#endif
	};
}
