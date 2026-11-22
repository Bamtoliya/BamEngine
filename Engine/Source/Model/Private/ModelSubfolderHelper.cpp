#include "ModelSubfolderHelper.h"
#include "PlatformPath.h"
#include <algorithm>
#include <cwctype>
#include <string>

namespace Engine
{
	namespace
	{
		bool EqualsIgnoreCase(std::wstring_view a, std::wstring_view b)
		{
			if (a.size() != b.size()) return false;
			for (size_t i = 0; i < a.size(); ++i)
			{
				if (std::towlower(a[i]) != std::towlower(b[i]))
					return false;
			}
			return true;
		}

		bool IsWindowsReservedDeviceName(std::wstring_view stem)
		{
			static const wchar_t* const reservedNames[] = {
				L"CON", L"PRN", L"AUX", L"NUL",
				L"COM1", L"COM2", L"COM3", L"COM4", L"COM5", L"COM6", L"COM7", L"COM8", L"COM9",
				L"LPT1", L"LPT2", L"LPT3", L"LPT4", L"LPT5", L"LPT6", L"LPT7", L"LPT8", L"LPT9"
			};

			for (const wchar_t* res : reservedNames)
			{
				if (EqualsIgnoreCase(stem, res))
					return true;
			}
			return false;
		}
	}

	std::wstring ModelSubfolderHelper::SanitizeName(std::wstring_view name, std::wstring_view fallback)
	{
		static const std::wstring_view invalidChars = L"\\/:*?\"<>|";
		std::wstring result;
		result.reserve(name.size());

		for (const wchar_t ch : name)
		{
			if (ch < 32 || invalidChars.find(ch) != std::wstring_view::npos)
				result.push_back(L'_');
			else
				result.push_back(ch);
		}

		// 앞뒤 공백 및 마침표 제거 (Windows 파일 시스템 특성)
		size_t start = 0;
		while (start < result.size() && (result[start] == L' ' || result[start] == L'\t' || result[start] == L'.'))
			++start;

		size_t end = result.size();
		while (end > start && (result[end - 1] == L' ' || result[end - 1] == L'\t' || result[end - 1] == L'.'))
			--end;

		if (start >= end)
			return std::wstring(fallback.empty() ? L"Model" : fallback);

		result = result.substr(start, end - start);

		if (IsWindowsReservedDeviceName(result))
			result = L"_" + result;

		return result.empty() ? std::wstring(fallback.empty() ? L"Model" : fallback) : result;
	}

	ModelSubfolderLayout ModelSubfolderHelper::ResolveLayout(
		const std::filesystem::path& destinationDirectory,
		std::wstring_view modelOrSourceStem,
		bool createDedicatedFolder,
		bool useMeshSubfolder,
		bool useMaterialSubfolder,
		bool useTextureSubfolder,
		bool useAnimationSubfolder)
	{
		ModelSubfolderLayout layout;
		layout.ModelStem = SanitizeName(modelOrSourceStem, L"Model");
		layout.UsesMeshSubfolder = useMeshSubfolder;
		layout.UsesMaterialSubfolder = useMaterialSubfolder;
		layout.UsesTextureSubfolder = useTextureSubfolder;
		layout.UsesAnimationSubfolder = useAnimationSubfolder;

		const std::filesystem::path destDir = destinationDirectory.empty()
			? std::filesystem::path(".") : destinationDirectory;

		const std::wstring destFolderLeaf = destDir.filename().wstring();
		const bool isAlreadyDedicated = EqualsIgnoreCase(destFolderLeaf, layout.ModelStem);

		std::filesystem::path root;
		if (isAlreadyDedicated)
		{
			root = destDir;
			layout.UsesDedicatedModelFolder = true;
		}
		else if (createDedicatedFolder)
		{
			root = destDir / layout.ModelStem;
			layout.UsesDedicatedModelFolder = true;
		}
		else
		{
			root = destDir;
			layout.UsesDedicatedModelFolder = false;
		}

		layout.RootDirectory = PlatformPath::Normalize(root);
		layout.ModelFilePath = PlatformPath::Normalize(layout.RootDirectory / (layout.ModelStem + L".bammodel"));
		layout.SkeletonFilePath = PlatformPath::Normalize(layout.RootDirectory / (layout.ModelStem + L"_Skeleton.bamskel"));
		layout.SidecarFilePath = PlatformPath::Normalize(layout.RootDirectory / (layout.ModelStem + L".bamimport.json"));

		layout.MeshesDirectory = useMeshSubfolder
			? PlatformPath::Normalize(layout.RootDirectory / L"Meshes") : layout.RootDirectory;
		layout.MaterialsDirectory = useMaterialSubfolder
			? PlatformPath::Normalize(layout.RootDirectory / L"Materials") : layout.RootDirectory;
		layout.TexturesDirectory = useTextureSubfolder
			? PlatformPath::Normalize(layout.RootDirectory / L"Textures") : layout.RootDirectory;
		layout.AnimationsDirectory = useAnimationSubfolder
			? PlatformPath::Normalize(layout.RootDirectory / L"Animations") : layout.RootDirectory;

		return layout;
	}

	std::filesystem::path ModelSubfolderHelper::ResolveUniqueDirectory(const std::filesystem::path& dirPath)
	{
		std::error_code ec;
		if (!std::filesystem::exists(dirPath, ec))
			return PlatformPath::Normalize(dirPath);

		const auto parent = dirPath.parent_path();
		const auto stem = dirPath.filename().wstring();

		for (uint32 index = 1; index < 10000; ++index)
		{
			auto candidate = parent / (stem + L"_" + std::to_wstring(index));
			if (!std::filesystem::exists(candidate, ec))
				return PlatformPath::Normalize(candidate);
		}

		return PlatformPath::Normalize(dirPath);
	}

	std::filesystem::path ModelSubfolderHelper::ResolveUniqueFilePath(const std::filesystem::path& filePath)
	{
		std::error_code ec;
		if (!std::filesystem::exists(filePath, ec))
			return PlatformPath::Normalize(filePath);

		const auto parent = filePath.parent_path();
		const auto stem = filePath.stem().wstring();
		const auto ext = filePath.extension().wstring();

		for (uint32 index = 1; index < 10000; ++index)
		{
			auto candidate = parent / (stem + L"_" + std::to_wstring(index) + ext);
			if (!std::filesystem::exists(candidate, ec))
				return PlatformPath::Normalize(candidate);
		}

		return PlatformPath::Normalize(filePath);
	}

	std::filesystem::path ModelSubfolderHelper::BuildMeshPath(const ModelSubfolderLayout& layout, std::wstring_view meshStem, uint32 lodLevel)
	{
		std::wstring name = SanitizeName(meshStem, L"Mesh");
		if (lodLevel > 0)
			name += L"_LOD" + std::to_wstring(lodLevel);
		return PlatformPath::Normalize(layout.MeshesDirectory / (name + L".bammesh"));
	}

	std::filesystem::path ModelSubfolderHelper::BuildMaterialPath(const ModelSubfolderLayout& layout, std::wstring_view materialStem)
	{
		const std::wstring name = SanitizeName(materialStem, L"Material");
		return PlatformPath::Normalize(layout.MaterialsDirectory / (name + L".bammatinst"));
	}

	std::filesystem::path ModelSubfolderHelper::BuildTexturePath(const ModelSubfolderLayout& layout, std::wstring_view textureStem, std::wstring_view extension)
	{
		const std::wstring name = SanitizeName(textureStem, L"Texture");
		std::wstring ext = extension.empty() ? L".png" : std::wstring(extension);
		if (!ext.empty() && ext.front() != L'.')
			ext = L"." + ext;
		return PlatformPath::Normalize(layout.TexturesDirectory / (name + ext));
	}

	std::filesystem::path ModelSubfolderHelper::BuildAnimationPath(const ModelSubfolderLayout& layout, std::wstring_view animationStem)
	{
		const std::wstring name = SanitizeName(animationStem, L"Animation");
		return PlatformPath::Normalize(layout.AnimationsDirectory / (name + L".bamanim"));
	}
}
