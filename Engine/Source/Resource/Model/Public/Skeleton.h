#pragma once

#include "Bone.h"
#include "Resource.h"

struct tagSkeletonCreateDesc
{
	wstring Tag;
	vector<Bone> Bones;
};

BEGIN(Engine)

CLASS()
class ENGINE_API Skeleton : public Resource
{
	REFLECT_CLASS()
#pragma region Constructor&Destructor
private:
	Skeleton() = default;
	virtual ~Skeleton() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static Skeleton* Create(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Member Variable
private:
	PROPERTY()
	vector<Bone> m_Bones;

	PROPERTY()
	unordered_map<wstring, uint32> m_BoneMap;
#pragma endregion
};
END