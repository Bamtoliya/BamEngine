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

#pragma region Residency
public:
	// 자리만 먼저 등록한 리소스는 키와 경로만 있고 내용이 없다. 내용을 쓰는 접근자는 RequireResident로 먼저 채운다.
	[[nodiscard]] bool IsResident() const { return m_Residency == EResidency::Resident; }
	[[nodiscard]] bool IsPending() const { return m_Residency == EResidency::Pending; }
	void MarkPending() { m_Residency = EResidency::Pending; }
	// 내용이 없으면 파일에서 바로 읽는다. 메인 스레드 전용이며 읽지 못한 리소스는 다시 시도하지 않는다.
	EResult EnsureResident();
	// 따로 읽은 같은 리소스의 내용을 가져온다. nullptr이거나 실패하면 읽지 못한 리소스로 남긴다.
	EResult Fulfill(Resource* prepared);
protected:
	// prepared의 GPU 객체까지 만든 뒤 내용을 맞바꾼다. 키·경로와 핸들은 유지한다.
	virtual EResult AdoptPrepared(Resource& prepared) { return EResult::NotImplemented; }
	void MarkResident() { m_Residency = EResidency::Resident; }
	void RequireResident() const
	{
		if (m_Residency == EResidency::Pending) static_cast<void>(const_cast<Resource*>(this)->EnsureResident());
	}
private:
	enum class EResidency : uint8
	{
		Resident,
		Pending,
		Unavailable
	};
	EResidency m_Residency = EResidency::Resident;
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
