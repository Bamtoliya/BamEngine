#pragma once

#include "RHIResource.h"
#include "RHIShader.h"

struct RHIDepthStencilState
{
	bool depthTestEnable = true;
	bool depthWriteEnable = true;
	ECompareOp depthCompareOp = ECompareOp::Less;

	bool stencilTestEnable = false;
	uint8 stencilReadMask = 0xFF;
	uint8 stencilWriteMask = 0xFF;

	EStencilOp stencilFailOp = EStencilOp::Keep;
	EStencilOp stencilDepthFailOp = EStencilOp::Keep;
	EStencilOp stencilPassOp = EStencilOp::Keep;
	ECompareOp stencilCompareOp = ECompareOp::Always;
};

struct RHIPipelineDesc
{
	EPipelineType pipelineType = EPipelineType::Graphics;
	BlendState blendState = {};
	EFillMode fillMode =	EFillMode::Solid;
	ECullMode cullMode =	ECullMode::Back;
	EFrontFace frontFace =	EFrontFace::Clockwise;
	ETopology topology =	ETopology::TriangleList;
	class RHIShader* vertexShader = nullptr;
	class RHIShader* pixelShader = nullptr;
	class RHIShader* computeShader = nullptr;
	class RHIShader* geometryShader = nullptr;
	class RHIShader* hullShader = nullptr;
	class RHIShader* domainShader = nullptr;

	uint32 colorAttachmentCount = 1;
	ETextureFormat colorAttachmentFormats[MAX_RENDER_TARGET_COUNT] = { ETextureFormat::R8G8B8A8_UNORM, };
	ETextureFormat depthStencilAttachmentFormat = ETextureFormat::D24_UNORM_S8_UINT;

	RHIDepthStencilState depthStencilState;

	vector<InputLayoutDesc> inputLayouts;

	bool operator==(const RHIPipelineDesc& other) const
	{
		if (pipelineType != other.pipelineType) return false;

		if (blendState != other.blendState) return false;

		if (fillMode != other.fillMode) return false;
		if (cullMode != other.cullMode) return false;
		if (frontFace != other.frontFace) return false;
		if (topology != other.topology) return false;

		if (vertexShader != other.vertexShader) return false;
		if (pixelShader != other.pixelShader) return false;
		if (computeShader != other.computeShader) return false;
		if (geometryShader != other.geometryShader) return false;
		if (hullShader != other.hullShader) return false;
		if (domainShader != other.domainShader) return false;

		if (colorAttachmentCount != other.colorAttachmentCount) return false;
		for (uint32 i = 0; i < colorAttachmentCount; ++i)
		{
			if (colorAttachmentFormats[i] != other.colorAttachmentFormats[i]) return false;
		}

		if (depthStencilAttachmentFormat != other.depthStencilAttachmentFormat) return false;

		if (depthStencilState.depthTestEnable != other.depthStencilState.depthTestEnable) return false;
		if (depthStencilState.depthWriteEnable != other.depthStencilState.depthWriteEnable) return false;

		if (inputLayouts.size() != other.inputLayouts.size()) return false;
		for (size_t i = 0; i < inputLayouts.size(); ++i)
		{
			if (!(inputLayouts[i] == other.inputLayouts[i])) return false;
		}

		return true;
	}
};

template<>
struct hash<RHIPipelineDesc>
{
	size_t operator()(const RHIPipelineDesc& desc) const
	{
		size_t seed = 0;
		// 주요 필드들을 해싱합니다.
		// 주의: 구조체 패딩 이슈를 피하기 위해 멤버별로 해싱하는 것이 안전합니다.
		HashCombine(seed, hash<int>()((int)desc.pipelineType));

		HashCombine(seed, hash<bool>()(desc.blendState.enable));
		HashCombine(seed, hash<bool>()(desc.blendState.enableColorWriteMask));

		HashCombine(seed, hash<int>()((int)desc.blendState.srcColor));
		HashCombine(seed, hash<int>()((int)desc.blendState.dstColor));
		HashCombine(seed, hash<int>()((int)desc.blendState.colorBlendOp));
		HashCombine(seed, hash<int>()((int)desc.blendState.srcAlpha));
		HashCombine(seed, hash<int>()((int)desc.blendState.dstAlpha));
		HashCombine(seed, hash<int>()((int)desc.blendState.alphaBlendOp));
		HashCombine(seed, hash<int>()((int)desc.blendState.colorWriteMask));


		HashCombine(seed, hash<int>()((int)desc.cullMode));
		HashCombine(seed, hash<void*>()(desc.vertexShader));
		HashCombine(seed, hash<void*>()(desc.pixelShader));

		// 포맷들도 해싱에 포함
		HashCombine(seed, hash<int>()(desc.colorAttachmentCount));
		for (uint32 i = 0; i < desc.colorAttachmentCount; ++i)
		{
			HashCombine(seed, hash<int>()((int)desc.colorAttachmentFormats[i]));
		}
		HashCombine(seed, hash<int>()((int)desc.depthStencilAttachmentFormat));

		// Depth State
		HashCombine(seed, hash<bool>()(desc.depthStencilState.depthTestEnable));
		HashCombine(seed, hash<bool>()(desc.depthStencilState.depthWriteEnable));

		HashCombine(seed, hash<size_t>()(desc.inputLayouts.size()));
		for (const auto& layout : desc.inputLayouts)
		{
			HashCombine(seed, hash<uint32>()(layout.stride));
			for (const auto& attr : layout.elements)
			{
				HashCombine(seed, hash<uint32>()(attr.location));
				HashCombine(seed, hash<int>()((int)attr.format));
				HashCombine(seed, hash<uint32>()(attr.offset));
			}
		}

		return seed;
	}
};

BEGIN(Engine)
class ENGINE_API RHIPipeline abstract : public RHIResource
{
protected:
	using DESC = RHIPipelineDesc;
	RHIPipeline(RHI* rhi) : RHIResource(rhi, ERHIResourceType::Pipeline) {}
	RHIPipeline(RHI* rhi, const DESC& desc) : RHIResource(rhi, ERHIResourceType::Pipeline), m_Desc(desc) {}
	virtual ~RHIPipeline() = default;
public:
	EPipelineType GetPipelineType() const { return m_Desc.pipelineType; }
	BlendState GetBlendState() const { return m_Desc.blendState; }
	EFillMode GetFillMode() const { return m_Desc.fillMode; }
	ECullMode GetCullMode() const { return m_Desc.cullMode; }
	EFrontFace GetFrontFace() const { return m_Desc.frontFace; }
	ETopology GetTopology() const { return m_Desc.topology; }

	void SetPipelineType(EPipelineType type) { m_Desc.pipelineType = type; }
	void SetBlendState(const BlendState& blendState) { m_Desc.blendState = blendState; }
	void SetFillMode(EFillMode mode) { m_Desc.fillMode = mode; }
	void SetCullMode(ECullMode mode) { m_Desc.cullMode = mode; }
	void SetFrontFace(EFrontFace face) { m_Desc.frontFace = face; }
	void SetTopology(ETopology topology) { m_Desc.topology = topology; }
protected:
	DESC m_Desc;
};
END