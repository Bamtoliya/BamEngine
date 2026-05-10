#pragma once 
#include "RHI.h"

class DirectX12RHI : public RHI
{
private:
	DirectX12RHI() = default;
	virtual ~DirectX12RHI() = default;
	EResult Initialize(void* arg) override;
public:
	static DirectX12RHI* Create(void* arg = nullptr);
	virtual void Free() override;
};