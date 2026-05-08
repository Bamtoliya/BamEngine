#pragma once

#include "Base.h"
#include "RenderTypes.h"

BEGIN(Engine)
class ENGINE_API RenderPass : public Base
{
	using DESC = tagRenderPassDesc;
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
	const tagRenderPassDesc& GetDesc() const { return m_Desc; }
	const uint32 GetPriority() const { return m_Desc.Priority; }
	const RenderPassID GetID() const { return m_Desc.ID; }
	const wstring& GetName() const { return m_Desc.Name; }
	const uint32 GetRenderTargetCount() const { return static_cast<uint32>(m_Desc.RenderTargetNames.size()); }
	const wstring& GetRenderTargetName(uint32 index) const { return m_Desc.RenderTargetNames[index]; }
	const wstring& GetDepthStencilName() const { return m_Desc.DepthStencilName; }
	const ERenderSortType GetSortType() const { return m_Desc.SortType; }
	const ERenderPassLoadOperation GetLoadOperation() const { return m_Desc.LoadOperation; }
	const ERenderPassStoreOperation GetStoreOperation() const { return m_Desc.StoreOperation; }		
	const ERenderPassLoadOperation GetStencilLoadOperation() const { return m_Desc.StencilLoadOperation; }
	const ERenderPassStoreOperation GetStencilStoreOperation() const { return m_Desc.StencilStoreOperation; }	
	const vec4& GetOverrideClearColor() const { return m_Desc.OverrideClearColor; }
	bool HasOverrideClearColor() const { return m_Desc.OverrideClearColor.a >= 0.0f; }
	const ERenderPassType GetPassType() const { return m_Desc.PassType; }
	EBlendMode GetAcceptedBlendModes() const { return m_Desc.AcceptedBlendModes; }
	bool IsAcceptsBlendMode(EBlendMode mode) const { return (m_Desc.AcceptedBlendModes & mode) != EBlendMode::None; }
#pragma endregion

#pragma region Setter
public:
	void SetPriority(uint32 priority) { m_Desc.Priority = priority; }
	void SetID(RenderPassID id) { m_Desc.ID = id; }
	void SetName(const wstring& name) { m_Desc.Name = name; }
	void SetSortType(ERenderSortType sortType) { m_Desc.SortType = sortType; }
	void AddColorAttachment(const wstring& rtName) { m_Desc.RenderTargetNames.push_back(rtName); }
	void ClearColorAttachments() { m_Desc.RenderTargetNames.clear(); }
	void SetColorAttachments(const std::vector<wstring>& rtNames) { m_Desc.RenderTargetNames = rtNames; }
	void SetDepthStencilAttachment(const wstring& dsName) { m_Desc.DepthStencilName = dsName; }
	void SetLoadOperation(ERenderPassLoadOperation loadOp) { m_Desc.LoadOperation = loadOp; }
	void SetStoreOperation(ERenderPassStoreOperation storeOp) { m_Desc.StoreOperation = storeOp; }
	void SetStencilLoadOperation(ERenderPassLoadOperation loadOp) { m_Desc.StencilLoadOperation = loadOp; }
	void SetStencilStoreOperation(ERenderPassStoreOperation storeOp) { m_Desc.StencilStoreOperation = storeOp; }
	void SetOverrideClearColor(const vec4& color) { m_Desc.OverrideClearColor = color; }
	void SetPassType(ERenderPassType passType) { m_Desc.PassType = passType; }
	void SetAcceptedBlendModes(EBlendMode blendModes) { m_Desc.AcceptedBlendModes = blendModes; }
#pragma endregion

#pragma region Variable
private:
	DESC m_Desc;
#pragma endregion
};
END