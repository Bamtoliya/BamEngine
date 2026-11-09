#include "EditorPluginModule.h"
#include "Editor_Macros.h"
#include "PlatformPath.h"
#include <cstdint>
#include <utility>

namespace Editor
{
	EditorPluginModule::~EditorPluginModule()
	{
		Close();
	}

	bool EditorPluginModule::Open(const wchar_t* dllName)
	{
#if defined(_WIN32)
		if (m_Handle || !m_ShadowDll.empty())
			return false;

		const auto executableDir = Engine::PlatformPath::GetExecutableDirectory();
		const auto sourceDll = executableDir / dllName;
		const auto shadowRoot = executableDir / L"PluginShadow" / sourceDll.stem() /
			std::to_wstring(GetCurrentProcessId());

		std::error_code error;
		std::filesystem::create_directories(shadowRoot, error);
		if (error)
		{
			EDITOR_LOG_ERROR("Editor plugin shadow root creation failed: {}", error.message());
			return false;
		}

		static std::uint64_t generation = 0;
		std::filesystem::path shadowDir;
		do
		{
			shadowDir = shadowRoot / std::to_wstring(++generation);
			if (std::filesystem::create_directory(shadowDir, error))
				break;
			if (error)
			{
				EDITOR_LOG_ERROR("Editor plugin shadow directory creation failed: {}", error.message());
				return false;
			}
		} while (true);

		m_ShadowDll = shadowDir / sourceDll.filename();
		m_ShadowDll.make_preferred();
		if (!std::filesystem::copy_file(sourceDll, m_ShadowDll, error))
		{
			EDITOR_LOG_ERROR("Editor plugin DLL copy failed ({}): {}", WStrToStr(sourceDll.wstring()), error.message());
			return false;
		}

#if defined(_DEBUG)
		auto sourcePdb = sourceDll;
		sourcePdb.replace_extension(L".pdb");
		if (!std::filesystem::copy_file(sourcePdb, shadowDir / sourcePdb.filename(), error))
		{
			EDITOR_LOG_ERROR("Editor plugin PDB copy failed ({}): {}", WStrToStr(sourcePdb.wstring()), error.message());
			return false;
		}
#endif

		m_Handle = LoadLibraryExW(m_ShadowDll.c_str(), nullptr,
			LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
		if (!m_Handle)
			EDITOR_LOG_ERROR("Editor plugin DLL load failed: Win32 error {}", GetLastError());
		return m_Handle != nullptr;
#else
		return false;
#endif
	}

	EditorPluginModule::Symbol EditorPluginModule::FindSymbol(const char* name) const
	{
#if defined(_WIN32)
		return m_Handle ? reinterpret_cast<Symbol>(GetProcAddress(m_Handle, name)) : nullptr;
#else
		return nullptr;
#endif
	}

	void EditorPluginModule::Swap(EditorPluginModule& other) noexcept
	{
#if defined(_WIN32)
		m_ShadowDll.swap(other.m_ShadowDll);
		std::swap(m_Handle, other.m_Handle);
#endif
	}

	void EditorPluginModule::Close()
	{
#if defined(_WIN32)
		if (m_Handle && !FreeLibrary(m_Handle))
		{
			EDITOR_LOG_ERROR("Editor plugin DLL unload failed: Win32 error {}", GetLastError());
			return;
		}
		m_Handle = nullptr;

		if (!m_ShadowDll.empty())
		{
			auto pdb = m_ShadowDll;
			pdb.replace_extension(L".pdb");
			std::error_code ignored;
			std::filesystem::remove(m_ShadowDll, ignored);
			std::filesystem::remove(pdb, ignored);
			std::filesystem::remove(m_ShadowDll.parent_path(), ignored);
			m_ShadowDll.clear();
		}
#endif
	}
}
