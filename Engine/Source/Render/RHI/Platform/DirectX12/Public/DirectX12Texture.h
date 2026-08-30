#pragma once

#include "DirectX12RHI.h"
#include "RHITexture.h"

struct DirectX12TextureDesc : public RHITextureDesc
{
	DirectX12TextureDesc() : RHITextureDesc() {}
	DirectX12TextureDesc(const RHITextureDesc& desc) : RHITextureDesc(desc) {}
	ID3D12Resource* nativeHandle = nullptr;
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = { 0 };
	D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON;
};

BEGIN(Engine)
class DirectX12Texture final : public RHITexture
{
#pragma region Constructor&Destructor
private:
	using DESC = DirectX12TextureDesc;
	DirectX12Texture(DirectX12RHI* rhi) : RHITexture{ rhi } {}
	virtual ~DirectX12Texture() = default;
	EResult Initialize(const DESC& desc);
public:
	static DirectX12Texture* Create(DirectX12RHI* rhi, const DESC& desc = {}, bool isOwned = true);
	virtual void Free() override;
#pragma endregion

#pragma region Bind
public:
	EResult Bind(uint32 slot = 0) override;
#pragma endregion

#pragma region Getter
public:
	D3D12_RESOURCE_STATES GetCurrentState() const { return m_CurrentState; }
	void SetCurrentState(D3D12_RESOURCE_STATES state) { m_CurrentState = state; }

	D3D12_CPU_DESCRIPTOR_HANDLE GetRTVHandle() const { return m_RTVHandle; }
	void SetRTVHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle) { m_RTVHandle = handle; }
	D3D12_CPU_DESCRIPTOR_HANDLE GetSRVHandle() const { return m_SRVHandle; }
	void SetSRVHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle) { m_SRVHandle = handle; }

	D3D12_GPU_DESCRIPTOR_HANDLE GetSRVGPUHandle() const { return m_SRVGPUHandle; }
	void SetSRVGPUHandle(D3D12_GPU_DESCRIPTOR_HANDLE handle) { m_SRVGPUHandle = handle; }
	
	D3D12_CPU_DESCRIPTOR_HANDLE GetDSVHandle() const { return m_DSVHandle; }
	void SetDSVHandle(D3D12_CPU_DESCRIPTOR_HANDLE handle) { m_DSVHandle = handle; }
#pragma endregion


public:
	virtual void* GetNativeHandle() const override { return m_Texture.Get(); }
	virtual void SetNativeHandle(void* nativeHandle) override
	{
		m_Texture = static_cast<ID3D12Resource*>(nativeHandle);
	}
private:
	ComPtr<ID3D12Resource> m_Texture = { nullptr };
	bool m_IsOwned = { true };

	D3D12_RESOURCE_STATES m_CurrentState = D3D12_RESOURCE_STATE_COMMON;

	D3D12_CPU_DESCRIPTOR_HANDLE m_RTVHandle = { 0 };
	D3D12_CPU_DESCRIPTOR_HANDLE m_SRVHandle = { 0 };
	D3D12_GPU_DESCRIPTOR_HANDLE m_SRVGPUHandle = { 0 };
	D3D12_CPU_DESCRIPTOR_HANDLE m_DSVHandle = { 0 };
};
END