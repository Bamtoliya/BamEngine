#pragma once

#include "Bone.h"
#include "Resource.h"

struct SkeletonCreateDesc : public ResourceCreateDesc
{
	vector<Bone> Bones;
};

BEGIN(Engine)

CLASS()
class ENGINE_API Skeleton : public Resource
{
	REFLECT_CLASS()
	DECLARE_RESOURCE(Skeleton)
#pragma region Constructor&Destructor
private:
	using DESC = SkeletonCreateDesc;
	Skeleton() : Resource(EResourceType::Skeleton) {}
	virtual ~Skeleton() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Skeleton* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Getter
public:
	const vector<Bone>& GetBones() const { return m_Bones; }
	const unordered_map<wstring, uint32>& GetBoneMap() const { return m_BoneMap; }
	uint32 GetBoneIndex(const wstring& boneName) const;
	wstring GetBoneName(uint32 boneIndex) const;
#pragma endregion


#pragma region Member Variable
private:
	PROPERTY(CATEGORY("PROP_BONE"))
	vector<Bone> m_Bones;

	PROPERTY(CATEGORY("PROP_BONE"))
	unordered_map<wstring, uint32> m_BoneMap;
#pragma endregion
};
END