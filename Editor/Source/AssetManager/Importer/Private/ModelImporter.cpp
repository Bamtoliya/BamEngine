#pragma once

#include "Model.h"
#include "ModelImporter.h"
#include "ModelImportTask.h"
#include "ModelConverter.h"
#include "ResourceManager.h"
#include "Logger.h"

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

		struct SavedMesh
		{
			archive::ArchiveResult Result;
			std::unique_ptr<StagedFile> File;
			size_t ByteCount = 0;
			double WorkerMs = 0.0;
			double WriteMs = 0.0;
			double ReleaseMs = 0.0;
		};

		using MeshOwner = std::unique_ptr<Mesh, ResourceDeleter>;
		using ModelOwner = std::unique_ptr<Resource, ResourceDeleter>;

		struct PendingMesh
		{
			wstring Key;
			MeshOwner Resource;
		};

		ModelImportDesc Settings;
		ModelCreateDesc ModelDesc;
		std::vector<PendingMesh> Meshes;
		unordered_map<wstring, uint32> NameCounts;
		size_t NextMesh = 0;
		size_t NextSave = 0;
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

	wstring MakeUniqueMeshStem( const wstring& sourceStem, const wstring& meshKey, uint32 meshSerial, unordered_map<wstring, uint32>& nameCounts)
	{
		wstring safeKey = meshKey.empty() ? (L"Mesh_" + std::to_wstring(meshSerial)) : meshKey;
		safeKey = SanitizeFileName(safeKey);

		wstring baseStem = SanitizeFileName(sourceStem) + L"_" + safeKey;

		uint32& count = nameCounts[baseStem];
		wstring finalStem = (count == 0) ? baseStem : (baseStem + L"_" + std::to_wstring(count));
		++count;

		return finalStem;
	}

	mat4 AssimpMat4ToGlm(const aiMatrix4x4& aiMat)
	{
		return mat4(
			aiMat.a1, aiMat.b1, aiMat.c1, aiMat.d1,
			aiMat.a2, aiMat.b2, aiMat.c2, aiMat.d2,
			aiMat.a3, aiMat.b3, aiMat.c3, aiMat.d3,
			aiMat.a4, aiMat.b4, aiMat.c4, aiMat.d4
		);
	}

 	EResult ValidateImportOutput(ResourceManager & manager, const wstring& key, bool saveToFile)
	{
		if (manager.IsValid(manager.FindHandleByKey(key)))
		{
			BAM_LOG(Error, "Editor", "Asset", "Import output resource already registered: {}", WStrToStr(key));
			return EResult::AlreadyInitialized;
		}

		if (!saveToFile)
			return EResult::Success;

		std::error_code ec;
		const bool exists = filesystem::exists(filesystem::path(key), ec);

		if (ec)
		{
			BAM_LOG(Error, "Editor", "Asset", "Failed to inspect import output: path={}, error={}",
				WStrToStr(key), ec.message());
			return EResult::Fail;
		}

		if (exists)
		{
			BAM_LOG(Error, "Editor", "Asset", "Import output already exists: {}", WStrToStr(key));
			return EResult::AlreadyInitialized;
		}

		return EResult::Success;
	}

	EResult CommitPreparedModel(ModelFinalizeState& state)
	{
		auto& manager = ResourceManager::Get();
		auto& modelDesc = state.ModelDesc;
		auto& pending = state.Meshes;
		const auto& settings = state.Settings;
		const auto& modelKey = modelDesc.Key;
		using ModelOwner = ModelFinalizeState::ModelOwner;

		if (const EResult result = ValidateImportOutput(manager, modelKey, settings.SaveToFile);
			result != EResult::Success)
		{
			return result;
		}

		for (const auto& mesh : pending)
		{
			// 메시 파일은 이 작업이 이미 저장했으므로 등록 여부만 검사합니다.
			if (const EResult result = ValidateImportOutput(manager, mesh.Key, false);
				result != EResult::Success)
			{
				return result;
			}
		}

		// 이후 단계가 실패하면 이번에 등록한 메시만 정리합니다.
		struct MeshRegistrationRollback
		{
			ResourceManager& Manager;
			std::vector<Handle> Handles;
			bool Committed = false;

			~MeshRegistrationRollback()
			{
				if (Committed)
					return;

				for (auto it = Handles.rbegin(); it != Handles.rend(); ++it)
					Manager.DestroyResource(*it);
			}
		};

		MeshRegistrationRollback rollback{ manager, {}, false };
		rollback.Handles.reserve(pending.size());
		modelDesc.Meshes.reserve(pending.size());

		for (auto& mesh : pending)
		{
			Mesh* instance = mesh.Resource.get();
			auto handle = manager.AddResource<Mesh>(mesh.Key, mesh.Resource.release());

			if (!handle.IsValid())
				return EResult::Fail;

			// AddResource는 중복 키이면 기존 리소스를 반환합니다.
			if (handle.Get() != instance)
				return EResult::AlreadyInitialized;

			rollback.Handles.push_back(handle.GetRawHandle());
			modelDesc.Meshes.push_back(std::move(handle));
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

		Model* instance = static_cast<Model*>(model.get());
		auto modelHandle = manager.AddResource<Model>(modelKey, static_cast<Model*>(model.release()));

		if (!modelHandle.IsValid())
			return EResult::Fail;

		if (modelHandle.Get() != instance)
			return EResult::AlreadyInitialized;

		rollback.Committed = true;

		BAM_LOG(Info, "Editor", "Asset", "Model import finished: meshes={}, nodes={}, key={}",
			modelDesc.Meshes.size(), modelDesc.Nodes.size(), WStrToStr(modelKey));

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

			auto& desc = state.ModelDesc;
			const wstring stem = SanitizeFileName(data.SourcePath.stem().wstring());
			desc.Key = NormalizePath((outputDir / (stem + L".bammodel")).wstring());
			desc.Path = desc.Key;

			if (const EResult result = ValidateImportOutput(manager, desc.Key, state.Settings.SaveToFile);
				result != EResult::Success)
			{
				return finish(result);
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

				for (int column = 0; column < 4; ++column)
				{
					for (int row = 0; row < 4; ++row)
						node.LocalTransform[column][row] = source.LocalTransform[column * 4 + row];
				}

				desc.Nodes.push_back(std::move(node));
			}

			if (state.Settings.SaveToFile)
			{
				std::error_code ec;
				filesystem::create_directories(outputDir, ec);
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
			constexpr auto maxBytes = (std::numeric_limits<uint32>::max)();

			if (source.Attributes.size() != vertexCount ||
				vertexCount > maxBytes / sizeof(VertexPosition) ||
				vertexCount > maxBytes / sizeof(VertexMaterial) || indexCount > maxBytes / sizeof(uint32))
			{
				return finish(EResult::InvalidArgument);
			}

			const wstring stem = MakeUniqueMeshStem(data.SourcePath.stem().wstring(),
				StrToWStr(source.Name), static_cast<uint32>(state->NextMesh), state->NameCounts);
			const wstring key = NormalizePath((outputDir / (stem + L".bammesh")).wstring());

			if (const EResult result = ValidateImportOutput(manager, key, state->Settings.SaveToFile);
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
			desc.IndexData = source.Indices.data();
			desc.IndexCount = static_cast<uint32>(indexCount);
			desc.IndexStride = sizeof(uint32);
			desc.BoundingBoxMin = source.BoundsMin;
			desc.BoundingBoxMax = source.BoundsMax;

			const auto createStart = Clock::now();
			ModelFinalizeState::MeshOwner mesh(Mesh::Create(&desc));
			const double createMs = std::chrono::duration<double, std::milli>(Clock::now() - createStart).count();

			const auto vertexBytes = static_cast<std::uint64_t>(vertexCount) *
				(sizeof(VertexPosition) + sizeof(VertexMaterial));
			const auto indexBytes = static_cast<std::uint64_t>(indexCount) * sizeof(uint32);

			BAM_LOG(Info, "Editor", "Asset",
				"Model mesh create timing: key={}, vertices={}, indices={}, input_bytes={}, create_ms={:.3f}, success={}",
				WStrToStr(key), vertexCount, indexCount, vertexBytes + indexBytes, createMs, static_cast<bool>(mesh));

			if (!mesh)
				return finish(EResult::Fail);

			state->Meshes.push_back({ key, std::move(mesh) });
			++state->NextMesh;
			return std::nullopt;
		}

		if (state->Settings.SaveToFile && state->NextSave < state->Meshes.size())
		{
			const auto& mesh = state->Meshes[state->NextSave];

			if (!state->Saving.valid())
			{
				if (const EResult result = ValidateImportOutput(manager, mesh.Key, true);
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
				static Engine::uint64 stagingSerial = 0;

				filesystem::path stagingPath(mesh.Key);
				stagingPath += ".bamimport-" + std::to_string(++stagingSerial) + "-" +
					std::to_string(Clock::now().time_since_epoch().count()) + ".tmp";

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

			const auto publishStart = Clock::now();

			// 저장하는 동안 다른 요청이 최종 출력을 만들었는지 다시 검사합니다.
			if (const EResult result = ValidateImportOutput(manager, mesh.Key, true);
				result != EResult::Success)
			{
				return finish(result);
			}

			std::error_code ec;
			filesystem::rename(saved.File->Path, filesystem::path(mesh.Key), ec);

			if (ec)
			{
				BAM_LOG(Error, "Editor", "Asset",
					"Failed to publish imported mesh: path={}, error={}", WStrToStr(mesh.Key), ec.message());
				return finish(EResult::Fail);
			}

			saved.File->Owned = false;

			if (HotReloadManager::IsCreated())
				HotReloadManager::Get().GetAssetReloader().RecordMeshFileState(filesystem::path(mesh.Key));

			const double publishMs =
				std::chrono::duration<double, std::milli>(Clock::now() - publishStart).count();

			BAM_LOG(Info, "Editor", "Asset",
				"Model mesh save timing: path={}, bytes={}, snapshot_ms={:.3f}, worker_ms={:.3f}, "
				"write_ms={:.3f}, release_ms={:.3f}, publish_ms={:.3f}",
				WStrToStr(mesh.Key), saved.ByteCount, state->SnapshotMs, saved.WorkerMs,
				saved.WriteMs, saved.ReleaseMs, publishMs);

			++state->NextSave;
			return std::nullopt;
		}

		const auto commitStart = Clock::now();
		const EResult commitResult = CommitPreparedModel(*state);
		const double commitMs = std::chrono::duration<double, std::milli>(Clock::now() - commitStart).count();

		BAM_LOG(Info, "Editor", "Asset",
			"Model commit timing: key={}, meshes={}, nodes={}, commit_ms={:.3f}, result={}",
			WStrToStr(state->ModelDesc.Key), state->Meshes.size(), state->ModelDesc.Nodes.size(),
			commitMs, static_cast<int>(commitResult));

		return finish(commitResult);
	}
	catch (...)
	{
		task.m_FinalizeState.reset();
		throw;
	}
}

EResult ModelImporter::CreateJob(const filesystem::path& sourcePath, const filesystem::path& destDir,
	void* arg, std::unique_ptr<ImportJob>& outJob)
{
	if (outJob || sourcePath.empty())
		return EResult::InvalidArgument;

	ModelImportDesc settings = arg ? *static_cast<const ModelImportDesc*>(arg) : ModelImportDesc{};

	if (settings.PostProcessFlags == EModelPostProcessFlag::None)
		settings.PostProcessFlags = EModelPostProcessFlag::Triangulate | EModelPostProcessFlag::FlipUVs;

	// 현재 변환 모듈에서 처리할 수 있는 후처리 설정만 받습니다.
	const auto supportedFlags = EModelPostProcessFlag::Triangulate | EModelPostProcessFlag::FlipUVs |
		EModelPostProcessFlag::GenSmoothNormals | EModelPostProcessFlag::CalcTangentSpace;

	const auto requestedFlags = static_cast<uint32>(settings.PostProcessFlags);
	if ((requestedFlags & ~static_cast<uint32>(supportedFlags)) != 0)
		return EResult::NotImplemented;

	if (settings.ImportOptions != EModelImportOption::Meshes)
	{
		BAM_LOG(Error, "Editor", "Asset", "Only static mesh import is currently supported.");
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
		outputDirectory = filesystem::absolute(destDir, ec).lexically_normal();
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

void ModelImporter::ProcessModelNode(
	const aiNode* node,
	const aiScene* scene,
	const filesystem::path& outputDir,
	const wstring& sourceStem,
	unordered_map<wstring, uint32>& nameCounts,
	uint32& meshSerial,
	ModelCreateDesc& modelCreateDesc)
{
	ResourceManager& resourceManager = ResourceManager::Get();
	for (uint32 i = 0; i < node->mNumMeshes; ++i)
	{
		MeshCreateDesc meshCreateInfo = {};
		const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];

		// 분리된 배열로 받기
		vector<VertexPosition> positions;
		vector<VertexMaterial> materials;
		vector<VertexSkinData> skinData;
		vector<uint32> indices;

		ProcessMeshVertex(mesh, scene, meshCreateInfo, positions, materials);

		Skeleton* pSkeleton = modelCreateDesc.Skeleton.Get();
		if (mesh->HasBones() && pSkeleton)
		{
			meshCreateInfo.Flags |= EMeshFlag::Dynamic;
			ProcessMeshSkinData(mesh, scene, pSkeleton, meshCreateInfo, skinData);
		}

		ProcessMeshIndex(mesh, scene, meshCreateInfo, indices);

		const wstring meshKey = StrToWStr(mesh->mName.C_Str());
		const wstring meshStem = MakeUniqueMeshStem(sourceStem, meshKey, meshSerial++, nameCounts);
		filesystem::path meshPath = outputDir / (meshStem + L".bammesh");

		meshCreateInfo.Path = NormalizePath(meshPath.wstring());
		meshCreateInfo.Key = meshCreateInfo.Path;
		meshCreateInfo.Flags |= EMeshFlag::KeepRawData;

		Mesh* newMesh = Mesh::Create(&meshCreateInfo);
		if (newMesh)
		{
			ResourceHandle<Mesh> handle = resourceManager.AddResource<Mesh>(meshCreateInfo.Key, newMesh);
			modelCreateDesc.Meshes.push_back(handle);
			resourceManager.SaveToBinaryFile(newMesh, meshCreateInfo.Path);
		}
	}
	for (uint32 i = 0; i < node->mNumChildren; ++i)
	{
		ProcessModelNode(node->mChildren[i], scene, outputDir, sourceStem, nameCounts, meshSerial, modelCreateDesc);
	}
}

void ModelImporter::ProcessMeshVertex(
	const aiMesh* mesh,
	const aiScene* scene,
	MeshCreateDesc& meshCreateInfo,
	vector<VertexPosition>& outPositions,
	vector<VertexMaterial>& outMaterials)
{
	(void)scene;
	outPositions.clear();
	outMaterials.clear();
	outPositions.reserve(mesh->mNumVertices);
	outMaterials.reserve(mesh->mNumVertices);

	vec3 boundingBoxMin(FLT_MAX);
	vec3 boundingBoxMax(-FLT_MAX);

	for (uint32 i = 0; i < mesh->mNumVertices; ++i)
	{
		// Position 스트림
		VertexPosition pos = {};
		pos.position = { mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z };
		boundingBoxMin = glm::min(boundingBoxMin, pos.position);
		boundingBoxMax = glm::max(boundingBoxMax, pos.position);
		outPositions.push_back(pos);

		// Material 스트림
		VertexMaterial mat = {};
		mat.color = { 1.0f, 1.0f, 1.0f, 1.0f };

		if (mesh->HasNormals())
			mat.normal = { mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z };

		if (mesh->mTextureCoords[0])
			mat.texCoord = { mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y };

		if (mesh->HasTangentsAndBitangents())
		{
			mat.tangent = { mesh->mTangents[i].x,   mesh->mTangents[i].y,   mesh->mTangents[i].z };
			mat.binormal = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
		}

		if (mesh->HasVertexColors(0))
			mat.color = { mesh->mColors[0][i].r, mesh->mColors[0][i].g, mesh->mColors[0][i].b, mesh->mColors[0][i].a };

		outMaterials.push_back(mat);
	}

	// Streams 배열에 세팅
	uint32 vertexCount = static_cast<uint32>(outPositions.size());

	meshCreateInfo.Streams[(uint32)EMeshStream::Position] = {
		outPositions.empty() ? nullptr : outPositions.data(),
		vertexCount,
		sizeof(VertexPosition)
	};

	meshCreateInfo.Streams[(uint32)EMeshStream::Material] = {
		outMaterials.empty() ? nullptr : outMaterials.data(),
		vertexCount,
		sizeof(VertexMaterial)
	};

	meshCreateInfo.BoundingBoxMin = outPositions.empty() ? vec3(0.0f) : boundingBoxMin;
	meshCreateInfo.BoundingBoxMax = outPositions.empty() ? vec3(0.0f) : boundingBoxMax;
}

void ModelImporter::ProcessMeshSkinData(
	const aiMesh* mesh,
	const aiScene* scene,
	Skeleton* skeleton,
	MeshCreateDesc& meshCreateInfo,
	vector<VertexSkinData>& outSkinData)
{
	(void)scene;
	outSkinData.clear();
	outSkinData.resize(mesh->mNumVertices);

	// 초기화 (모든 웨이트와 BoneID를 0으로)
	for (auto& skin : outSkinData) {
		skin.boneIDs = uvec4(0);
		skin.weights = vec4(0.0f);
	}
	for (uint32 i = 0; i < mesh->mNumBones; ++i)
	{
		const aiBone* bone = mesh->mBones[i];
		wstring boneName = StrToWStr(bone->mName.C_Str());

		// 1. 핵심: 메쉬 로컬 i 번호가 아니라, Skeleton에 등록된 진짜 뼈 번호를 가져옵니다.
		uint32 globalBoneID = skeleton->GetBoneIndex(boneName);
		// (Skeleton 클래스에 이름으로 Index를 찾는 함수가 있어야 합니다)
		for (uint32 j = 0; j < bone->mNumWeights; ++j)
		{
			const aiVertexWeight& weight = bone->mWeights[j];
			const uint32 vertexID = weight.mVertexId;

			if (vertexID >= outSkinData.size()) continue;
			// 빈 슬롯 찾아서 채우기
			for (uint32 slot = 0; slot < 4; ++slot)
			{
				if (outSkinData[vertexID].weights[slot] == 0.0f)
				{
					outSkinData[vertexID].boneIDs[slot] = globalBoneID; // <--- 로컬 i가 아닌 절대 번호 대입!
					outSkinData[vertexID].weights[slot] = weight.mWeight;
					break;
				}
			}
		}
	}
	// 함수 맨 끝 부분만 수정
	meshCreateInfo.Streams[(uint32)EMeshStream::SkinData] = {
		outSkinData.empty() ? nullptr : outSkinData.data(),
		static_cast<uint32>(outSkinData.size()),
		sizeof(VertexSkinData)
	};

}
void ModelImporter::ProcessMeshIndex(const aiMesh* mesh, const aiScene* scene, MeshCreateDesc& meshCreateInfo, vector<uint32>& outIndices)
{
	(void)scene;
	outIndices.clear();
	outIndices.reserve(mesh->mNumFaces * 3);
	for (uint32 i = 0; i < mesh->mNumFaces; ++i)
	{
		const aiFace& face = mesh->mFaces[i];
		for (uint32 j = 0; j < face.mNumIndices; ++j)
		{
			outIndices.push_back(face.mIndices[j]);
		}
	}
	meshCreateInfo.IndexData = outIndices.empty() ? nullptr : outIndices.data();
	meshCreateInfo.IndexCount = static_cast<uint32>(outIndices.size());
	meshCreateInfo.IndexStride = sizeof(uint32);
}
void ModelImporter::ProcessSkeleton(
	const aiScene* scene,
	const filesystem::path& outputDir,
	const wstring& sourceStem,
	ModelCreateDesc& modelCreateDesc)
{
	// 1. 전체 메쉬를 돌면서 뼈의 이름과 OffsetMatrix(역 바인드 포즈)를 수집합니다.
	unordered_map<string, mat4> boneOffsetMap;
	for (uint32 i = 0; i < scene->mNumMeshes; ++i)
	{
		const aiMesh* mesh = scene->mMeshes[i];
		for (uint32 j = 0; j < mesh->mNumBones; ++j)
		{
			const aiBone* aiBone = mesh->mBones[j];
			string boneName = aiBone->mName.C_Str();
			if (boneOffsetMap.find(boneName) == boneOffsetMap.end())
			{
				mat4 offset = AssimpMat4ToGlm(aiBone->mOffsetMatrix);
				boneOffsetMap[boneName] = offset;
			}
		}
	}
	// 뼈가 하나도 없다면 스켈레톤을 만들 필요가 없습니다.
	if (boneOffsetMap.empty())
		return;
	SkeletonCreateDesc skeletonDesc;
	// 2. DFS 재귀로 트리를 순회. (오타 수정: <auto& self -> auto& self)
	// 부모를 push_back 한 뒤 자식을 재귀호출하므로, 무조건 부모 Index < 자식 Index 가 보장됩니다.
	auto buildSkeletonTree = [&](auto& self, const aiNode* node, int32 parentIndex) -> void
		{
			string nodeName = node->mName.C_Str();
			Bone newBone;
			newBone.Name = StrToWStr(nodeName);
			newBone.ParentIndex = parentIndex;
			newBone.LocalTransform = AssimpMat4ToGlm(node->mTransformation);
			// 실제 스키닝에 관여하는 뼈라면 수집해둔 OffsetMatrix를 사용, 아니면 단위행렬
			if (boneOffsetMap.find(nodeName) != boneOffsetMap.end()) {
				newBone.OffsetMatrix = boneOffsetMap[nodeName];
			}
			else {
				newBone.OffsetMatrix = mat4(1.0f);
			}
			// 현재 내 인덱스를 얻어 배열에 삽입
			int32 myIndex = static_cast<int32>(skeletonDesc.Bones.size());
			skeletonDesc.Bones.push_back(newBone);
			// 자식 탐색
			for (uint32 i = 0; i < node->mNumChildren; ++i) {
				self(self, node->mChildren[i], myIndex);
			}
		};
	// 씬의 루트부터 파싱 시작
	buildSkeletonTree(buildSkeletonTree, scene->mRootNode, -1);
	// 3. 리소스 생성 및 등록
	wstring skelStem = SanitizeFileName(sourceStem + L"_Skeleton");
	filesystem::path skelPath = outputDir / (skelStem + L".bamskel");
	skeletonDesc.Path = NormalizePath(skelPath.wstring());
	skeletonDesc.Key = skeletonDesc.Path;
	Skeleton* newSkeleton = Skeleton::Create(&skeletonDesc);
	if (newSkeleton)
	{
		ResourceHandle<Skeleton> handle = ResourceManager::Get().AddResource<Skeleton>(skeletonDesc.Key, newSkeleton);
		modelCreateDesc.Skeleton = handle;
		ResourceManager::Get().SaveToBinaryFile(newSkeleton, skeletonDesc.Path);
	}
}
void ModelImporter::ProcessAnimations(
	const aiScene* scene,
	const filesystem::path& outputDir,
	const wstring& sourceStem,
	ModelCreateDesc& modelCreateDesc)
{
	ResourceManager& resourceManager = ResourceManager::Get();

	for (uint32 i = 0; i < scene->mNumAnimations; ++i)
	{
		const aiAnimation* aiAnim = scene->mAnimations[i];

		AnimationCreateDesc animDesc = {};
		animDesc.Duration = aiAnim->mDuration;
		animDesc.TicksPerSecond = aiAnim->mTicksPerSecond != 0.0 ? aiAnim->mTicksPerSecond : 24.0f; // 기본값 방어 코드

		// 트랙 (본별 애니메이션) 추출
		animDesc.Tracks.reserve(aiAnim->mNumChannels);
		for (uint32 j = 0; j < aiAnim->mNumChannels; ++j)
		{
			const aiNodeAnim* channel = aiAnim->mChannels[j];
			AnimationTrack track;
			track.TargetBoneName = StrToWStr(channel->mNodeName.C_Str());

			// 1. Position Keyframes
			track.PositionKeyframes.reserve(channel->mNumPositionKeys);
			for (uint32 k = 0; k < channel->mNumPositionKeys; ++k)
			{
				const aiVectorKey& key = channel->mPositionKeys[k];
				track.PositionKeyframes.push_back({ key.mTime, vec3(key.mValue.x, key.mValue.y, key.mValue.z) });
			}

			// 2. Rotation Keyframes (Assimp의 aiQuaternion은 w, x, y, z 순서임에 주의)
			track.RotationKeyframes.reserve(channel->mNumRotationKeys);
			for (uint32 k = 0; k < channel->mNumRotationKeys; ++k)
			{
				const aiQuatKey& key = channel->mRotationKeys[k];
				track.RotationKeyframes.push_back({ key.mTime, quat(key.mValue.w, key.mValue.x, key.mValue.y, key.mValue.z) });
			}

			// 3. Scale Keyframes
			track.ScaleKeyframes.reserve(channel->mNumScalingKeys);
			for (uint32 k = 0; k < channel->mNumScalingKeys; ++k)
			{
				const aiVectorKey& key = channel->mScalingKeys[k];
				track.ScaleKeyframes.push_back({ key.mTime, vec3(key.mValue.x, key.mValue.y, key.mValue.z) });
			}

			animDesc.Tracks.push_back(track);
		}

		// 고유 식별자 생성 및 경로 설정
		wstring animKey = StrToWStr(aiAnim->mName.C_Str());
		if (animKey.empty()) animKey = L"Anim_" + std::to_wstring(i);

		wstring animStem = SanitizeFileName(sourceStem + L"_" + animKey);
		filesystem::path animPath = outputDir / (animStem + L".bamanim");

		animDesc.Path = NormalizePath(animPath.wstring());
		animDesc.Key = animDesc.Path; // Key를 경로로 사용할 지 ID로 사용할 지 정책에 따라 조정

		// 리소스 생성 및 등록
		Animation* newAnim = Animation::Create(&animDesc);
		if (newAnim)
		{
			ResourceHandle<Animation> handle = resourceManager.AddResource<Animation>(animDesc.Key, newAnim);
			modelCreateDesc.Animations.push_back(handle);

			// 물리적인 파일로 직렬화 아웃 (SaveToBinaryFile 등 엔진 API 활용)
			resourceManager.SaveToBinaryFile(newAnim, animDesc.Path);
		}
	}
}

void ModelImporter::ProcessMaterial(
	const aiMaterial* aiMat,
	uint32 matIndex,
	const aiScene* scene,
	const filesystem::path& outputDir,
	const wstring& sourceStem,
	ModelCreateDesc& modelCreateDesc)
{
	ResourceManager& resourceManager = ResourceManager::Get();

	MaterialDesc matDesc = {};

	// 머티리얼 이름 추출, 없으면 기본 인덱스
	aiString matName;
	if (aiMat->Get(AI_MATKEY_NAME, matName) != aiReturn_SUCCESS) {
		matName = aiString("Material_" + std::to_string(matIndex));
	}

	wstring matKey = StrToWStr(matName.C_Str());
	wstring matStem = SanitizeFileName(sourceStem + L"_" + matKey);
	filesystem::path matPath = outputDir / (matStem + L".bammat");

	matDesc.Path = NormalizePath(matPath.wstring());
	matDesc.Key = matDesc.Path;

	// TODO: 머티리얼 속성 파싱 (Diffuse Color 등)
	aiColor4D colorOut;
	if (aiMat->Get(AI_MATKEY_COLOR_DIFFUSE, colorOut) == aiReturn_SUCCESS) {
		// matDesc에 색상 정보 기록
	}

	// TODO: 텍스처 파싱 파트 (사용하시는 텍스처 키 연동)
	aiString texPath;
	if (aiMat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == aiReturn_SUCCESS) {
		// Albedo 텍스처 경로 변환 및 등록
	}

	Material* newMaterial = Material::Create(&matDesc);
	if (newMaterial)
	{
		ResourceHandle<Material> handle = resourceManager.AddResource<Material>(matDesc.Key, newMaterial);
		modelCreateDesc.Materials.push_back(handle);

		// JSON 또는 Binary 형태로 머티리얼 직렬화 저장
		resourceManager.SaveToJsonFile(newMaterial, matDesc.Path);
	}
}
