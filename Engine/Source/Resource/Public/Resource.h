#pragma once

#include "Base.h"
#include "ReflectionMacro.h"
#include "TypeIdentityInterface.h"
#include <archive/ArchiveResult.h>
#include <reflection/ObjectView.h>
#include "ResourceBinaryHeader.h"
#include "PlatformPath.h"

#define DECLARE_RESOURCE(ClassType) \
public: \
	static ClassType* CreateEmpty() { return new ClassType(); } \
	[[nodiscard]] reflection::ObjectView GetReflectedObjectView() noexcept override \
	{ \
		return reflection::ObjectView::From(*this); \
	} \
	[[nodiscard]] reflection::ObjectView GetReflectedObjectView() const noexcept override \
	{ \
		return reflection::ObjectView::From(*this); \
	} \
private:

struct ResourceCreateDesc
{
	std::wstring		Key = L"";
	std::wstring		Path = L"";
};

BEGIN(archive)
class IArchive;
END

BEGIN(Engine)

CLASS()
class ENGINE_API Resource : public ITypeIdentity
{
	REFLECT_CLASS()
	using DESC = ResourceCreateDesc;
	friend class ResourceManager;
#pragma region Constructor&Destructor
protected:
 	Resource(EResourceType type) : m_ResourceType(type) {}
	public: virtual ~Resource() = default;
	virtual EResult Initialize(void* arg = nullptr)
	{
		if (!arg) return EResult::InvalidArgument;
		CAST_DESC
		m_Key = NormalizePath(desc->Key.empty() ? desc->Path : desc->Key);
		m_Path = NormalizePath(desc->Path);
		return EResult::Success; 
	}
public:
	virtual void Free() BAM_PURE;
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

#pragma region Path Management
public:
	const wstring& GetPath() const { return m_Path; }
	void SetPath(const wstring& path) { m_Path = path; }
	// Called under the manager lock: do not load resources or mutate state here.
	// Append changes to stable path fields, or return false to reject the transaction before any writes.
	virtual bool PrepareFilePathRelocation(const std::vector<FilePathChange>&, std::vector<Utf8FilePathUpdate>&)
	{
		return true;
	}
#pragma endregion

#pragma region GPU Upload
public:
	// DeferredResourceUpload 범위에서 만든 리소스의 GPU 객체를 만든다. 이미 있거나 필요 없으면 성공이다. 메인 스레드 전용.
	virtual EResult UploadToGPU() { return EResult::Success; }
	// UploadToGPU가 복사할 CPU 데이터의 크기
	[[nodiscard]] virtual size_t GetUploadByteSize() const { return 0; }
#pragma endregion

#pragma region Save&Load
public:
	[[nodiscard]] virtual reflection::ObjectView GetReflectedObjectView() noexcept
	{
		return reflection::ObjectView::From(*this);
	}
	[[nodiscard]] virtual reflection::ObjectView GetReflectedObjectView() const noexcept
	{
		return reflection::ObjectView::From(*this);
	}
	[[nodiscard]] virtual archive::ArchiveResult WriteArchive(archive::IArchive& ar) const;
	[[nodiscard]] virtual archive::ArchiveResult ReadArchive(const archive::IArchive& ar);
	// [Hot Reload]: 디스크 파일로부터 리소스를 인플레이스 핫스왑합니다.기본 구현은 리플렉션 아카이브를 로드합니다.
	virtual EResult Reload(const std::filesystem::path & filePath = {});
protected:
	[[nodiscard]] virtual uint32 GetArchiveFormatVersion() const noexcept { return 0; }
	virtual EResult OnPostLoad() { return EResult::Success; }
#pragma endregion

#pragma region Member Variables
protected:
	PROPERTY(READONLY, TRANSIENT)
	uint32 m_Version = { 0 };

	PROPERTY(READONLY)
	EResourceType m_ResourceType;

	PROPERTY(READONLY)
	wstring m_Key = {};

	PROPERTY(READONLY)
	wstring m_Path = {};
#pragma endregion
};
END
