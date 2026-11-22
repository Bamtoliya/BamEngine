#pragma once

#include "Model.h"
#include "ModelImporter.h"
#include "ModelImportTask.h"
#include "ModelConverter.h"
#include "TextureImporter.h"
#include "AssetWorkerHost.h"
#include "ResourceManager.h"
#include "Logger.h"
#include "PlatformPath.h"
#include "ModelSubfolderHelper.h"

#pragma push_macro("new")
#undef new
#include <archive/BeveArchive.h>
#include <archive/JsonArchive.h>
#pragma pop_macro("new")

#include "HotReloadManager.h"
#include "AssetHotReloadManager.h"

namespace Editor
{
	struct ModelFinalizeState
	{
		~ModelFinalizeState() noexcept;
		struct OutputReservation
		{
			struct DirectoryLess
			{
				bool operator()(const wstring& lhs, const wstring& rhs) const noexcept
				{
					return CompareStringOrdinal(lhs.c_str(), -1, rhs.c_str(), -1, TRUE) == CSTR_LESS_THAN;
				}
			};

			OutputReservation() = default;
			OutputReservation(const OutputReservation&) = delete;
			OutputReservation& operator=(const OutputReservation&) = delete;

			EResult Acquire(const filesystem::path& outputDir)
			{
				std::error_code ec;
				const auto absolutePath = Engine::PlatformPath::ResolveProjectRelative(outputDir);
				const auto canonicalPath = filesystem::weakly_canonical(absolutePath, ec);
				if (ec)
				{
					BAM_LOG(Error, "Editor", "Asset", "Cannot resolve import output: path={}, error={}",
						WStrToStr(absolutePath.wstring()), ec.message());
					return EResult::Fail;
				}

				auto key = canonicalPath.generic_wstring();
				if (!key.empty() && key.back() == L'/' && canonicalPath.has_relative_path())
					key.pop_back();

				std::lock_guard lock(s_Mutex);
				if (!s_ActiveDirectories.insert(key).second)
				{
					BAM_LOG(Error, "Editor", "Asset", "Another model import is using output directory: {}", WStrToStr(key));
					return EResult::AlreadyInitialized;
				}

				m_Key.swap(key);
				return EResult::Success;
			}

			~OutputReservation()
			{
				if (m_Key.empty())
					return;

				std::lock_guard lock(s_Mutex);
				s_ActiveDirectories.erase(m_Key);
			}

		private:
			inline static std::mutex s_Mutex;
			inline static std::set<wstring, DirectoryLess> s_ActiveDirectories;
			wstring m_Key;
		};

		struct ResourceDeleter
		{
			void operator()(Resource* resource) const
			{
				if (!resource)
					return;

				resource->Free();
				delete resource;
			}
		};

		struct StagedFile
		{
			filesystem::path Path;
			bool Owned = false;

			StagedFile() = default;
			StagedFile(const StagedFile&) = delete;
			StagedFile& operator=(const StagedFile&) = delete;

			~StagedFile()
			{
				if (Owned)
				{
					std::error_code ec;
					filesystem::remove(Path, ec);
				}
			}
		};

		struct MeshBackup
		{
			filesystem::path Directory;
			filesystem::path File;
			bool Owned = false;
			bool Keep = false;

			~MeshBackup()
			{
				if (!Owned || Keep)
					return;

				std::error_code ec;
				filesystem::remove(File, ec);
				filesystem::remove(Directory, ec);
			}
		};

		struct SavedMesh
		{
			archive::ArchiveResult Result;
			std::unique_ptr<StagedFile> File;
			filesystem::path PublishedPath;
			std::unique_ptr<MeshBackup> Backup;
			size_t ByteCount = 0;
			double SnapshotMs = 0.0;
			double WorkerMs = 0.0;
			double WriteMs = 0.0;
			double ReleaseMs = 0.0;
		};

		using MeshOwner = std::unique_ptr<Mesh, ResourceDeleter>;
		using SkeletonOwner = std::unique_ptr<Skeleton, ResourceDeleter>;
		using AnimationOwner = std::unique_ptr<Animation, ResourceDeleter>;
		using ModelOwner = std::unique_ptr<Resource, ResourceDeleter>;

		struct PendingMesh
		{
			wstring Key;
			MeshOwner Resource;
			size_t OriginalMeshIndex = 0;
			uint32 LODLevel = 0;
		};

		OutputReservation Output;
		ModelImportDesc Settings;
		ModelCreateDesc ModelDesc;
		Engine::ModelSubfolderLayout Layout;
		filesystem::path MeshDirectory;
		ResourceHandle<Material> BaseMaterial;
		std::vector<PendingMesh> Meshes;
		unordered_map<wstring, uint32> NameCounts;
		size_t NextMesh = 0;
		std::vector<SavedMesh> StagedMeshes;
		size_t NextPublish = 0;
		struct MeshReloadBackup
		{
			Handle Existing;
			uint64 GeometryVersion;
			std::unique_ptr<archive::BeveArchive> Snapshot;
		};

		std::vector<MeshReloadBackup> ReloadedMeshes;
		bool MeshCommitComplete = false;
		double SnapshotMs = 0.0;
		std::future<SavedMesh> Saving;
		struct TextureBakeRequest
		{
			filesystem::path SourcePath;
			filesystem::path BamtexPath;
			filesystem::path DdsPath;
			tagTextureImportDesc Options;
		};
		struct TextureBake
		{
			filesystem::path BamtexPath;
			std::future<EResult> Future;
		};
		std::vector<TextureBakeRequest> PendingTextureBakes;
		size_t NextTextureBake = 0;
		std::optional<TextureBake> ActiveTextureBake;
		filesystem::path TextureOutputDirectory;
		std::string ModelDisplayName;
		unordered_map<wstring, bool> TextureBakeResults;
		unordered_map<wstring, bool> EmbeddedTextureFilesWritten;
		bool TextureBakesScheduled = false;
		bool TextureBakesComplete = false;

		std::vector<Bone> SkeletonBones;
		uint64 SkinBindingSignature = 0;
	};
}

namespace
{
	wstring SanitizeFileName(wstring name)
	{
		return Engine::ModelSubfolderHelper::SanitizeName(name);
	}

	void AssignModelSourceKeys(const Editor::PreparedModelData& data, ModelCreateDesc& desc)
	{
		std::vector<wstring> paths(desc.Nodes.size());
		std::unordered_map<wstring, size_t> pathCounts;

		for (size_t i = 0; i < desc.Nodes.size(); ++i)
		{
			const auto& node = desc.Nodes[i];
			if (node.Name.empty() || node.ParentIndex < -1)
				continue;

			wstring path;
			if (node.ParentIndex >= 0)
			{
				const auto parent = static_cast<size_t>(node.ParentIndex);
				if (parent >= i || paths[parent].empty())
					continue;
				path = paths[parent];
			}

			path += std::to_wstring(node.Name.size()) + L":" + node.Name;
			paths[i] = std::move(path);
			++pathCounts[paths[i]];
		}

		for (size_t i = 0; i < desc.Nodes.size(); ++i)
		{
			auto& node = desc.Nodes[i];
			if (paths[i].empty() || pathCounts.at(paths[i]) != 1)
				continue;
			if (node.ParentIndex >= 0 && desc.Nodes[static_cast<size_t>(node.ParentIndex)].SourceNodeKey.empty())
				continue;

			node.SourceNodeKey = paths[i];
			node.MeshAttachmentKeys.resize(node.MeshIndices.size());
			std::unordered_map<wstring, size_t> meshNameCounts;

			for (size_t j = 0; j < node.MeshIndices.size(); ++j)
			{
				const auto meshIndex = node.MeshIndices[j];
				if (meshIndex >= data.Meshes.size() || data.Meshes[meshIndex].Name.empty())
					continue;

				auto& key = node.MeshAttachmentKeys[j];
				key = StrToWStr(data.Meshes[meshIndex].Name);
				if (!key.empty())
					++meshNameCounts[key];
			}

			for (auto& key : node.MeshAttachmentKeys)
			{
				if (!key.empty() && meshNameCounts.at(key) != 1)
					key.clear();
			}
		}
	}

	wstring MakeUniqueStem(const wstring& sourceStem, const wstring& itemKey, const wstring& fallbackPrefix, uint32 serial, unordered_map<wstring, uint32>& nameCounts)
	{
		wstring safeKey = itemKey.empty() ? (fallbackPrefix + L"_" + std::to_wstring(serial)) : itemKey;
		safeKey = SanitizeFileName(safeKey);

		wstring baseStem = SanitizeFileName(sourceStem) + L"_" + safeKey;

		uint32& count = nameCounts[baseStem];
		wstring finalStem = (count == 0) ? baseStem : (baseStem + L"_" + std::to_wstring(count));
		++count;

		return finalStem;
	}

	mat4 ArrayToGlmMat4(const std::array<float, 16>& values)
	{
		mat4 result(1.f);
		for (int column = 0; column < 4; ++column)
		{
			for (int row = 0; row < 4; ++row)
				result[column][row] = values[column * 4 + row];
		}
		return result;
	}

	EResult ValidateImportOutput(ResourceManager& manager, const wstring& key, bool saveToFile, bool overwrite = false)
	{
		if (!overwrite && manager.IsValid(manager.FindHandleByKey(key)))
		{
			BAM_LOG(Error, "Editor", "Asset", "Import output resource already registered: {}", WStrToStr(key));
			return EResult::AlreadyInitialized;
		}
		if (!saveToFile)
			return EResult::Success;
		std::error_code ec;
		const auto targetPath = Engine::PlatformPath::ResolveProjectRelative(filesystem::path(key));
		const bool exists = filesystem::exists(targetPath, ec);
		if (ec)
		{
			BAM_LOG(Error, "Editor", "Asset", "Failed to inspect import output: path={}, error={}",
				WStrToStr(key), ec.message());
			return EResult::Fail;
		}
		if (!overwrite && exists)
		{
			BAM_LOG(Error, "Editor", "Asset", "Import output already exists: {}", WStrToStr(key));
			return EResult::AlreadyInitialized;
		}
		if (exists && !filesystem::is_regular_file(targetPath, ec))
		{
			BAM_LOG(Error, "Editor", "Asset", "Import output is not a regular file: {}", WStrToStr(targetPath.wstring()));
			return EResult::Fail;
		}
		return EResult::Success;
	}

	filesystem::path MakeImportStagingPath(const filesystem::path& target)
	{
		// 모델 임포트 확정 단계는 메인 스레드에서 실행합니다.
		static Engine::uint64 serial = 0;
		auto stagingPath = target;
		const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
		stagingPath += ".bamimport-" + std::to_string(++serial) + "-" + std::to_string(stamp) + ".tmp";
		return stagingPath;
	}

	EResult SaveEmbeddedTexture(const filesystem::path& target, const vector<uint8>& bytes)
	{
		if (std::cmp_greater(bytes.size(), (std::numeric_limits<std::streamsize>::max)()))
			return EResult::InvalidArgument;

		std::error_code ec;
		filesystem::create_directories(target.parent_path(), ec);
		if (ec)
			return EResult::Fail;

		ModelFinalizeState::StagedFile staged;
		staged.Path = MakeImportStagingPath(target);
		std::ofstream file(staged.Path, std::ios::out | std::ios::binary | std::ios::noreplace);
		if (!file)
			return EResult::Fail;

		staged.Owned = true;
		file.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		file.close();
		if (!file)
			return EResult::Fail;

		filesystem::rename(staged.Path, target, ec);
		if (ec)
			return EResult::Fail;

		staged.Owned = false;
		return EResult::Success;
	}

	EResult ImportTextureFallback(const filesystem::path& sourcePath, const filesystem::path& outputDir,
		tagTextureImportDesc options)
	{
		struct ImporterDeleter
		{
			void operator()(TextureImporter* importer) const
			{
				if (importer)
					Safe_Release(importer);
			}
		};
		std::unique_ptr<TextureImporter, ImporterDeleter> importer(TextureImporter::Create());
		if (!importer)
			return EResult::Fail;

		return importer->Import(sourcePath, outputDir, &options);
	}

	tagTextureImportDesc MakeModelTextureImportDesc(uint32 slot, ETextureCompressionMode mode)
	{
		tagTextureImportDesc desc{};
		if (slot == 1)
		{
			desc.TargetFormat = Engine::ETextureFormat::BC5_RG_UNORM;
			desc.CompressFlags = ECompressFlags::GenerateMipMaps | ECompressFlags::HighQuality | ECompressFlags::NormalMap;
		}
		else if (slot == 2)
		{
			desc.TargetFormat = Engine::ETextureFormat::BC7_RGBA_UNORM;
			desc.CompressFlags = ECompressFlags::GenerateMipMaps | ECompressFlags::HighQuality;
		}
		else
		{
			desc.TargetFormat = Engine::ETextureFormat::BC7_RGBA_UNORM;
			desc.CompressFlags = ECompressFlags::GenerateMipMaps | ECompressFlags::HighQuality | ECompressFlags::SRGB;
		}
		desc.CompressFlags = ApplyTextureCompressionMode(desc.CompressFlags, mode, desc.TargetFormat);
		return desc;
	}

	bool StartNextModelTextureBake(ModelFinalizeState& state)
	{
		if (state.NextTextureBake >= state.PendingTextureBakes.size())
			return false;

		const size_t total = state.PendingTextureBakes.size();
		const size_t index = state.NextTextureBake++;
		const auto& request = state.PendingTextureBakes[index];
		const auto& outputDir = state.TextureOutputDirectory;
		constexpr float kModelPhaseEnd = 0.35f;
		constexpr float kTexturePhaseSpan = 0.60f;
		const float sliceSpan = kTexturePhaseSpan / static_cast<float>(total);
		AssetWorkerHost::ImportProgressScope progressScope;
		progressScope.BaseProgress = kModelPhaseEnd + sliceSpan * static_cast<float>(index);
		progressScope.ProgressSpan = sliceSpan;
		progressScope.DisplayName = (state.ModelDisplayName.empty()
			? WStrToStr(request.SourcePath.filename().wstring()) : state.ModelDisplayName)
			+ " (" + std::to_string(index + 1) + "/" + std::to_string(total) + ")";

		auto promise = std::make_shared<std::promise<EResult>>();
		auto future = promise->get_future();
		const bool queued = AssetWorkerHost::IsCreated() && AssetWorkerHost::Get().EnqueueImportTexture(
			request.SourcePath, request.BamtexPath, request.DdsPath, request.Options.TargetFormat,
			request.Options.CompressFlags,
			[promise, sourcePath = request.SourcePath, outputDir, options = request.Options]
			(AssetWorkerHost::TextureImportResult result)
			{
				try
				{
					EResult importResult = EResult::Fail;
					if (result == AssetWorkerHost::TextureImportResult::OutputReady)
						importResult = EResult::Success;
					else if (result == AssetWorkerHost::TextureImportResult::Unavailable)
						importResult = ImportTextureFallback(sourcePath, outputDir, options);
					else if (result == AssetWorkerHost::TextureImportResult::Cancelled)
						BAM_LOG(Info, "Editor", "Asset", "Model texture worker import cancelled: {}",
							WStrToStr(sourcePath.wstring()));
					promise->set_value(importResult);
				}
				catch (...)
				{
					promise->set_exception(std::current_exception());
				}
			},
			std::move(progressScope));

		if (queued)
		{
			state.ActiveTextureBake.emplace(ModelFinalizeState::TextureBake{ request.BamtexPath, std::move(future) });
		}
		else
		{
			state.ActiveTextureBake.emplace(ModelFinalizeState::TextureBake{ request.BamtexPath,
				std::async(std::launch::async, [request, outputDir]()
				{
					return ImportTextureFallback(request.SourcePath, outputDir, request.Options);
				}) });
		}
		return true;
	}

	EResult ScheduleModelTextureBakes(ModelFinalizeState& state, const PreparedModelData& data,
		const filesystem::path& outputDir)
	{
		state.TextureBakesScheduled = true;
		const auto& settings = state.Settings;
		if (!settings.SaveToFile || !settings.BakeTexturesToDDS ||
			!HasFlag(settings.ImportOptions, EModelImportOption::Materials) || data.Materials.empty())
		{
			state.TextureBakesComplete = true;
			return EResult::Success;
		}

		auto& manager = ResourceManager::Get();
		auto nameCounts = state.NameCounts;
		const wstring sourceStem = data.SourcePath.stem().wstring();
		if (HasFlag(settings.ImportOptions, EModelImportOption::Skeleton) && !data.Bones.empty())
			MakeUniqueStem(sourceStem, L"Skeleton", L"Skeleton", 0, nameCounts);

		if (HasFlag(settings.ImportOptions, EModelImportOption::Animations))
		{
			for (uint32 animIdx = 0; animIdx < static_cast<uint32>(data.Animations.size()); ++animIdx)
				MakeUniqueStem(sourceStem, StrToWStr(data.Animations[animIdx].Name), L"Anim", animIdx, nameCounts);
		}

		const auto materialDir = state.ModelDesc.UseMaterialSubfolder ? outputDir / L"Materials" : outputDir;
		const auto textureDir = state.ModelDesc.UseTextureSubfolder ? outputDir / L"Textures" : outputDir;
		const auto resolvedTextureDir = Engine::PlatformPath::ResolveProjectRelative(textureDir);
		state.TextureOutputDirectory = resolvedTextureDir;
		unordered_map<wstring, size_t> requestByOutput;

		for (size_t matIdx = 0; matIdx < data.Materials.size(); ++matIdx)
		{
			const auto& srcMat = data.Materials[matIdx];
			const wstring matStem = MakeUniqueStem(sourceStem, StrToWStr(srcMat.Name), L"Mat",
				static_cast<uint32>(matIdx), nameCounts);
			const wstring matKey = NormalizePath((materialDir / (matStem + L".bammatinst")).wstring());
			if (const EResult result = ValidateImportOutput(manager, matKey, settings.SaveToFile, settings.Overwrite);
				result != EResult::Success)
			{
				return result;
			}

			if (settings.Overwrite && !settings.OverwriteMaterials)
			{
				auto existingHandle = manager.FindHandleByKey(matKey);
				bool hasExistingMaterial = manager.IsValid(existingHandle);
				if (!hasExistingMaterial)
				{
					std::error_code ec;
					hasExistingMaterial = filesystem::exists(Engine::PlatformPath::ResolveUri(matKey), ec);
					if (ec)
					{
						BAM_LOG(Error, "Editor", "Asset", "Failed to inspect material: {}, {}", WStrToStr(matKey), ec.message());
						return EResult::Fail;
					}
				}
				if (hasExistingMaterial)
					continue;
			}

			const Tools::ModelConversion::TextureBindingInfo* bindings[] = {
				&srcMat.BaseColorTexture, &srcMat.NormalTexture,
				&srcMat.MetallicRoughnessTexture, &srcMat.EmissiveTexture
			};
			for (uint32 slot = 0; slot < 4; ++slot)
			{
				const auto& info = *bindings[slot];
				if (info.Path.empty() && info.EmbeddedData.empty())
					continue;

				const bool embedded = info.IsEmbedded && !info.EmbeddedData.empty();
				filesystem::path sourcePath;
				if (embedded)
				{
					static constexpr const wchar_t* slotNames[] = { L"Albedo", L"Normal", L"ORM", L"Emissive" };
					sourcePath = Engine::PlatformPath::ResolveProjectRelative(
						textureDir / (matStem + L"_" + slotNames[slot] + L".png"));

					std::error_code ec;
					const bool exists = filesystem::exists(sourcePath, ec);
					const bool shouldWrite = !exists || settings.Overwrite;
					const EResult saveResult = ec ? EResult::Fail
						: (shouldWrite ? SaveEmbeddedTexture(sourcePath, info.EmbeddedData) : EResult::Success);
					if (saveResult != EResult::Success)
					{
						BAM_LOG(Error, "Editor", "Asset", "Embedded texture save failed: path={}, result={}",
							WStrToStr(sourcePath.wstring()), ResultToString(saveResult));
						return saveResult;
					}
					state.EmbeddedTextureFilesWritten[sourcePath.wstring()] = shouldWrite;
				}
				else
				{
					sourcePath = ModelImporter::ResolveExternalTexturePath(data.SourcePath, outputDir, info.Path);
					if (sourcePath.empty())
						continue;
				}

				const auto extension = sourcePath.extension().string();
				if (_stricmp(extension.c_str(), ".dds") == 0 || _stricmp(extension.c_str(), ".bamtex") == 0)
					continue;

				filesystem::path bamtexPath = resolvedTextureDir / sourcePath.filename();
				bamtexPath.replace_extension(".bamtex");
				std::error_code ec;
				const bool bakedExists = filesystem::exists(bamtexPath, ec);
				if (ec)
				{
					BAM_LOG(Error, "Editor", "Asset", "Failed to inspect baked texture: path={}, error={}",
						WStrToStr(bamtexPath.wstring()), ec.message());
					return EResult::Fail;
				}
				if (bakedExists && !settings.Overwrite)
					continue;

				filesystem::create_directories(resolvedTextureDir, ec);
				if (ec)
				{
					BAM_LOG(Error, "Editor", "Asset", "Failed to create texture output directory: path={}, error={}",
						WStrToStr(resolvedTextureDir.wstring()), ec.message());
					return EResult::Fail;
				}

				filesystem::path ddsPath = bamtexPath;
				ddsPath.replace_extension(".dds");
				ModelFinalizeState::TextureBakeRequest request{
					sourcePath, bamtexPath, ddsPath, MakeModelTextureImportDesc(slot, settings.TextureCompression) };
				const wstring outputKey = bamtexPath.wstring();
				const auto found = requestByOutput.find(outputKey);
				if (found == requestByOutput.end())
				{
					requestByOutput.emplace(outputKey, state.PendingTextureBakes.size());
					state.PendingTextureBakes.push_back(std::move(request));
				}
				else
				{
					state.PendingTextureBakes[found->second] = std::move(request);
				}
			}
		}

		state.TextureBakesComplete = !StartNextModelTextureBake(state);
		return EResult::Success;
	}

	bool PollModelTextureBakes(ModelFinalizeState& state)
	{
		if (!state.ActiveTextureBake ||
			state.ActiveTextureBake->Future.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
			return false;

		auto& bake = *state.ActiveTextureBake;
		EResult result = EResult::Fail;
		try
		{
			result = bake.Future.get();
		}
		catch (const std::exception& exception)
		{
			BAM_LOG(Error, "Editor", "Asset", "Model texture bake failed: path={}, error={}",
				WStrToStr(bake.BamtexPath.wstring()), exception.what());
		}
		catch (...)
		{
			BAM_LOG(Error, "Editor", "Asset", "Model texture bake failed: path={}",
				WStrToStr(bake.BamtexPath.wstring()));
		}

		const bool succeeded = result == EResult::Success;
		state.TextureBakeResults[bake.BamtexPath.wstring()] = succeeded;
		if (!succeeded)
		{
			BAM_LOG(Warn, "Editor", "Asset", "Model texture bake failed; using source texture: {}",
				WStrToStr(bake.BamtexPath.wstring()));
		}

		state.ActiveTextureBake.reset();
		state.TextureBakesComplete = !StartNextModelTextureBake(state);
		return state.TextureBakesComplete;
	}

	bool BackupMeshOutput(ModelFinalizeState::SavedMesh& saved, std::error_code& ec)
	{
		const auto status = filesystem::symlink_status(saved.PublishedPath, ec);
		if (ec == std::errc::no_such_file_or_directory)
			ec.clear();
		if (ec)
			return false;
		if (!filesystem::exists(status))
			return true;
		if (!filesystem::is_regular_file(status))
		{
			ec = std::make_error_code(std::errc::invalid_argument);
			return false;
		}

		auto backup = std::make_unique<ModelFinalizeState::MeshBackup>();
		backup->Directory = MakeImportStagingPath(saved.PublishedPath);
		backup->File = backup->Directory / L"original.bak";

		if (!filesystem::create_directory(backup->Directory, ec))
		{
			if (!ec)
				ec = std::make_error_code(std::errc::file_exists);
			return false;
		}

		backup->Owned = true;
		if (!filesystem::copy_file(saved.PublishedPath, backup->File, filesystem::copy_options::none, ec))
			return false;

		saved.Backup = std::move(backup);
		return true;
	}

	void RollbackPublishedMeshes(ModelFinalizeState& state)
	{
		while (state.NextPublish > 0)
		{
			auto& saved = state.StagedMeshes[--state.NextPublish];
			std::error_code ec;

			if (saved.Backup)
			{
				saved.Backup->Keep = true;
				filesystem::rename(saved.Backup->File, saved.PublishedPath, ec);
				if (!ec)
					saved.Backup->Keep = false;
			}
			else
			{
				const auto status = filesystem::symlink_status(saved.PublishedPath, ec);
				if (ec == std::errc::no_such_file_or_directory)
					ec.clear();
				if (!ec && filesystem::exists(status))
				{
					if (filesystem::is_regular_file(status))
						filesystem::remove(saved.PublishedPath, ec);
					else
						ec = std::make_error_code(std::errc::invalid_argument);
				}
			}

			if (ec)
			{
				const auto backupPath = saved.Backup ? WStrToStr(saved.Backup->File.wstring()) : std::string{};
				BAM_LOG(Error, "Editor", "Asset", "Mesh rollback failed: path={}, backup={}, error={}",
					WStrToStr(saved.PublishedPath.wstring()), backupPath, ec.message());
			}
			else if (HotReloadManager::IsCreated())
			{
				HotReloadManager::Get().GetAssetReloader().RecordMeshFileState(saved.PublishedPath);
			}
		}
	}

	void RollbackReloadedMeshes(ModelFinalizeState& state)
	{
		auto& manager = ResourceManager::Get();

		while (!state.ReloadedMeshes.empty())
		{
			auto backup = std::move(state.ReloadedMeshes.back());
			state.ReloadedMeshes.pop_back();

			auto* mesh = dynamic_cast<Mesh*>(manager.GetResource(backup.Existing));
			if (!mesh || mesh->GetGeometryVersion() == backup.GeometryVersion)
				continue;

			const auto result = mesh->ReadArchive(*backup.Snapshot);
			if (!result)
			{
				BAM_LOG(Error, "Editor", "Asset", "Mesh memory rollback failed: key={}, code={}, field={}",
					WStrToStr(mesh->GetKey()), static_cast<int>(result.Code), result.Field);
				continue;
			}

			mesh->IncreaseVersion();
		}
	}

	EResult ApplyReimport(Resource* existing, const Resource& imported, bool savedToFile)
	{
		if (existing == nullptr || typeid(*existing) != typeid(imported))
			return EResult::InvalidArgument;

		if (savedToFile)
			return existing->Reload(Engine::PlatformPath::ResolveProjectRelative(imported.GetPath()));

		archive::BeveArchive snapshot;
		auto result = imported.WriteArchive(snapshot);
		if (result)
			result = existing->ReadArchive(snapshot);

		if (!result)
		{
			BAM_LOG(Error, "Editor", "Asset", "In-memory reimport failed: key={}, code={}, field={}",
				WStrToStr(imported.GetKey()), static_cast<int>(result.Code), result.Field);
			return EResult::Fail;
		}

		existing->IncreaseVersion();
		return EResult::Success;
	}

	EResult CommitPreparedModel(ModelFinalizeState& state, const PreparedModelData& data, const filesystem::path& outputDir)
	{
		auto& manager = ResourceManager::Get();
		auto& modelDesc = state.ModelDesc;
		auto& pending = state.Meshes;
		const auto& settings = state.Settings;
		const auto& modelKey = modelDesc.Key;
		const wstring sourceStem = data.SourcePath.stem().wstring();

		using SkeletonOwner = ModelFinalizeState::SkeletonOwner;
		using AnimationOwner = ModelFinalizeState::AnimationOwner;
		using ModelOwner = ModelFinalizeState::ModelOwner;

		if (const EResult result = ValidateImportOutput(manager, modelKey, settings.SaveToFile, settings.Overwrite);
			result != EResult::Success)
		{
			return result;
		}

		for (const auto& mesh : pending)
		{
			// 메시 파일은 이 작업이 이미 저장했으므로 등록 여부만 검사합니다.
			if (const EResult result = ValidateImportOutput(manager, mesh.Key, false, settings.Overwrite);
				result != EResult::Success)
			{
				return result;
			}
		}

		// 이후 단계가 실패하면 이번에 등록한 리소스들을 역순으로 정리합니다.
		struct RegistrationRollback
		{
			ResourceManager& Manager;
			std::vector<Handle> Handles;
			bool Committed = false;

			~RegistrationRollback()
			{
				if (Committed)
					return;

				for (auto it = Handles.rbegin(); it != Handles.rend(); ++it)
					Manager.DestroyResource(*it);
			}
		};

		RegistrationRollback rollback{ manager, {}, false };
		rollback.Handles.reserve(pending.size() + 1 + data.Animations.size() + data.Materials.size());
		modelDesc.Meshes.reserve(data.Meshes.size());
		modelDesc.MeshLODs.resize(data.Meshes.size());

		for (auto& mesh : pending)
		{
			ResourceHandle<Mesh> meshHandle;
			auto existingHandle = manager.FindHandleByKey(mesh.Key);
			if (settings.Overwrite && manager.IsValid(existingHandle))
			{
				auto* existingMesh = dynamic_cast<Mesh*>(manager.GetResource(existingHandle));
				if (!existingMesh)
					return EResult::InvalidArgument;

				auto snapshot = std::make_unique<archive::BeveArchive>();
				const auto snapshotResult = existingMesh->WriteArchive(*snapshot);
				if (!snapshotResult)
				{
					BAM_LOG(Error, "Editor", "Asset", "Failed to snapshot mesh: key={}, code={}, field={}",
						WStrToStr(mesh.Key), static_cast<int>(snapshotResult.Code), snapshotResult.Field);
					return EResult::Fail;
				}

				state.ReloadedMeshes.push_back({ existingHandle, existingMesh->GetGeometryVersion(), std::move(snapshot) });
				const auto result = ApplyReimport(existingMesh, *mesh.Resource, settings.SaveToFile);
				if (result != EResult::Success)
					return result;

				meshHandle = ResourceHandle<Mesh>(existingHandle);
			}
			else
			{
				Mesh* instance = mesh.Resource.get();
				meshHandle = manager.AddResource<Mesh>(mesh.Key, mesh.Resource.release());

				if (!meshHandle.IsValid())
					return EResult::Fail;

				if (meshHandle.Get() != instance)
					return EResult::AlreadyInitialized;

				rollback.Handles.push_back(meshHandle.GetRawHandle());
			}

			if (mesh.LODLevel == 0)
			{
				modelDesc.Meshes.push_back(meshHandle);
			}
			else if (mesh.LODLevel == 1)
			{
				if (mesh.OriginalMeshIndex < modelDesc.MeshLODs.size())
					modelDesc.MeshLODs[mesh.OriginalMeshIndex].LOD1Mesh = meshHandle;
			}
			else if (mesh.LODLevel == 2)
			{
				if (mesh.OriginalMeshIndex < modelDesc.MeshLODs.size())
					modelDesc.MeshLODs[mesh.OriginalMeshIndex].LOD2Mesh = meshHandle;
			}
		}

		// 스켈레톤 생성 및 저장 (.bamskel)
		if (HasFlag(settings.ImportOptions, EModelImportOption::Skeleton) && !data.Bones.empty())
		{
			const wstring skelStem = MakeUniqueStem(sourceStem, L"Skeleton", L"Skeleton", 0, state.NameCounts);
			const wstring skelKey = NormalizePath((outputDir / (skelStem + L".bamskel")).wstring());

			if (const EResult result = ValidateImportOutput(manager, skelKey, settings.SaveToFile, settings.Overwrite);
				result != EResult::Success)
			{
				return result;
			}

			SkeletonCreateDesc skelDesc;
			skelDesc.Key = skelKey;
			skelDesc.Path = skelKey;
			skelDesc.Bones = std::move(state.SkeletonBones);

			SkeletonOwner skeleton(Skeleton::Create(&skelDesc));
			if (!skeleton)
			{
				BAM_LOG(Error, "Editor", "Asset", "Skeleton creation failed: {}", WStrToStr(skelKey));
				return EResult::Fail;
			}

			if (settings.SaveToFile)
			{
				const EResult saveResult = manager.SaveToBinaryFile(skeleton.get(), skelKey);
				if (saveResult != EResult::Success)
					return saveResult;
			}

			auto existingSkelHandle = manager.FindHandleByKey(skelKey);
			if (settings.Overwrite && manager.IsValid(existingSkelHandle))
			{
				const auto result = ApplyReimport(manager.GetResource(existingSkelHandle), *skeleton, settings.SaveToFile);
				if (result != EResult::Success)
					return result;

				modelDesc.Skeleton = ResourceHandle<Skeleton>(existingSkelHandle);
			}
			else
			{
				Skeleton* skelInstance = skeleton.get();
				auto skelHandle = manager.AddResource<Skeleton>(skelKey, skeleton.release());
				if (!skelHandle.IsValid() || skelHandle.Get() != skelInstance)
					return EResult::Fail;

				rollback.Handles.push_back(skelHandle.GetRawHandle());
				modelDesc.Skeleton = std::move(skelHandle);
			}
		}

		const auto animationDir = modelDesc.UseAnimationSubfolder ? outputDir / L"Animations" : outputDir;
		// 애니메이션 클립 생성 및 저장 (.bamanim)
		if (HasFlag(settings.ImportOptions, EModelImportOption::Animations) && !data.Animations.empty())
		{
			modelDesc.Animations.reserve(data.Animations.size());

			for (uint32 animIdx = 0; animIdx < static_cast<uint32>(data.Animations.size()); ++animIdx)
			{
				const auto& srcAnim = data.Animations[animIdx];
				const wstring rawAnimName = StrToWStr(srcAnim.Name);
				const wstring animStem = MakeUniqueStem(sourceStem, rawAnimName, L"Anim", animIdx, state.NameCounts);
				const wstring animKey = NormalizePath((animationDir / (animStem + L".bamanim")).wstring());

				if (const EResult result = ValidateImportOutput(manager, animKey, settings.SaveToFile, settings.Overwrite);
					result != EResult::Success)
				{
					return result;
				}

				AnimationCreateDesc animDesc;
				animDesc.Key = animKey;
				animDesc.Path = animKey;
				animDesc.Name = rawAnimName.empty() ? animStem : rawAnimName;
				animDesc.Duration = static_cast<f64>(srcAnim.Duration);
				animDesc.TicksPerSecond = static_cast<f64>(srcAnim.TicksPerSecond);
				animDesc.Tracks.reserve(srcAnim.Tracks.size());

				for (const auto& srcTrack : srcAnim.Tracks)
				{
					AnimationTrack track;
					track.TargetBoneName = StrToWStr(srcTrack.BoneName);

					track.PositionKeyframes.reserve(srcTrack.PositionKeys.size());
					for (const auto& pk : srcTrack.PositionKeys)
					{
						VectorKeyframe kf;
						kf.Time = static_cast<f64>(pk.Time);
						kf.Value = { pk.Value[0], pk.Value[1], pk.Value[2] };
						track.PositionKeyframes.push_back(kf);
					}

					track.RotationKeyframes.reserve(srcTrack.RotationKeys.size());
					for (const auto& rk : srcTrack.RotationKeys)
					{
						QuaternionKeyframe kf;
						kf.Time = static_cast<f64>(rk.Time);
						kf.Value = quat(rk.Value[3], rk.Value[0], rk.Value[1], rk.Value[2]);
						track.RotationKeyframes.push_back(kf);
					}

					track.ScaleKeyframes.reserve(srcTrack.ScaleKeys.size());
					for (const auto& sk : srcTrack.ScaleKeys)
					{
						VectorKeyframe kf;
						kf.Time = static_cast<f64>(sk.Time);
						kf.Value = { sk.Value[0], sk.Value[1], sk.Value[2] };
						track.ScaleKeyframes.push_back(kf);
					}

					animDesc.Tracks.push_back(std::move(track));
				}

				AnimationOwner animation(Animation::Create(&animDesc));
				if (!animation)
				{
					BAM_LOG(Error, "Editor", "Asset", "Animation creation failed: {}", WStrToStr(animKey));
					return EResult::Fail;
				}

				if (settings.SaveToFile)
				{
					const EResult saveResult = manager.SaveToBinaryFile(animation.get(), animKey);
					if (saveResult != EResult::Success)
						return saveResult;
				}

				auto existingAnimHandle = manager.FindHandleByKey(animKey);
				if (settings.Overwrite && manager.IsValid(existingAnimHandle))
				{
					const auto result = ApplyReimport(manager.GetResource(existingAnimHandle), *animation, settings.SaveToFile);
					if (result != EResult::Success)
						return result;

					modelDesc.Animations.push_back(ResourceHandle<Animation>(existingAnimHandle));
				}
				else
				{
					Animation* animInstance = animation.get();
					auto animHandle = manager.AddResource<Animation>(animKey, animation.release());
					if (!animHandle.IsValid() || animHandle.Get() != animInstance)
						return EResult::Fail;

					rollback.Handles.push_back(animHandle.GetRawHandle());
					modelDesc.Animations.push_back(std::move(animHandle));
				}
			}
		}

		if (HasFlag(settings.ImportOptions, EModelImportOption::Materials) && !data.Materials.empty())
		{
			const auto& baseMat = state.BaseMaterial;

			const auto textureDir = modelDesc.UseTextureSubfolder ? outputDir / L"Textures" : outputDir;
			// 텍스처 로드/추출 헬퍼 람다
			auto bindTexture = [&](MaterialInstance& material, uint32 slot,
				const Tools::ModelConversion::TextureBindingInfo& info, const wstring& matStem,
				const wchar_t* slotName) -> EResult
				{
					if (info.Path.empty() && info.EmbeddedData.empty())
						return EResult::Success;

					const bool embedded = info.IsEmbedded && !info.EmbeddedData.empty();
					filesystem::path texFilePath;
					bool wroteEmbeddedFile = false;

					if (embedded)
					{
						const wstring texName = matStem + L"_" + slotName + L".png";
						texFilePath = Engine::PlatformPath::ResolveProjectRelative(textureDir / texName);

						if (!settings.SaveToFile)
						{
							const auto texKey = ResourceManager::ResolveFileKey(texFilePath);
							auto handle = manager.FindHandleByKey(texKey);

							if (manager.IsValid(handle))
							{
								auto* texture = dynamic_cast<Texture*>(manager.GetResource(handle));
								if (!texture)
									return EResult::InvalidArgument;

								if (settings.Overwrite)
								{
									const auto result = texture->ReloadFromEncodedMemory(info.EmbeddedData);
									if (result != EResult::Success)
										return result;
								}
							}
							else
							{
								auto* texture = Texture::CreateFromEncodedMemory(info.EmbeddedData, texKey);
								if (!texture)
									return EResult::Fail;

								const auto registered = manager.AddResource<Texture>(texKey, texture);
								if (!registered.IsValid() || registered.Get() != texture)
									return EResult::Fail;

								handle = registered.GetRawHandle();
								rollback.Handles.push_back(handle);
							}

							material.SetTextureBySlot(slot, ResourceHandle<Texture>(handle));
							return EResult::Success;
						}

						const auto prewritten = state.EmbeddedTextureFilesWritten.find(texFilePath.wstring());
						if (prewritten != state.EmbeddedTextureFilesWritten.end())
						{
							wroteEmbeddedFile = prewritten->second;
						}
						else
						{
							std::error_code ec;
							const bool exists = filesystem::exists(texFilePath, ec);
							const bool shouldWrite = !exists || settings.Overwrite;
							const auto result = ec ? EResult::Fail
								: (shouldWrite ? SaveEmbeddedTexture(texFilePath, info.EmbeddedData) : EResult::Success);

							if (result != EResult::Success)
							{
								BAM_LOG(Error, "Editor", "Asset", "Embedded texture save failed: path={}, result={}",
									WStrToStr(texFilePath.wstring()), ResultToString(result));
								return result;
							}
							wroteEmbeddedFile = shouldWrite;
						}
					}
					else
					{
						texFilePath = ModelImporter::ResolveExternalTexturePath(data.SourcePath, outputDir, info.Path);
						if (texFilePath.empty())
						{
							BAM_LOG(Warn, "Editor", "Asset", "External texture not found: {}", info.Path);
							return EResult::Success;
						}
					}

					if (settings.SaveToFile && settings.BakeTexturesToDDS && !texFilePath.empty())
					{
						const auto ext = texFilePath.extension().string();
						const bool isAlreadyBaked = (_stricmp(ext.c_str(), ".dds") == 0 || _stricmp(ext.c_str(), ".bamtex") == 0);
						if (!isAlreadyBaked)
						{
							const auto resolvedTextureDir = Engine::PlatformPath::ResolveProjectRelative(textureDir);
							filesystem::path bakedBamtexPath = resolvedTextureDir / texFilePath.filename();
							bakedBamtexPath.replace_extension(".bamtex");

							std::error_code checkEc;
							const bool bakedExists = filesystem::exists(bakedBamtexPath, checkEc);
							if (checkEc)
							{
								BAM_LOG(Error, "Editor", "Asset", "Failed to inspect baked texture: path={}, error={}",
									WStrToStr(bakedBamtexPath.wstring()), checkEc.message());
								return EResult::Fail;
							}
							const bool needBake = !bakedExists || settings.Overwrite;

							if (needBake)
							{
								const auto result = state.TextureBakeResults.find(bakedBamtexPath.wstring());
								if (result == state.TextureBakeResults.end())
								{
									BAM_LOG(Error, "Editor", "Asset", "Missing model texture bake result: {}",
										WStrToStr(bakedBamtexPath.wstring()));
									return EResult::Fail;
								}
								if (result->second)
								{
									if (!filesystem::exists(bakedBamtexPath, checkEc) || checkEc)
									{
										BAM_LOG(Error, "Editor", "Asset", "Baked texture output is missing: path={}, error={}",
											WStrToStr(bakedBamtexPath.wstring()), checkEc.message());
										return EResult::Fail;
									}

									texFilePath = bakedBamtexPath;
									wroteEmbeddedFile = true;
								}
							}
							else if (bakedExists)
							{
								texFilePath = bakedBamtexPath;
							}
						}
					}

					const wstring texKey = NormalizePath(texFilePath.wstring());
					auto handle = embedded ? manager.FindHandleByFilePath(texFilePath) : manager.FindHandleByKey(texKey);
					const bool alreadyLoaded = manager.IsValid(handle);
					if (!alreadyLoaded)
						handle = manager.LoadFile(texKey);

					auto* texture = dynamic_cast<Texture*>(manager.GetResource(handle));
					if (!texture)
						return embedded ? EResult::Fail : EResult::Success;

					if (wroteEmbeddedFile && alreadyLoaded)
					{
						const auto result = texture->Reload(texFilePath);
						if (result != EResult::Success)
						{
							BAM_LOG(Error, "Editor", "Asset", "Embedded texture reload failed: path={}, result={}",
								WStrToStr(texFilePath.wstring()), ResultToString(result));
							return result;
						}
					}

					if (wroteEmbeddedFile && AssetCache::IsCreated())
						AssetCache::Get().RequestInvalidate(texFilePath);

					material.SetTextureBySlot(slot, ResourceHandle<Texture>(handle));
					return EResult::Success;
				};

			const auto materialDir = modelDesc.UseMaterialSubfolder ? outputDir / L"Materials" : outputDir;
			for (size_t matIdx = 0; matIdx < data.Materials.size(); ++matIdx)
			{
				const auto& srcMat = data.Materials[matIdx];
				const wstring rawMatName = StrToWStr(srcMat.Name);
				const wstring matStem = MakeUniqueStem(sourceStem, rawMatName, L"Mat", static_cast<uint32>(matIdx), state.NameCounts);
				const wstring matKey = NormalizePath((materialDir / (matStem + L".bammatinst")).wstring());

				if (const EResult result = ValidateImportOutput(manager, matKey, settings.SaveToFile, settings.Overwrite);
					result != EResult::Success)
				{
					return result;
				}

				auto existingMatHandle = manager.FindHandleByKey(matKey);
				if (settings.Overwrite && !settings.OverwriteMaterials)
				{
					bool hasExistingMaterial = manager.IsValid(existingMatHandle);
					if (!hasExistingMaterial && settings.SaveToFile)
					{
						const auto path = Engine::PlatformPath::ResolveUri(matKey);
						std::error_code ec;
						hasExistingMaterial = filesystem::exists(path, ec);
						if (ec)
						{
							BAM_LOG(Error, "Editor", "Asset", "Failed to inspect material: {}, {}", WStrToStr(matKey), ec.message());
							return EResult::Fail;
						}

						if (hasExistingMaterial)
							existingMatHandle = manager.LoadFile(matKey);
					}

					if (hasExistingMaterial)
					{
						if (!dynamic_cast<MaterialInstance*>(manager.GetResource(existingMatHandle)))
						{
							BAM_LOG(Error, "Editor", "Asset", "Failed to reuse existing material: {}", WStrToStr(matKey));
							return EResult::Fail;
						}

						if (matIdx < modelDesc.Materials.size())
							modelDesc.Materials[matIdx] = ResourceHandle<MaterialInterface>(existingMatHandle);

						continue;
					}
				}
				MaterialInstanceDesc matDesc;
				matDesc.Key = matKey;
				matDesc.Path = matKey;
				matDesc.BaseMaterialHandle = baseMat;

				using MaterialInstanceOwner = std::unique_ptr<MaterialInstance, ModelFinalizeState::ResourceDeleter>;
				MaterialInstanceOwner matInst(MaterialInstance::Create(&matDesc));
				if (!matInst)
				{
					BAM_LOG(Error, "Editor", "Asset", "MaterialInstance creation failed: {}", WStrToStr(matKey));
					return EResult::Fail;
				}

				// 슬롯별 텍스처 바인딩
				if (const auto result = bindTexture(*matInst, 0, srcMat.BaseColorTexture, matStem, L"Albedo"); result != EResult::Success)
					return result;
				if (const auto result = bindTexture(*matInst, 1, srcMat.NormalTexture, matStem, L"Normal"); result != EResult::Success)
					return result;
				if (const auto result = bindTexture(*matInst, 2, srcMat.MetallicRoughnessTexture, matStem, L"ORM"); result != EResult::Success)
					return result;
				if (const auto result = bindTexture(*matInst, 3, srcMat.EmissiveTexture, matStem, L"Emissive"); result != EResult::Success)
					return result;

				// PBR 기본 인자 설정 (BaseColor 등)
				matInst->SetFloat4("BaseColor", vec4(srcMat.BaseColorFactor[0], srcMat.BaseColorFactor[1], srcMat.BaseColorFactor[2], srcMat.BaseColorFactor[3]));

				if (settings.SaveToFile)
				{
					const EResult saveResult = manager.SaveToBinaryFile(matInst.get(), matKey);
					if (saveResult != EResult::Success)
					{
						BAM_LOG(Error, "Editor", "Asset", "MaterialInstance save failed: path={}, result={}", WStrToStr(matKey), static_cast<int>(saveResult));
						return saveResult;
					}
				}

				MaterialInstance* instPtr = matInst.get();
				if (settings.Overwrite && manager.IsValid(existingMatHandle))
				{
					const auto result = ApplyReimport(manager.GetResource(existingMatHandle), *matInst, settings.SaveToFile);
					if (result != EResult::Success)
						return result;

					if (matIdx < modelDesc.Materials.size())
						modelDesc.Materials[matIdx] = ResourceHandle<MaterialInterface>(existingMatHandle);

					continue;
				}
				auto matHandle = manager.AddResource<MaterialInstance>(matKey, matInst.release());
				if (!matHandle.IsValid() || matHandle.Get() != instPtr)
				{
					BAM_LOG(Error, "Editor", "Asset", "MaterialInstance registration failed: {}", WStrToStr(matKey));
					return EResult::Fail;
				}

				rollback.Handles.push_back(matHandle.GetRawHandle());
				if (matIdx < modelDesc.Materials.size())
					modelDesc.Materials[matIdx] = ResourceHandle<MaterialInterface>(matHandle);
			}
		}

		ModelOwner model(Model::Create(&modelDesc));
		if (!model)
		{
			BAM_LOG(Error, "Editor", "Asset", "Model creation failed: {}", WStrToStr(modelKey));
			return EResult::Fail;
		}

		if (settings.SaveToFile)
		{
			const EResult result = manager.SaveToBinaryFile(model.get(), modelKey);
			if (result != EResult::Success)
				return result;
		}

		auto existingModelHandle = manager.FindHandleByKey(modelKey);
		if (settings.Overwrite && manager.IsValid(existingModelHandle))
		{
			const auto result = ApplyReimport(manager.GetResource(existingModelHandle), *model, settings.SaveToFile);
			if (result != EResult::Success)
				return result;
		}
		else
		{
			Model* instance = static_cast<Model*>(model.get());
			auto modelHandle = manager.AddResource<Model>(modelKey, static_cast<Model*>(model.release()));

			if (!modelHandle.IsValid())
				return EResult::Fail;

			if (modelHandle.Get() != instance)
				return EResult::AlreadyInitialized;
		}

		rollback.Committed = true;
		state.MeshCommitComplete = true;

		if (settings.SaveToFile)
		{
			const wstring sidecarStem = SanitizeFileName(data.SourcePath.stem().wstring());
			const auto sidecarPath = outputDir / (sidecarStem + L".bamimport.json");
			ModelImporter::SaveImportOptions(sidecarPath, settings);

			const auto bamImportDir = Engine::PlatformPath::ResolveProjectRelative(L".bam/import");
			std::error_code ec;
			std::filesystem::create_directories(bamImportDir, ec);
			ModelImporter::SaveImportOptions(bamImportDir / (sidecarStem + L".bamimport.json"), settings);
		}

		BAM_LOG(Info, "Editor", "Asset", "Model import finished: meshes={}, nodes={}, has_skeleton={}, animations={}, key={}",
			modelDesc.Meshes.size(), modelDesc.Nodes.size(), modelDesc.Skeleton.IsValid(),
			modelDesc.Animations.size(), WStrToStr(modelKey));

		return EResult::Success;
	}

	EResult CheckImportSaveResult(const archive::ArchiveResult& result, const wstring& path)
	{
		if (result)
			return EResult::Success;

		BAM_LOG(Error, "Editor", "Asset", "Model mesh save failed: code={}, field={}, path={}",
			static_cast<int>(result.Code), result.Field, WStrToStr(path));

		return result.Code == archive::ArchiveErrorCode::UnsupportedType ?
			EResult::NotImplemented : EResult::Fail;
	}

	ModelFinalizeState::SavedMesh EncodeAndStageMesh(
		std::unique_ptr<archive::BeveArchive> snapshot, filesystem::path stagingPath)
	{
		using Clock = std::chrono::steady_clock;

		ModelFinalizeState::SavedMesh saved;
		std::string bytes;

		const auto encodeStart = Clock::now();
		saved.Result = snapshot->ToBeve(bytes);
		snapshot.reset();
		saved.WorkerMs = std::chrono::duration<double, std::milli>(Clock::now() - encodeStart).count();
		saved.ByteCount = bytes.size();

		if (saved.Result)
		{
			if (std::cmp_greater(bytes.size(), (std::numeric_limits<std::streamsize>::max)()))
			{
				saved.Result = { archive::ArchiveErrorCode::FileWriteFailed, "Mesh.StagingSize" };
			}
			else
			{
				saved.File = std::make_unique<ModelFinalizeState::StagedFile>();
				saved.File->Path = std::move(stagingPath);

				const auto writeStart = Clock::now();

				{
					std::ofstream file(saved.File->Path, std::ios::out | std::ios::binary | std::ios::noreplace);

					if (!file)
					{
						saved.Result = { archive::ArchiveErrorCode::FileOpenFailed, "Mesh.StagingFile" };
					}
					else
					{
						saved.File->Owned = true;

						if (!bytes.empty())
							file.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));

						file.close();

						if (!file)
							saved.Result = { archive::ArchiveErrorCode::FileWriteFailed, "Mesh.StagingFile" };
					}
				}

				saved.WriteMs = std::chrono::duration<double, std::milli>(Clock::now() - writeStart).count();
			}
		}

		const auto releaseStart = Clock::now();
		std::string{}.swap(bytes);
		saved.ReleaseMs = std::chrono::duration<double, std::milli>(Clock::now() - releaseStart).count();

		return saved;
	}
}

Editor::ModelFinalizeState::~ModelFinalizeState() noexcept
{
	if (MeshCommitComplete || NextPublish == 0)
		return;

	try
	{
		RollbackPublishedMeshes(*this);
	}
	catch (...)
	{
		for (size_t i = 0; i < NextPublish; ++i)
		{
			if (auto& backup = StagedMeshes[i].Backup)
				backup->Keep = true;
		}

		std::fputs("Model import shutdown rollback interrupted. Remaining mesh backups were kept.\n", stderr);
	}
}

EResult ModelImporter::Import(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg)
{
	std::unique_ptr<ImportJob> job;
	const EResult createResult = CreateJob(sourcePath, destDir, arg, job);
	if (createResult != EResult::Success)
		return createResult;

	auto* task = dynamic_cast<ModelImportTask*>(job.get());
	if (!task)
		return EResult::InvalidArgument;

	const EResult prepareResult = task->Prepare();
	if (prepareResult != EResult::Success)
	{
		BAM_LOG(Error, "Editor", "Asset", "Model preparation failed: code={}, source={}, error={}",
			static_cast<int>(task->GetErrorCode()), sourcePath.string(), task->GetErrorMessage());
		return prepareResult;
	}

	return Finalize(*task);
}

EResult ModelImporter::Finalize(ModelImportTask& task)
{
	for (;;)
	{
		const auto result = FinalizeStep(task);
		if (result)
			return *result;

		if (task.m_FinalizeState && task.m_FinalizeState->Saving.valid())
			task.m_FinalizeState->Saving.wait();
	}
}

std::optional<EResult> ModelImporter::FinalizeStep(ModelImportTask& task)
{
	using Clock = std::chrono::steady_clock;
	const auto deadline = Clock::now() + std::chrono::milliseconds(2);

	do
	{
		const auto result = FinalizeOneStep(task);
		if (result)
			return result;

		const auto& state = task.m_FinalizeState;
		if (!state)
			return EResult::Fail;

		// 작업 스레드를 기다리며 반복하지 않고 다음 프레임으로 넘깁니다.
		if (state->Saving.valid() &&
			state->Saving.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
		{
			return std::nullopt;
		}
	} while (Clock::now() < deadline);

	return std::nullopt;
}

std::optional<EResult> ModelImporter::FinalizeOneStep(ModelImportTask& task)
{
	using Clock = std::chrono::steady_clock;
	const auto finish = [&task](EResult result) -> std::optional<EResult>
		{
			if (result != EResult::Success && task.m_FinalizeState && !task.m_FinalizeState->MeshCommitComplete)
			{
				RollbackPublishedMeshes(*task.m_FinalizeState);
				RollbackReloadedMeshes(*task.m_FinalizeState);
			}

			task.m_FinalizeState.reset();
			return result;
		};

	try
	{
		auto* prepared = task.GetPreparedModel();
		if (!prepared)
			return finish(EResult::InvalidArgument);

		auto& data = *prepared;
		const auto& request = task.GetRequest();
		const auto& outputDir = request.DestinationDirectory;
		auto& manager = ResourceManager::Get();

		if (!task.m_FinalizeState)
		{
			if (data.Nodes.empty() || data.Meshes.empty() || outputDir.empty())
				return finish(EResult::InvalidArgument);

			task.m_FinalizeState = std::make_shared<ModelFinalizeState>();
			auto& state = *task.m_FinalizeState;
			state.Settings = request.Settings;
			state.ModelDisplayName = WStrToStr(data.SourcePath.filename().wstring());

			// Phase 6: 하이브리드 모델 에셋 서브폴더 레이아웃 계산
			state.Layout = Engine::ModelSubfolderHelper::ResolveLayout(
				outputDir,
				data.SourcePath.stem().wstring(),
				state.Settings.CreateModelSubfolder,
				state.Settings.UseMeshSubfolder,
				state.Settings.UseMaterialSubfolder,
				state.Settings.UseTextureSubfolder,
				state.Settings.UseAnimationSubfolder);

			if (const auto result = state.Output.Acquire(state.Layout.RootDirectory); result != EResult::Success)
				return finish(result);

			auto& desc = state.ModelDesc;
			const wstring stem = state.Layout.ModelStem;
			desc.Key = NormalizePath(state.Layout.ModelFilePath.wstring());
			desc.Path = desc.Key;
			const auto& rotation = state.Settings.RotationOffset;
			desc.PlacementEulerDegrees = vec3(rotation[0], rotation[1], rotation[2]);
			const auto sourceUri = Engine::PlatformPath::ToVirtualUri(data.SourcePath);
			if (Engine::PlatformPath::IsProjectUri(sourceUri))
				desc.ImportSourceUri = sourceUri;

			if (const EResult result = ValidateImportOutput(manager, desc.Key, state.Settings.SaveToFile, state.Settings.Overwrite);
				result != EResult::Success)
			{
				return finish(result);
			}

			desc.UseMeshSubfolder = state.Layout.UsesMeshSubfolder;
			desc.UseMaterialSubfolder = state.Layout.UsesMaterialSubfolder;
			desc.UseAnimationSubfolder = state.Layout.UsesAnimationSubfolder;
			desc.UseTextureSubfolder = state.Layout.UsesTextureSubfolder;


			auto modelHandle = manager.FindHandleByKey(desc.Key);
			bool hasExistingModel = manager.IsValid(modelHandle);

			if (!hasExistingModel && state.Settings.SaveToFile)
			{
				auto modelPath = Engine::PlatformPath::ResolveUri(desc.Key);
				std::error_code ec;
				hasExistingModel = filesystem::exists(modelPath, ec);
				if (ec)
					return finish(EResult::Fail);

				if (hasExistingModel)
				{
					modelHandle = manager.LoadFile(desc.Key);
				}
				else
				{
					modelPath += L".json";
					desc.UseMeshSubfolder = !filesystem::exists(modelPath, ec);
					desc.UseMaterialSubfolder = desc.UseMeshSubfolder;
					desc.UseAnimationSubfolder = desc.UseMeshSubfolder;
					desc.UseTextureSubfolder = desc.UseMeshSubfolder;
					if (ec)
						return finish(EResult::Fail);
				}
			}

			if (hasExistingModel)
			{
				const auto* existingModel = dynamic_cast<Model*>(manager.GetResource(modelHandle));
				if (!existingModel)
				{
					BAM_LOG(Error, "Editor", "Asset", "Failed to read existing model layout: {}", WStrToStr(desc.Key));
					return finish(EResult::Fail);
				}

				desc.UseMeshSubfolder = existingModel->UsesMeshSubfolder();
				desc.UseMaterialSubfolder = existingModel->UsesMaterialSubfolder();
				desc.UseAnimationSubfolder = existingModel->UsesAnimationSubfolder();
				desc.UseTextureSubfolder = existingModel->UsesTextureSubfolder();
			}

			state.MeshDirectory = desc.UseMeshSubfolder ? state.Layout.MeshesDirectory : state.Layout.RootDirectory;

			const auto options = state.Settings.ImportOptions;
			if (HasFlag(options, EModelImportOption::Materials) && !data.Materials.empty())
			{
				const bool importsSkeleton = HasFlag(options, EModelImportOption::Skeleton) && !data.Bones.empty();
				const wstring baseMatPath = importsSkeleton
					? L"Resources/Material/SkinningMaterial.bammat.json"
					: L"Resources/Material/DefaultMaterial.bammat.json";

				auto handle = manager.FindHandleByFilePath(baseMatPath);
				if (!manager.IsValid(handle))
					handle = manager.LoadFile(baseMatPath);

				if (!dynamic_cast<Material*>(manager.GetResource(handle)))
				{
					BAM_LOG(Error, "Editor", "Asset", "Required base material unavailable: {}", WStrToStr(baseMatPath));
					return finish(EResult::Fail);
				}

				state.BaseMaterial = ResourceHandle<Material>(handle);
			}

			desc.Nodes.reserve(data.Nodes.size());
			desc.Materials.resize(data.MaterialCount);
			desc.MeshMaterialIndices.reserve(data.Meshes.size());
			desc.MeshLODs.resize(data.Meshes.size());
			state.Meshes.reserve(data.Meshes.size() * 3);

			for (const auto& mesh : data.Meshes)
				desc.MeshMaterialIndices.push_back(mesh.MaterialIndex);

			for (const auto& source : data.Nodes)
			{
				ModelNode node;
				node.Name = StrToWStr(source.Name);
				node.ParentIndex = source.ParentIndex;
				node.MeshIndices.assign(source.MeshIndices.begin(), source.MeshIndices.end());
				node.LocalTransform = ArrayToGlmMat4(source.LocalTransform);
				desc.Nodes.push_back(std::move(node));
			}

			AssignModelSourceKeys(data, desc);

			if (HasFlag(options, EModelImportOption::Skeleton) && !data.Bones.empty())
			{
				state.SkeletonBones.reserve(data.Bones.size());
				for (const auto& srcBone : data.Bones)
				{
					Bone bone;
					bone.Name = StrToWStr(srcBone.Name);
					bone.ParentIndex = srcBone.ParentIndex;
					bone.OffsetMatrix = ArrayToGlmMat4(srcBone.OffsetMatrix);
					bone.LocalTransform = ArrayToGlmMat4(srcBone.LocalTransform);
					state.SkeletonBones.push_back(std::move(bone));
				}

				state.SkinBindingSignature = Skeleton::ComputeSkinBindingSignature(state.SkeletonBones);
			}

			if (state.Settings.SaveToFile)
			{
				std::error_code ec;
				filesystem::create_directories(Engine::PlatformPath::ResolveProjectRelative(state.MeshDirectory), ec);
				if (ec)
					return finish(EResult::Fail);
			}
		}

		// finish()에서 작업의 참조를 해제해도 현재 호출이 끝날 때까지 상태를 유지합니다.
		auto state = task.m_FinalizeState;

		if (state->NextMesh < data.Meshes.size())
		{
			auto& source = data.Meshes[state->NextMesh];
			const auto vertexCount = source.Positions.size();
			const auto indexCount = source.Indices.size();
			const bool hasSkinWeights = !source.SkinWeights.empty();
			constexpr auto maxBytes = (std::numeric_limits<uint32>::max)();

			if (source.Attributes.size() != vertexCount ||
				(hasSkinWeights && source.SkinWeights.size() != vertexCount) ||
				vertexCount > maxBytes / sizeof(VertexPosition) ||
				vertexCount > maxBytes / sizeof(VertexMaterial) ||
				(hasSkinWeights && vertexCount > maxBytes / sizeof(VertexSkinData)) ||
				indexCount > maxBytes / sizeof(uint32))
			{
				return finish(EResult::InvalidArgument);
			}

			const wstring stem = MakeUniqueStem(data.SourcePath.stem().wstring(),
				StrToWStr(source.Name), L"Mesh", static_cast<uint32>(state->NextMesh), state->NameCounts);
			const wstring key = NormalizePath((state->MeshDirectory / (stem + L".bammesh")).wstring());

			if (const EResult result = ValidateImportOutput(manager, key, state->Settings.SaveToFile, state->Settings.Overwrite);
				result != EResult::Success)
			{
				return finish(result);
			}

			MeshCreateDesc desc;
			desc.Key = key;
			desc.Path = key;
			desc.Streams[static_cast<uint32>(EMeshStream::Position)] = {
				source.Positions.data(), static_cast<uint32>(vertexCount), sizeof(VertexPosition)
			};
			desc.Streams[static_cast<uint32>(EMeshStream::Material)] = {
				source.Attributes.data(), static_cast<uint32>(vertexCount), sizeof(VertexMaterial)
			};
			if (hasSkinWeights)
			{
				desc.Flags |= EMeshFlag::Dynamic;
				desc.Streams[static_cast<uint32>(EMeshStream::SkinData)] = {
					source.SkinWeights.data(), static_cast<uint32>(vertexCount), sizeof(VertexSkinData)
				};
				desc.SkinBindingSignature = state->SkinBindingSignature;
			}
			desc.IndexData = source.Indices.data();
			desc.IndexCount = static_cast<uint32>(indexCount);
			desc.IndexStride = sizeof(uint32);
			desc.BoundingBoxMin = source.BoundsMin;
			desc.BoundingBoxMax = source.BoundsMax;

			const auto createStart = Clock::now();
			ModelFinalizeState::MeshOwner mesh(Mesh::Create(&desc));
			const double createMs = std::chrono::duration<double, std::milli>(Clock::now() - createStart).count();

			const auto vertexBytes = static_cast<std::uint64_t>(vertexCount) *
				(sizeof(VertexPosition) + sizeof(VertexMaterial) + (hasSkinWeights ? sizeof(VertexSkinData) : 0));
			const auto indexBytes = static_cast<std::uint64_t>(indexCount) * sizeof(uint32);

			BAM_LOG(Info, "Editor", "Asset",
				"Model mesh create timing: key={}, vertices={}, indices={}, skinned={}, input_bytes={}, create_ms={:.3f}, success={}",
				WStrToStr(key), vertexCount, indexCount, hasSkinWeights, vertexBytes + indexBytes, createMs, static_cast<bool>(mesh));

			if (!mesh)
				return finish(EResult::Fail);

			state->Meshes.push_back({ key, std::move(mesh), state->NextMesh, 0 });

			// 파생 LOD 메시가 있는 경우 LOD 1, LOD 2 메시도 생성 및 등록
			for (size_t lodIdx = 0; lodIdx < source.LODs.size(); ++lodIdx)
			{
				const auto& lodSource = source.LODs[lodIdx];
				const auto lodVertexCount = lodSource.Positions.size();
				const auto lodIndexCount = lodSource.Indices.size();
				if (lodVertexCount == 0 || lodIndexCount == 0)
					continue;

				const bool lodHasSkinWeights = !lodSource.SkinWeights.empty();
				const wstring lodName = stem + L"_LOD" + std::to_wstring(lodIdx + 1) + L".bammesh";
				const wstring lodKey = NormalizePath((state->MeshDirectory / lodName).wstring());

				if (const EResult result = ValidateImportOutput(manager, lodKey, state->Settings.SaveToFile, state->Settings.Overwrite);
					result != EResult::Success)
				{
					return finish(result);
				}

				MeshCreateDesc lodDesc;
				lodDesc.Key = lodKey;
				lodDesc.Path = lodKey;
				lodDesc.Streams[static_cast<uint32>(EMeshStream::Position)] = {
					const_cast<VertexPosition*>(lodSource.Positions.data()), static_cast<uint32>(lodVertexCount), sizeof(VertexPosition)
				};
				lodDesc.Streams[static_cast<uint32>(EMeshStream::Material)] = {
					const_cast<VertexMaterial*>(lodSource.Attributes.data()), static_cast<uint32>(lodVertexCount), sizeof(VertexMaterial)
				};
				if (lodHasSkinWeights)
				{
					lodDesc.Flags |= EMeshFlag::Dynamic;
					lodDesc.Streams[static_cast<uint32>(EMeshStream::SkinData)] = {
						const_cast<VertexSkinData*>(lodSource.SkinWeights.data()), static_cast<uint32>(lodVertexCount), sizeof(VertexSkinData)
					};
					lodDesc.SkinBindingSignature = state->SkinBindingSignature;
				}
				lodDesc.IndexData = const_cast<uint32*>(lodSource.Indices.data());
				lodDesc.IndexCount = static_cast<uint32>(lodIndexCount);
				lodDesc.IndexStride = sizeof(uint32);
				lodDesc.BoundingBoxMin = lodSource.BoundsMin;
				lodDesc.BoundingBoxMax = lodSource.BoundsMax;

				ModelFinalizeState::MeshOwner lodMeshOwner(Mesh::Create(&lodDesc));
				if (lodMeshOwner)
				{
					state->Meshes.push_back({ lodKey, std::move(lodMeshOwner), state->NextMesh, static_cast<uint32>(lodIdx + 1) });
				}
			}

			++state->NextMesh;
			return std::nullopt;
		}

		if (state->Settings.SaveToFile && state->StagedMeshes.size() < state->Meshes.size())
		{
			const auto& mesh = state->Meshes[state->StagedMeshes.size()];
			const auto meshPath = Engine::PlatformPath::ResolveProjectRelative(filesystem::path(mesh.Key));

			if (!state->Saving.valid())
			{
				if (const EResult result = ValidateImportOutput(manager, mesh.Key, true, state->Settings.Overwrite);
					result != EResult::Success)
				{
					return finish(result);
				}

				auto snapshot = std::make_unique<archive::BeveArchive>();
				const auto snapshotStart = Clock::now();

				const auto archiveResult = mesh.Resource->WriteArchive(*snapshot);
				state->SnapshotMs = std::chrono::duration<double, std::milli>(
					Clock::now() - snapshotStart).count();

				if (const EResult result = CheckImportSaveResult(archiveResult, mesh.Key);
					result != EResult::Success)
				{
					return finish(result);
				}

				// 이 함수는 메인 스레드에서 실행합니다.
				auto stagingPath = MakeImportStagingPath(meshPath);

				state->Saving = std::async(
					std::launch::async, EncodeAndStageMesh, std::move(snapshot), std::move(stagingPath));

				return std::nullopt;
			}

			if (state->Saving.wait_for(std::chrono::seconds(0)) != std::future_status::ready)
				return std::nullopt;

			auto saved = state->Saving.get();

			if (const EResult result = CheckImportSaveResult(saved.Result, mesh.Key);
				result != EResult::Success)
			{
				return finish(result);
			}

			if (!saved.File || !saved.File->Owned)
				return finish(EResult::Fail);

			saved.SnapshotMs = state->SnapshotMs;
			state->StagedMeshes.push_back(std::move(saved));
			return std::nullopt;
		}

		if (state->Settings.SaveToFile && state->NextPublish < state->StagedMeshes.size())
		{
			if (state->NextPublish == 0)
			{
				for (const auto& pendingMesh : state->Meshes)
				{
					const auto result = ValidateImportOutput(manager, pendingMesh.Key, true, state->Settings.Overwrite);
					if (result != EResult::Success)
						return finish(result);
				}
			}
			const auto& mesh = state->Meshes[state->NextPublish];
			auto& saved = state->StagedMeshes[state->NextPublish];
			const auto meshPath = Engine::PlatformPath::ResolveProjectRelative(filesystem::path(mesh.Key));

			// 여기서 기존 publishStart 선언과 파일 교체 코드가 이어집니다.

			const auto publishStart = Clock::now();

			// 저장하는 동안 다른 요청이 최종 출력을 만들었는지 다시 검사합니다.
			if (const EResult result = ValidateImportOutput(manager, mesh.Key, true, state->Settings.Overwrite);
				result != EResult::Success)
			{
				return finish(result);
			}

			std::error_code ec;
			saved.PublishedPath = meshPath;
			if (!BackupMeshOutput(saved, ec))
			{
				BAM_LOG(Error, "Editor", "Asset", "Failed to back up mesh: path={}, error={}",
					WStrToStr(meshPath.wstring()), ec.message());
				return finish(EResult::Fail);
			}
			filesystem::rename(saved.File->Path, meshPath, ec);

			if (ec)
			{
				BAM_LOG(Error, "Editor", "Asset",
					"Failed to publish imported mesh: path={}, error={}", WStrToStr(mesh.Key), ec.message());
				return finish(EResult::Fail);
			}

			saved.File->Owned = false;
			++state->NextPublish;

			if (HotReloadManager::IsCreated())
				HotReloadManager::Get().GetAssetReloader().RecordMeshFileState(meshPath);

			const double publishMs =
				std::chrono::duration<double, std::milli>(Clock::now() - publishStart).count();

			BAM_LOG(Info, "Editor", "Asset",
				"Model mesh save timing: path={}, bytes={}, snapshot_ms={:.3f}, worker_ms={:.3f}, "
				"write_ms={:.3f}, release_ms={:.3f}, publish_ms={:.3f}",
				WStrToStr(mesh.Key), saved.ByteCount, saved.SnapshotMs, saved.WorkerMs,
				saved.WriteMs, saved.ReleaseMs, publishMs);

			return std::nullopt;
		}

		const auto& modelRootDir = state->Layout.RootDirectory.empty()
			? outputDir : state->Layout.RootDirectory;

		if (!state->TextureBakesScheduled)
		{
			if (const EResult result = ScheduleModelTextureBakes(*state, data, modelRootDir);
				result != EResult::Success)
			{
				return finish(result);
			}
		}

		if (!state->TextureBakesComplete && !PollModelTextureBakes(*state))
			return std::nullopt;

		const auto commitStart = Clock::now();
		const EResult commitResult = CommitPreparedModel(*state, data, modelRootDir);
		const double commitMs = std::chrono::duration<double, std::milli>(Clock::now() - commitStart).count();

		BAM_LOG(Info, "Editor", "Asset",
			"Model commit timing: key={}, meshes={}, nodes={}, commit_ms={:.3f}, result={}",
			WStrToStr(state->ModelDesc.Key), state->Meshes.size(), state->ModelDesc.Nodes.size(),
			commitMs, static_cast<int>(commitResult));

		return finish(commitResult);
	}
	catch (...)
	{
		finish(EResult::Fail);
		throw;
	}
}

Tools::ModelConversion::Options ModelImporter::MakeConversionOptions(const ModelImportDesc& desc)
{
	Tools::ModelConversion::Options options;
	options.FlipUVs = HasFlag(desc.PostProcessFlags, EModelPostProcessFlag::FlipUVs);
	options.ConvertToLeftHanded = HasFlag(desc.PostProcessFlags, EModelPostProcessFlag::MakeLeftHanded);
	options.FlipWindingOrder = HasFlag(desc.PostProcessFlags, EModelPostProcessFlag::FlipWindingOrder);
	options.UniformScale = desc.UniformScale;
	options.FlipPitch = desc.FlipPitch;
	options.FlipHeading = desc.FlipHeading;
	options.GenerateLODs = desc.GenerateLODs;
	options.LODCount = desc.LODCount;
	options.LODReductionRates = { desc.LOD1Reduction, desc.LOD2Reduction };
	options.TargetError = desc.TargetError;
	return options;
}

EResult ModelImporter::CreateJob(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg, std::unique_ptr<ImportJob>& outJob)
{
	if (outJob || sourcePath.empty())
		return EResult::InvalidArgument;

	ModelImportDesc settings;
	if (arg)
	{
		settings = *static_cast<const ModelImportDesc*>(arg);
	}
	else
	{
		const auto sidecar = ResolveSidecarPath(sourcePath, destDir);
		if (sidecar.empty() || !LoadImportOptions(sidecar, settings))
			settings = ModelImportDesc{};
	}

	if (settings.PostProcessFlags == EModelPostProcessFlag::None)
		settings.PostProcessFlags = EModelPostProcessFlag::Triangulate | EModelPostProcessFlag::FlipUVs;

	// 현재 변환 모듈에서 처리할 수 있는 후처리 설정만 받습니다.
	const auto supportedFlags = EModelPostProcessFlag::Triangulate | EModelPostProcessFlag::FlipUVs |
		EModelPostProcessFlag::GenSmoothNormals | EModelPostProcessFlag::CalcTangentSpace |
		EModelPostProcessFlag::LimitBoneWeights |
		EModelPostProcessFlag::MakeLeftHanded | EModelPostProcessFlag::FlipWindingOrder;

	const auto requestedFlags = static_cast<uint32>(settings.PostProcessFlags);
	if ((requestedFlags & ~static_cast<uint32>(supportedFlags)) != 0)
	{
		BAM_LOG(Error, "Editor", "Asset", "Unsupported model post process flags: requested=0x{:08X}, supported=0x{:08X}",
			requestedFlags, static_cast<uint32>(supportedFlags));
		return EResult::NotImplemented;
	}
	const auto supportedImportOptions = EModelImportOption::Meshes |
		EModelImportOption::Skeleton | EModelImportOption::Animations | EModelImportOption::Materials;
	const auto requestedOptions = static_cast<uint32>(settings.ImportOptions);
	if (!HasFlag(settings.ImportOptions, EModelImportOption::Meshes) ||
		(requestedOptions & ~static_cast<uint32>(supportedImportOptions)) != 0)
	{
		BAM_LOG(Error, "Editor", "Asset", "Unsupported model import options: requested=0x{:08X}, supported=0x{:08X}",
			requestedOptions, static_cast<uint32>(supportedImportOptions));
		return EResult::NotImplemented;
	}

	std::error_code ec;
	auto inputPath = filesystem::absolute(sourcePath, ec).lexically_normal();
	if (ec)
		return EResult::InvalidArgument;

	filesystem::path outputDirectory;
	if (destDir.empty())
	{
		outputDirectory = inputPath.parent_path();
	}
	else
	{
		outputDirectory = Engine::PlatformPath::ToProjectRelative(destDir);
		if (outputDirectory.empty())
		{
			outputDirectory = inputPath.parent_path();
		}
		if (ec)
			return EResult::InvalidArgument;
	}

	ModelImportRequest request;
	request.SourcePath = std::move(inputPath);
	request.DestinationDirectory = std::move(outputDirectory);
	request.Settings = settings;

	outJob = std::make_unique<ModelImportTask>(std::move(request));
	return EResult::Success;
}

std::filesystem::path ModelImporter::ResolveExternalTexturePath(const std::filesystem::path& sourcePath,
	const std::filesystem::path& destinationDirectory, std::string_view texturePathUtf8)
{
	namespace fs = std::filesystem;

	if (sourcePath.empty() || texturePathUtf8.empty()) return {};

	try
	{
		const fs::path reference = Engine::Utf8ToPath(texturePathUtf8);

		const auto isFile = [](const fs::path& path)
			{
				std::error_code error;
				return fs::is_regular_file(path, error);
			};

		if (reference.is_absolute())
			return isFile(reference) ? reference.lexically_normal() : fs::path{};

		const auto sourceDirectory = Engine::PlatformPath::ResolveProjectRelative(sourcePath).parent_path();
		fs::path candidate = (sourceDirectory / reference).lexically_normal();

		if (isFile(candidate))
			return candidate;

		if (!destinationDirectory.empty())
		{
			const auto outputDirectory = Engine::PlatformPath::ResolveProjectRelative(destinationDirectory);
			candidate = (outputDirectory / reference).lexically_normal();

			if (isFile(candidate))
				return candidate;
		}
	}
	catch (const fs::filesystem_error& exception)
	{
		BAM_LOG(Warn, "Editor", "Asset", "External texture path resolution failed: {}", exception.what());
	}

	return {};
}

bool ModelImporter::SaveImportOptions(const std::filesystem::path& sidecarPath, const ModelImportDesc& desc)
{
	try
	{
		std::error_code ec;
		std::filesystem::create_directories(sidecarPath.parent_path(), ec);

		archive::JsonArchive ar;
		ar.Write("DccPreset", static_cast<uint32>(desc.DccPreset));
		ar.Write("PostProcessFlags", static_cast<uint32>(desc.PostProcessFlags));
		ar.Write("ImportOptions", static_cast<uint32>(desc.ImportOptions));
		ar.Write("UniformScale", desc.UniformScale);
		ar.Write("RotationX", desc.RotationOffset[0]);
		ar.Write("RotationY", desc.RotationOffset[1]);
		ar.Write("RotationZ", desc.RotationOffset[2]);
		ar.Write("SaveToFile", desc.SaveToFile);
		ar.Write("Overwrite", desc.Overwrite);
		ar.Write("OverwriteMaterials", desc.OverwriteMaterials);
		ar.Write("BakeTexturesToDDS", desc.BakeTexturesToDDS);
		ar.Write("TextureCompression", static_cast<uint32>(desc.TextureCompression));
		ar.Write("FlipPitch", desc.FlipPitch);
		ar.Write("FlipHeading", desc.FlipHeading);
		ar.Write("GenerateLODs", desc.GenerateLODs);
		ar.Write("LODCount", desc.LODCount);
		ar.Write("LOD1Reduction", desc.LOD1Reduction);
		ar.Write("LOD2Reduction", desc.LOD2Reduction);
		ar.Write("TargetError", desc.TargetError);
		ar.Write("CreateModelSubfolder", desc.CreateModelSubfolder);
		ar.Write("UseMeshSubfolder", desc.UseMeshSubfolder);
		ar.Write("UseMaterialSubfolder", desc.UseMaterialSubfolder);
		ar.Write("UseTextureSubfolder", desc.UseTextureSubfolder);
		ar.Write("UseAnimationSubfolder", desc.UseAnimationSubfolder);

		std::string jsonStr;
		if (!ar.ToJson(jsonStr))
			return false;

		std::ofstream file(sidecarPath, std::ios::out | std::ios::trunc);
		if (!file.is_open())
			return false;

		file << jsonStr;
		return true;
	}
	catch (...)
	{
		return false;
	}
}

bool ModelImporter::LoadImportOptions(const std::filesystem::path& sidecarPath, ModelImportDesc& outDesc)
{
	try
	{
		std::error_code ec;
		if (!std::filesystem::is_regular_file(sidecarPath, ec))
			return false;

		std::ifstream file(sidecarPath, std::ios::in);
		if (!file.is_open())
			return false;

		std::string jsonStr((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		file.close();

		if (jsonStr.empty())
			return false;

		archive::JsonArchive ar;
		if (!ar.Parse(jsonStr))
			return false;

		uint32 dccPreset = 0;
		if (ar.Read("DccPreset", dccPreset))
			outDesc.DccPreset = static_cast<EModelImportDccPreset>(dccPreset);

		uint32 flags = 0;
		if (ar.Read("PostProcessFlags", flags))
			outDesc.PostProcessFlags = static_cast<EModelPostProcessFlag>(flags);

		uint32 options = 0;
		if (ar.Read("ImportOptions", options))
			outDesc.ImportOptions = static_cast<EModelImportOption>(options);

		float scale = 1.0f;
		if (ar.Read("UniformScale", scale))
			outDesc.UniformScale = scale;

		float rotX = 0.0f, rotY = 0.0f, rotZ = 0.0f;
		if (ar.Read("RotationX", rotX)) outDesc.RotationOffset[0] = rotX;
		if (ar.Read("RotationY", rotY)) outDesc.RotationOffset[1] = rotY;
		if (ar.Read("RotationZ", rotZ)) outDesc.RotationOffset[2] = rotZ;

		bool bVal = false;
		if (ar.Read("SaveToFile", bVal)) outDesc.SaveToFile = bVal;
		if (ar.Read("Overwrite", bVal)) outDesc.Overwrite = bVal;
		if (ar.Read("OverwriteMaterials", bVal)) outDesc.OverwriteMaterials = bVal;
		if (ar.Read("BakeTexturesToDDS", bVal)) outDesc.BakeTexturesToDDS = bVal;

		uint32 compression = 0;
		if (ar.Read("TextureCompression", compression))
			outDesc.TextureCompression = static_cast<ETextureCompressionMode>(compression);

		if (ar.Read("FlipPitch", bVal)) outDesc.FlipPitch = bVal;
		if (ar.Read("FlipHeading", bVal)) outDesc.FlipHeading = bVal;
		if (ar.Read("GenerateLODs", bVal)) outDesc.GenerateLODs = bVal;

		uint32 lodCount = 3;
		if (ar.Read("LODCount", lodCount)) outDesc.LODCount = lodCount;

		float lod1 = 0.5f, lod2 = 0.25f, error = 0.02f;
		if (ar.Read("LOD1Reduction", lod1)) outDesc.LOD1Reduction = lod1;
		if (ar.Read("LOD2Reduction", lod2)) outDesc.LOD2Reduction = lod2;
		if (ar.Read("TargetError", error)) outDesc.TargetError = error;

		if (ar.Read("CreateModelSubfolder", bVal)) outDesc.CreateModelSubfolder = bVal;
		if (ar.Read("UseMeshSubfolder", bVal)) outDesc.UseMeshSubfolder = bVal;
		if (ar.Read("UseMaterialSubfolder", bVal)) outDesc.UseMaterialSubfolder = bVal;
		if (ar.Read("UseTextureSubfolder", bVal)) outDesc.UseTextureSubfolder = bVal;
		if (ar.Read("UseAnimationSubfolder", bVal)) outDesc.UseAnimationSubfolder = bVal;

		return true;
	}
	catch (...)
	{
		return false;
	}
}

std::filesystem::path ModelImporter::ResolveSidecarPath(const std::filesystem::path& modelOrSourcePath, const std::filesystem::path& destDir)
{
	std::error_code ec;
	const wstring stem = SanitizeFileName(modelOrSourcePath.stem().wstring());

	std::vector<std::filesystem::path> candidates;
	if (!destDir.empty())
	{
		candidates.push_back(destDir / (stem + L".bamimport.json"));
		candidates.push_back(destDir / stem / (stem + L".bamimport.json"));
		candidates.push_back(Engine::PlatformPath::ResolveProjectRelative(destDir / (stem + L".bamimport.json")));
		candidates.push_back(Engine::PlatformPath::ResolveProjectRelative(destDir / stem / (stem + L".bamimport.json")));
	}
	candidates.push_back(modelOrSourcePath.parent_path() / (stem + L".bamimport.json"));
	candidates.push_back(Engine::PlatformPath::ResolveProjectRelative(std::filesystem::path(".bam/import") / (stem + L".bamimport.json")));

	for (const auto& cand : candidates)
	{
		if (std::filesystem::is_regular_file(cand, ec))
			return cand;
	}
	return {};
}
