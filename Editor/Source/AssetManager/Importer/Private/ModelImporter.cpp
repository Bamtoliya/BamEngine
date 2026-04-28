#pragma once

#include "Model.h"
#include "ModelImporter.h"

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

	wstring MakeUniqueMeshStem(
		const wstring& sourceStem,
		const wstring& meshKey,
		uint32 meshSerial,
		unordered_map<wstring, uint32>& nameCounts)
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
}

EResult ModelImporter::Import(const filesystem::path& sourcePath, const filesystem::path& destDir, void* arg)
{
	CAST_DESC
		Assimp::Importer importer;
	uint32 postProcessFlags = aiProcess_Triangulate | aiProcess_FlipUVs;
	if (desc)
	{
		postProcessFlags = static_cast<uint32>(desc->PostProcessFlags);
		if (postProcessFlags == 0)
		{
			postProcessFlags = aiProcess_Triangulate | aiProcess_FlipUVs;
		}
	}
	const aiScene* scene = importer.ReadFile(sourcePath.string(), postProcessFlags);
	if (!scene || (scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE) || !scene->mRootNode)
	{
		fmt::print("Error importing model: {}\n", importer.GetErrorString());
		return EResult::Fail;
	}
	tagModelCreateDesc modelCreateDesc;
	filesystem::path outputDir = destDir.empty() ? sourcePath.parent_path() : destDir;
	wstring sourceStem = sourcePath.stem().wstring();
	unordered_map<wstring, uint32> nameCounts;
	uint32 meshSerial = 0;

	if (HasFlag(desc->ImportOptions, EModelImportOption::Skeleton))
	{
		ProcessSkeleton(scene, outputDir, sourceStem, modelCreateDesc);
	}

	ProcessModelNode(scene->mRootNode, scene, outputDir, sourceStem, nameCounts, meshSerial, modelCreateDesc);

	if (HasFlag(desc->ImportOptions, EModelImportOption::Animations))
	{
		ProcessAnimations(scene, outputDir, sourceStem, modelCreateDesc);
	}

	//if (HasFlag(desc->ImportOptions, EModelImportOption::Materials))
	//{
	//	for (uint32 i = 0; i < scene->mNumMaterials; ++i)
	//	{
	//		ProcessMaterial(scene->mMaterials[i], i, scene, outputDir, sourceStem, modelCreateDesc);
	//	}
	//}

#pragma region Save To File
	//filesystem::path outputPath = outputDir / sourcePath.filename().replace_extension(".bammodel.json");
	//Model* model = Model::Create(&modelCreateDesc);
	//ResourceManager::Get().SaveToJsonFile(model, outputPath.wstring());
	//ResourceManager::Get().DestroyResource(model);
	//for (auto& meshHandle : modelCreateDesc.Meshes)
	//{
	//	ResourceManager::Get().DestroyResource(meshHandle.GetRawHandle());
	//}
	//for (auto& materialHandle : modelCreateDesc.Materials)
	//{
	//	ResourceManager::Get().DestroyResource(materialHandle.GetRawHandle());
	//}
	//for (auto& animationHandle : modelCreateDesc.Animations)
	//{
	//	ResourceManager::Get().DestroyResource(animationHandle.GetRawHandle());
	//}
	//ResourceManager::Get().DestroyResource(modelCreateDesc.Skeleton.GetRawHandle());
	//modelCreateDesc.Meshes.clear();
	//modelCreateDesc.Materials.clear();
	//modelCreateDesc.Animations.clear();
	//modelCreateDesc.Skeleton = ResourceHandle<Skeleton>();
#pragma endregion

	return EResult::Success;
}

void ModelImporter::ProcessModelNode(
	const aiNode* node,
	const aiScene* scene,
	const filesystem::path& outputDir,
	const wstring& sourceStem,
	unordered_map<wstring, uint32>& nameCounts,
	uint32& meshSerial,
	tagModelCreateDesc& modelCreateDesc)
{
	ResourceManager& resourceManager = ResourceManager::Get();
	for (uint32 i = 0; i < node->mNumMeshes; ++i)
	{
		tagMeshCreateDesc meshCreateInfo = {};
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
	tagMeshCreateDesc& meshCreateInfo,
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
			mat.bitangent = { mesh->mBitangents[i].x, mesh->mBitangents[i].y, mesh->mBitangents[i].z };
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
	tagMeshCreateDesc& meshCreateInfo,
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
void ModelImporter::ProcessMeshIndex(const aiMesh* mesh, const aiScene* scene, tagMeshCreateDesc& meshCreateInfo, vector<uint32>& outIndices)
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
	tagModelCreateDesc& modelCreateDesc)
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
	tagSkeletonCreateDesc skeletonDesc;
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
	tagModelCreateDesc& modelCreateDesc)
{
	ResourceManager& resourceManager = ResourceManager::Get();

	for (uint32 i = 0; i < scene->mNumAnimations; ++i)
	{
		const aiAnimation* aiAnim = scene->mAnimations[i];

		tagAnimationCreateDesc animDesc = {};
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
	tagModelCreateDesc& modelCreateDesc)
{
	ResourceManager& resourceManager = ResourceManager::Get();

	tagMaterialDesc matDesc = {};

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
