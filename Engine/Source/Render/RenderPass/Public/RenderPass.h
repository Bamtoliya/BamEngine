#pragma once

#include "Base.h"
#include "RenderTypes.h"

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
	bool IsAcceptsBlendMode(EBlendMode mode) const { return (m_Desc.acceptedBlendModes & mode) != EBlendMode::None; }
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
#pragma endregion

#pragma region Variable
private:
	DESC m_Desc;
#pragma endregion
};
END