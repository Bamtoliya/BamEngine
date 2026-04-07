#pragma once

#include "Base.h"
#include "ReflectionMacro.h"
#include "SerializableInterface.h"

struct tagResourceCreateDesc
{
	wstring		Key = L"";
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
class ENGINE_API Resource : public SerializableInterface
{
	REFLECT_BASE()
	using DESC = tagResourceCreateDesc;
	friend class ResourceManager;
#pragma region Constructor&Destructor
protected:
 	Resource(EResourceType type) : m_ResourceType(type) {}
	virtual ~Resource() = default;
	virtual EResult Initialize(void* arg = nullptr)
	{
		if (!arg) return EResult::InvalidArgument;
		CAST_DESC
		m_Key = desc->Key;
		return EResult::Success; 
	}
public:
	virtual void Free() PURE;
#pragma	endregion


#pragma region Bind
	virtual EResult Bind(uint32 slot) { return EResult::NotImplemented; }
#pragma endregion

#pragma region Version Management
public:
	uint32 GetVersion() const { return m_Version; }
	void IncreaseVersion() { m_Version++; }
#pragma endregion

#pragma region Key Management
public:
	const wstring& GetKey() const { return m_Key; }
	void SetKey(const wstring& key) { m_Key = key; }
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
	uint32 m_Version = { 0 };

	PROPERTY(READONLY)
	EResourceType m_ResourceType;

	PROPERTY(READONLY)
	wstring m_Key = {};
#pragma endregion
};
END