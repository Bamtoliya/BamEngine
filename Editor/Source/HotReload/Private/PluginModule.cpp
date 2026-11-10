#include "PluginModule.h"
#include "Editor_Macros.h"
#include "PlatformPath.h"
#include <cstdint>
#include <utility>
#if defined(_WIN32)
#include <bcrypt.h>
#include <array>
#include <memory>
#endif

namespace Editor
{
	PluginModule::~PluginModule()
	{
		Close();
	}

	bool PluginModule::Open(const wchar_t* dllName)
	{
#if defined(_WIN32)
		if (!dllName || !*dllName)
			return false;

		const auto executableDir = Engine::PlatformPath::GetExecutableDirectory();
#if defined(_DEBUG)
		constexpr bool copyPdb = true;
#else
		constexpr bool copyPdb = false;
#endif
		return PrepareShadowCopy(executableDir / dllName, executableDir / L"PluginShadow", copyPdb) &&
			LoadPrepared(LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
#else
		return false;
#endif
	}

	bool PluginModule::PrepareInstalled(const std::filesystem::path& sourceDll,
		const std::filesystem::path& shadowBase)
	{
#if defined(_WIN32)
		return PrepareShadowCopy(sourceDll, shadowBase, false);
#else
		return false;
#endif
	}

#if defined(_WIN32)
	bool PluginModule::PrepareShadowCopy(const std::filesystem::path& sourceDll,
		const std::filesystem::path& shadowBase, bool copyPdb)
	{
		if (m_Handle || !m_ShadowDll.empty() || !sourceDll.is_absolute() || !shadowBase.is_absolute())
			return false;

		const auto shadowRoot = shadowBase / sourceDll.stem() / std::to_wstring(GetCurrentProcessId());

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
			Close();
			return false;
		}

		if (copyPdb)
		{
			auto sourcePdb = sourceDll;
			sourcePdb.replace_extension(L".pdb");
			if (!std::filesystem::copy_file(sourcePdb, shadowDir / sourcePdb.filename(), error))
			{
				EDITOR_LOG_ERROR("Editor plugin PDB copy failed ({}): {}", WStrToStr(sourcePdb.wstring()), error.message());
				Close();
				return false;
			}
		}

		return true;
	}

	bool PluginModule::LoadPrepared(DWORD searchFlags)
	{
		if (m_Handle || m_ShadowDll.empty())
			return false;

		m_Handle = LoadLibraryExW(m_ShadowDll.c_str(), nullptr, searchFlags);
		if (!m_Handle)
			EDITOR_LOG_ERROR("Editor plugin DLL load failed: Win32 error {}", GetLastError());

		return m_Handle != nullptr;
	}
#endif

#if defined(_WIN32)
	namespace
	{
		using PluginReadHandle = std::unique_ptr<void, decltype(&CloseHandle)>;

		PluginReadHandle OpenPluginSnapshot(const std::filesystem::path& path)
		{
			const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
				nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

			return PluginReadHandle(file == INVALID_HANDLE_VALUE ? nullptr : file, &CloseHandle);
		}

		std::string HashPluginSnapshot(HANDLE file)
		{
			BCRYPT_ALG_HANDLE rawAlgorithm = nullptr;
			if (!BCRYPT_SUCCESS(BCryptOpenAlgorithmProvider(&rawAlgorithm, BCRYPT_SHA256_ALGORITHM, nullptr, 0)))
				return {};

			const auto closeAlgorithm = [](BCRYPT_ALG_HANDLE handle)
				{
					BCryptCloseAlgorithmProvider(handle, 0);
				};
			const std::unique_ptr<void, decltype(closeAlgorithm)> algorithm(rawAlgorithm, closeAlgorithm);

			BCRYPT_HASH_HANDLE rawHash = nullptr;
			if (!BCRYPT_SUCCESS(BCryptCreateHash(algorithm.get(), &rawHash, nullptr, 0, nullptr, 0, 0)))
				return {};

			const std::unique_ptr<void, decltype(&BCryptDestroyHash)> hash(rawHash, &BCryptDestroyHash);

			std::array<UCHAR, 64 * 1024> buffer;
			for (;;)
			{
				DWORD bytesRead = 0;
				if (!ReadFile(file, buffer.data(), static_cast<DWORD>(buffer.size()), &bytesRead, nullptr))
					return {};
				if (bytesRead == 0)
					break;
				if (!BCRYPT_SUCCESS(BCryptHashData(hash.get(), buffer.data(), bytesRead, 0)))
					return {};
			}

			std::array<UCHAR, 32> digest;
			if (!BCRYPT_SUCCESS(BCryptFinishHash(hash.get(), digest.data(), static_cast<ULONG>(digest.size()), 0)))
				return {};

			constexpr char hexDigits[] = "0123456789abcdef";
			std::string fingerprint;
			fingerprint.reserve(digest.size() * 2);
			for (const auto byte : digest)
			{
				fingerprint.push_back(hexDigits[byte >> 4]);
				fingerprint.push_back(hexDigits[byte & 0x0f]);
			}

			return fingerprint;
		}
	}
#endif

	std::string PluginModule::GetPreparedFingerprint() const
	{
#if defined(_WIN32)
		if (m_Handle || m_ShadowDll.empty())
			return {};

		const auto file = OpenPluginSnapshot(m_ShadowDll);
		return file ? HashPluginSnapshot(file.get()) : std::string{};
#else
		return {};
#endif
	}

	bool PluginModule::OpenApproved(std::string_view approvedFingerprint)
	{
#if defined(_WIN32)
		if (m_Handle || m_ShadowDll.empty() || approvedFingerprint.empty())
			return false;

		const auto file = OpenPluginSnapshot(m_ShadowDll);
		if (!file || HashPluginSnapshot(file.get()) != approvedFingerprint)
		{
			EDITOR_LOG_WARN("Installed plugin DLL fingerprint did not match approval");
			return false;
		}

		return LoadPrepared(LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
			LOAD_LIBRARY_SEARCH_SYSTEM32);
#else
		return false;
#endif
	}

	PluginModule::Symbol PluginModule::FindSymbol(const char* name) const
	{
#if defined(_WIN32)
		return m_Handle ? reinterpret_cast<Symbol>(GetProcAddress(m_Handle, name)) : nullptr;
#else
		return nullptr;
#endif
	}

	void PluginModule::Swap(PluginModule& other) noexcept
	{
#if defined(_WIN32)
		m_ShadowDll.swap(other.m_ShadowDll);
		std::swap(m_Handle, other.m_Handle);
#endif
	}

	void PluginModule::Close()
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
