#pragma once

#include <filesystem>
#if defined(_WIN32)
#include "Engine_Includes.h"
#endif

namespace Editor
{
	class EditorPluginModule
	{
	public:
		using Symbol = void (*)();

		EditorPluginModule() = default;
		~EditorPluginModule();
		EditorPluginModule(const EditorPluginModule&) = delete;
		EditorPluginModule& operator=(const EditorPluginModule&) = delete;

		bool Open(const wchar_t* dllName);
		Symbol FindSymbol(const char* name) const;
		void Swap(EditorPluginModule& other) noexcept;
		void Close();

	private:
#if defined(_WIN32)
		std::filesystem::path m_ShadowDll;
		HMODULE m_Handle = nullptr;
#endif
	};
}
