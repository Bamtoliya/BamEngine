#pragma once

#include "Base.h"
#include "RenderTypes.h"
#include <vector>
#include <unordered_map>

BEGIN(Engine)
class ENGINE_API RenderPass : public Base
{
	using DESC = RenderPassDesc;
#pragma region Constructor&Destructor
private:
	RenderPass() {}
	virtual ~RenderPass() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static RenderPass* Create(void* arg = nullptr);
	virtual void Free() override { __super::Free(); }
#pragma endregion

#pragma region Getter
public:
	const RenderPassDesc& GetDesc() const { return m_Desc; }
	const uint32 GetPriority() const { return m_Desc.priority; }
	const RenderPassID GetID() const { return m_Desc.ID; }
	const wstring& GetName() const { return m_Desc.name; }
	const uint32 GetRenderTargetCount() const { return static_cast<uint32>(m_Desc.renderTargetNames.size()); }
	const wstring& GetRenderTargetName(uint32 index) const { return m_Desc.renderTargetNames[index]; }
	const wstring& GetDepthStencilName() const { return m_Desc.depthStencilName; }
	const ERenderSortType GetSortType() const { return m_Desc.sortType; }
	const ERenderPassLoadOperation GetLoadOperation() const { return m_Desc.loadOperation; }
	const ERenderPassStoreOperation GetStoreOperation() const { return m_Desc.storeOperation; }		
	const ERenderPassLoadOperation GetStencilLoadOperation() const { return m_Desc.stencilLoadOperation; }
	const ERenderPassStoreOperation GetStencilStoreOperation() const { return m_Desc.stencilStoreOperation; }	
	const vec4& GetOverrideClearColor() const { return m_Desc.overrideClearColor; }
	bool HasOverrideClearColor() const { return m_Desc.overrideClearColor.a >= 0.0f; }
	const ERenderPassType GetPassType() const { return m_Desc.passType; }
	EBlendMode GetAcceptedBlendModes() const { return m_Desc.acceptedBlendModes; }
	bool IsAcceptsBlendMode(EBlendMode mode) const { return (m_Desc.acceptedBlendModes & mode) != EBlendMode::None || (mode == EBlendMode::PureAdditive && (m_Desc.acceptedBlendModes & EBlendMode::Additive) != EBlendMode::None); }
	bool IsEnabled() const { return m_Desc.isEnabled; }
	bool UsesPointSpriteSampling() const { return m_PointSpriteSampling; }
	// 레이트레이싱이 차폐를 맡는 캐스터를 제외하는지. 방향광 그림자 패스는 캐스케이드, 점광원 그림자 패스는 큐브맵에 적용한다.
	bool SkipsRaytracedShadowCasters() const { return m_SkipsRaytracedShadowCasters; }
	void SetSkipsRaytracedShadowCasters(bool skip) { m_SkipsRaytracedShadowCasters = skip; }
	// 방향광 그림자 패스의 스포트 사분면에서 제외하는지
	bool SkipsRaytracedSpotCasters() const { return m_SkipsRaytracedSpotCasters; }
	void SetSkipsRaytracedSpotCasters(bool skip) { m_SkipsRaytracedSpotCasters = skip; }
#pragma endregion

#pragma region Setter
public:
	void SetPriority(uint32 priority) { m_Desc.priority = priority; }
	void SetID(RenderPassID id) { m_Desc.ID = id; }
	void SetName(const wstring& name) { m_Desc.name = name; }
	void SetSortType(ERenderSortType sortType) { m_Desc.sortType = sortType; }
	void AddColorAttachment(const wstring& rtName) { m_Desc.renderTargetNames.push_back(rtName); }
	void ClearColorAttachments() { m_Desc.renderTargetNames.clear(); }
	void SetColorAttachments(const std::vector<wstring>& rtNames) { m_Desc.renderTargetNames = rtNames; }
	void SetDepthStencilAttachment(const wstring& dsName) { m_Desc.depthStencilName = dsName; }
	void SetLoadOperation(ERenderPassLoadOperation loadOp) { m_Desc.loadOperation = loadOp; }
	void SetStoreOperation(ERenderPassStoreOperation storeOp) { m_Desc.storeOperation = storeOp; }
	void SetStencilLoadOperation(ERenderPassLoadOperation loadOp) { m_Desc.stencilLoadOperation = loadOp; }
	void SetStencilStoreOperation(ERenderPassStoreOperation storeOp) { m_Desc.stencilStoreOperation = storeOp; }
	void SetOverrideClearColor(const vec4& color) { m_Desc.overrideClearColor = color; }
	void SetPassType(ERenderPassType passType) { m_Desc.passType = passType; }
	void SetAcceptedBlendModes(EBlendMode blendModes) { m_Desc.acceptedBlendModes = blendModes; }
	void SetEnabled(bool enabled) { m_Desc.isEnabled = enabled; }
	void SetPointSpriteSampling(bool enabled) { m_PointSpriteSampling = enabled; }
#pragma endregion

#pragma region Pass Constant Buffer Interface
public:
	// 임의의 슬롯(slot 2, slot 3 등)에 범용 패스 상수 데이터를 바인딩 설정
	void SetPassConstant(uint32 slot, const void* data, uint32 size)
	{
		if (!data || size == 0) return;
		auto& buf = m_PassConstants[slot];
		buf.assign(reinterpret_cast<const uint8*>(data), reinterpret_cast<const uint8*>(data) + size);
	}

	template<typename T>
	void SetPassConstantStruct(uint32 slot, const T& data)
	{
		SetPassConstant(slot, &data, sizeof(T));
	}

	void ClearPassConstant(uint32 slot) { m_PassConstants.erase(slot); }
	void ClearAllPassConstants() { m_PassConstants.clear(); }
	bool HasPassConstant(uint32 slot) const { return m_PassConstants.find(slot) != m_PassConstants.end(); }
	const std::unordered_map<uint32, std::vector<uint8>>& GetAllPassConstants() const { return m_PassConstants; }
#pragma endregion

#pragma region Pass Textures
public:
	void SetPassTexture(uint32 slot, const wstring& renderTargetName) { m_PassTextures[slot] = renderTargetName; }
	void ClearPassTexture(uint32 slot) { m_PassTextures.erase(slot); }
	void ClearAllPassTextures() { m_PassTextures.clear(); }
	const std::unordered_map<uint32, wstring>& GetAllPassTextures() const { return m_PassTextures; }
#pragma endregion

#pragma region PrePass Copy
public:
	void SetPrePassCopy(const wstring& srcRT, const wstring& dstRT)
	{
		m_Desc.prePassCopySrcRT = srcRT;
		m_Desc.prePassCopyDstRT = dstRT;
	}
	void ClearPrePassCopy()
	{
		m_Desc.prePassCopySrcRT.clear();
		m_Desc.prePassCopyDstRT.clear();
	}
	bool HasPrePassCopy() const { return !m_Desc.prePassCopySrcRT.empty() && !m_Desc.prePassCopyDstRT.empty(); }
	const wstring& GetPrePassCopySource() const { return m_Desc.prePassCopySrcRT; }
	const wstring& GetPrePassCopyDest() const { return m_Desc.prePassCopyDstRT; }
#pragma endregion


#pragma region Variable
private:
	DESC m_Desc;
	std::unordered_map<uint32, std::vector<uint8>> m_PassConstants;
	std::unordered_map<uint32, wstring> m_PassTextures;
	bool m_PointSpriteSampling = false;
	bool m_SkipsRaytracedShadowCasters = false;
	bool m_SkipsRaytracedSpotCasters = false;
#pragma endregion
};
END