#pragma once

#include "Base.h"
#include "ReflectionMacro.h"
#include "SerializableInterface.h"

struct tagResourceCreateDesc
{
	wstring		Tag = L"";
	wstring		Path = L"";
};

BEGIN(Engine)

inline constexpr uint64 ENGINE_ASSET_MAGIC = 0x54455353414D4142;

ENUM()
enum class EResourceType : uint32
{
	Unknown = 0,
	Texture,
	Sprite,
	Material,
	MaterialInstance,
	Shader,
	Mesh,
	Model,
	Animation,
	AudioClip,
	Skeleton,
	Prefab,
	Script,
	Scene,
	Save,

};

STRUCT()
struct ENGINE_API tagResourceBinaryHeader
{
	REFLECT_STRUCT()

	PROPERTY()
	uint64 MagicNumber = { ENGINE_ASSET_MAGIC };

	PROPERTY()
	EResourceType ResourceType = { EResourceType::Unknown };

	PROPERTY()
	uint32 Version = { 1 };

	tagResourceBinaryHeader() = default;
};


CLASS()
class ENGINE_API Resource : public Base, public SerializableInterface
{
	REFLECT_BASE()
	using DESC = tagResourceCreateDesc;
#pragma region Constructor&Destructor
protected:
 	Resource(EResourceType type) : m_ResourceType(type) {}
	virtual ~Resource() = default;
	virtual EResult Initialize(void* arg = nullptr) {
		if (arg)
		{
			CAST_DESC
			m_Tag = desc->Tag;
		}
		return EResult::Success; 
	}
public:
	virtual void Free() PURE;
#pragma	endregion


#pragma region Bind
	virtual EResult Bind(uint32 slot) PURE;
#pragma endregion

#pragma region Tag Management
public:
	wstring GetTag() { return m_Tag; }
	void SetTag(const wstring& tag) { m_Tag = tag; }
#pragma endregion

#pragma region Version Management
public:
	uint32 GetVersion() const { return m_Version; }
	void IncreaseVersion() { m_Version++; }
#pragma endregion

#pragma region Path Management
public:
	const wstring& GetPath() const { return m_Path; }
	void SetPath(const wstring& path) { m_Path = path; }
#pragma endregion

#pragma region Save & Load
public:
	virtual void Serialize(Archive& ar) override
	{
		tagResourceBinaryHeader header;
		header.ResourceType = m_ResourceType;
		header.Version = m_Version;
		ar.Process<tagResourceBinaryHeader>("AssetHeader", header);
		SerializationHelper::SerializeReflectionProperties(ar, &GetTypeInfo(), this);
	}
	virtual void Deserialize(Archive& ar) override
	{
		tagResourceBinaryHeader header;
		ar.Process<tagResourceBinaryHeader>("AssetHeader", header);
		if (header.MagicNumber != ENGINE_ASSET_MAGIC)
		{
			fmt::print(stderr, "Invalid asset file: Magic number mismatch.\n");
		}
		return;
		
		if(header.Version < 1)
		{

		}

		SerializationHelper::SerializeReflectionProperties(ar, &GetTypeInfo(), this);
	}
#pragma endregion



#pragma region Member Variables
protected:
	PROPERTY(READONLY)
	wstring m_Tag = {};

	PROPERTY(READONLY)
	uint32 m_Version = { 0 };

	PROPERTY(READONLY)
	EResourceType m_ResourceType;

	PROPERTY(READONLY)
	wstring m_Path = {};
#pragma endregion
};
END