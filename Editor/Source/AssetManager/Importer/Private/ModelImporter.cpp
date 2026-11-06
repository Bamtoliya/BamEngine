#pragma once

#include "Model.h"
#include "ModelImporter.h"
#include "ModelImportTask.h"
#include "ModelConverter.h"
#include "ResourceManager.h"
#include "Logger.h"
#include "PlatformPath.h"

#pragma push_macro("new")
#undef new
#include <archive/BeveArchive.h>
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
		};

		OutputReservation Output;
		ModelImportDesc Settings;
		ModelCreateDesc ModelDesc;
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
	};
}

namespace
{
	wstring SanitizeFileName(wstring name)
	{
		static const wstring invalid = L"\\/:*?\"<>|";
		for (wchar_t& ch : name)
		{
			if (invalid.find(ch) != wstring::npos)
				ch = L'_';
		}
		return name;
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

		std::error_code ec;
		filesystem::rename(staged.Path, target, ec);
		if (ec)
			return EResult::Fail;

		staged.Owned = false;
		return EResult::Success;
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
		modelDesc.Meshes.reserve(pending.size());

		for (auto& mesh : pending)
		{
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

				modelDesc.Meshes.push_back(ResourceHandle<Mesh>(existingHandle));
				continue;
			}

			Mesh* instance = mesh.Resource.get();
			auto handle = manager.AddResource<Mesh>(mesh.Key, mesh.Resource.release());

			if (!handle.IsValid())
				return EResult::Fail;

			if (handle.Get() != instance)
				return EResult::AlreadyInitialized;

			rollback.Handles.push_back(handle.GetRawHandle());
			modelDesc.Meshes.push_back(std::move(handle));
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
			skelDesc.Bones.reserve(data.Bones.size());

			for (const auto& srcBone : data.Bones)
			{
				Bone bone;
				bone.Name = StrToWStr(srcBone.Name);
				bone.ParentIndex = srcBone.ParentIndex;
				bone.OffsetMatrix = ArrayToGlmMat4(srcBone.OffsetMatrix);
				bone.LocalTransform = ArrayToGlmMat4(srcBone.LocalTransform);
				skelDesc.Bones.push_back(std::move(bone));
			}

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

		// 애니메이션 클립 생성 및 저장 (.bamanim)
		if (HasFlag(settings.ImportOptions, EModelImportOption::Animations) && !data.Animations.empty())
		{
			modelDesc.Animations.reserve(data.Animations.size());

			for (uint32 animIdx = 0; animIdx < static_cast<uint32>(data.Animations.size()); ++animIdx)
			{
				const auto& srcAnim = data.Animations[animIdx];
				const wstring rawAnimName = StrToWStr(srcAnim.Name);
				const wstring animStem = MakeUniqueStem(sourceStem, rawAnimName, L"Anim", animIdx, state.NameCounts);
				const wstring animKey = NormalizePath((outputDir / (animStem + L".bamanim")).wstring());

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
						texFilePath = Engine::PlatformPath::ResolveProjectRelative(outputDir / (matStem + L"_" + slotName + L".png"));

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
					else
					{
						texFilePath = ModelImporter::ResolveExternalTexturePath(data.SourcePath, outputDir, info.Path);
						if (texFilePath.empty())
						{
							BAM_LOG(Warn, "Editor", "Asset", "External texture not found: {}", info.Path);
							return EResult::Success;
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

			for (size_t matIdx = 0; matIdx < data.Materials.size(); ++matIdx)
			{
				const auto& srcMat = data.Materials[matIdx];
				const wstring rawMatName = StrToWStr(srcMat.Name);
				const wstring matStem = MakeUniqueStem(sourceStem, rawMatName, L"Mat", static_cast<uint32>(matIdx), state.NameCounts);
				const wstring matKey = NormalizePath((outputDir / (matStem + L".bammatinst")).wstring());

				if (const EResult result = ValidateImportOutput(manager, matKey, settings.SaveToFile, settings.Overwrite);
					result != EResult::Success)
				{
					return result;
				}

				auto existingMatHandle = manager.FindHandleByKey(matKey);
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

			if (const auto result = state.Output.Acquire(outputDir); result != EResult::Success)
				return finish(result);

			auto& desc = state.ModelDesc;
			const wstring stem = SanitizeFileName(data.SourcePath.stem().wstring());
			desc.Key = NormalizePath((outputDir / (stem + L".bammodel")).wstring());
			desc.Path = desc.Key;

			if (const EResult result = ValidateImportOutput(manager, desc.Key, state.Settings.SaveToFile, state.Settings.Overwrite);
				result != EResult::Success)
			{
				return finish(result);
			}

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
			state.Meshes.reserve(data.Meshes.size());

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

			if (state.Settings.SaveToFile)
			{
				std::error_code ec;
				filesystem::create_directories(Engine::PlatformPath::ResolveProjectRelative(outputDir), ec);
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
			const wstring key = NormalizePath((outputDir / (stem + L".bammesh")).wstring());

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

			state->Meshes.push_back({ key, std::move(mesh) });
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

		const auto commitStart = Clock::now();
		const EResult commitResult = CommitPreparedModel(*state, data, outputDir);
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
	options.RotationOffset = desc.RotationOffset;
	return options;
}

ModelImportDesc ModelImporter::GetDefaultDesc(const std::filesystem::path& sourcePath)
{
	ModelImportDesc desc{};
	std::string ext = sourcePath.extension().string();
	for (char& c : ext) c = static_cast<char>(tolower(c));

	// glTF 포맷은 자체 Y-Up이므로 Assimp의 MakeLeftHanded가 쿼터니언을 비트는 현상 방지
	if (ext == ".gltf" || ext == ".glb")
	{
		desc.PostProcessFlags &= ~EModelPostProcessFlag::MakeLeftHanded;
	}
	return desc;
}

EResult ModelImporter::CreateJob(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg, std::unique_ptr<ImportJob>& outJob)
{
	if (outJob || sourcePath.empty())
		return EResult::InvalidArgument;

	ModelImportDesc settings = arg ? *static_cast<const ModelImportDesc*>(arg) : ModelImportDesc{};

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