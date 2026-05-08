#pragma once

#include "RHIResource.h"
#include "RHIShader.h"

struct RHIDepthStencilState
{
	bool DepthTestEnable = true;
	bool DepthWriteEnable = true;
	ECompareOp DepthCompareOp = ECompareOp::Less;
	bool StencilTestEnable = false;
};

struct tagRHIPipelineDesc
{
	EPipelineType PipelineType = EPipelineType::Graphics;
	tagBlendState BlendState = {};
	EFillMode FillMode =	EFillMode::Solid;
	ECullMode CullMode =	ECullMode::Back;
	EFrontFace FrontFace =	EFrontFace::CounterClockwise;
	ETopology Topology =	ETopology::TriangleList;
	class RHIShader* VertexShader = nullptr;
	class RHIShader* PixelShader = nullptr;
	class RHIShader* ComputeShader = nullptr;
	class RHIShader* GeometryShader = nullptr;
	class RHIShader* HullShader = nullptr;
	class RHIShader* DomainShader = nullptr;

	uint32 ColorAttachmentCount = 1;
	ETextureFormat ColorAttachmentFormats[MAX_RENDER_TARGET_COUNT] = { ETextureFormat::R8G8B8A8_UNORM, };
	ETextureFormat DepthStencilAttachmentFormat = ETextureFormat::D24_UNORM_S8_UINT;

	RHIDepthStencilState DepthStencilState;

	vector<tagInputLayoutDesc> InputLayouts;

	bool operator==(const tagRHIPipelineDesc& other) const
	{
		if (PipelineType != other.PipelineType) return false;

		if (BlendState != other.BlendState) return false;

		if (FillMode != other.FillMode) return false;
		if (CullMode != other.CullMode) return false;
		if (FrontFace != other.FrontFace) return false;
		if (Topology != other.Topology) return false;

		if (VertexShader != other.VertexShader) return false;
		if (PixelShader != other.PixelShader) return false;
		if (ComputeShader != other.ComputeShader) return false;
		if (GeometryShader != other.GeometryShader) return false;
		if (HullShader != other.HullShader) return false;
		if (DomainShader != other.DomainShader) return false;

		if (ColorAttachmentCount != other.ColorAttachmentCount) return false;
		for (uint32 i = 0; i < ColorAttachmentCount; ++i)
		{
			if (ColorAttachmentFormats[i] != other.ColorAttachmentFormats[i]) return false;
		}

		if (DepthStencilAttachmentFormat != other.DepthStencilAttachmentFormat) return false;

		if (DepthStencilState.DepthTestEnable != other.DepthStencilState.DepthTestEnable) return false;
		if (DepthStencilState.DepthWriteEnable != other.DepthStencilState.DepthWriteEnable) return false;

		if (InputLayouts.size() != other.InputLayouts.size()) return false;
		for (size_t i = 0; i < InputLayouts.size(); ++i)
		{
			if (!(InputLayouts[i] == other.InputLayouts[i])) return false;
		}

		return true;
	}
};

template<>
struct hash<tagRHIPipelineDesc>
{
	size_t operator()(const tagRHIPipelineDesc& desc) const
	{
		size_t seed = 0;
		// 주요 필드들을 해싱합니다.
		// 주의: 구조체 패딩 이슈를 피하기 위해 멤버별로 해싱하는 것이 안전합니다.
		HashCombine(seed, hash<int>()((int)desc.PipelineType));

		HashCombine(seed, hash<bool>()(desc.BlendState.Enable));
		HashCombine(seed, hash<bool>()(desc.BlendState.EnableColorWriteMask));

		HashCombine(seed, hash<int>()((int)desc.BlendState.SrcColor));
		HashCombine(seed, hash<int>()((int)desc.BlendState.DstColor));
		HashCombine(seed, hash<int>()((int)desc.BlendState.ColorBlendOp));
		HashCombine(seed, hash<int>()((int)desc.BlendState.SrcAlpha));
		HashCombine(seed, hash<int>()((int)desc.BlendState.DstAlpha));
		HashCombine(seed, hash<int>()((int)desc.BlendState.AlphaBlendOp));
		HashCombine(seed, hash<int>()((int)desc.BlendState.ColorWriteMask));


		HashCombine(seed, hash<int>()((int)desc.CullMode));
		HashCombine(seed, hash<void*>()(desc.VertexShader));
		HashCombine(seed, hash<void*>()(desc.PixelShader));

		// 포맷들도 해싱에 포함
		HashCombine(seed, hash<int>()(desc.ColorAttachmentCount));
		for (uint32 i = 0; i < desc.ColorAttachmentCount; ++i)
		{
			HashCombine(seed, hash<int>()((int)desc.ColorAttachmentFormats[i]));
		}
		HashCombine(seed, hash<int>()((int)desc.DepthStencilAttachmentFormat));

		// Depth State
		HashCombine(seed, hash<bool>()(desc.DepthStencilState.DepthTestEnable));
		HashCombine(seed, hash<bool>()(desc.DepthStencilState.DepthWriteEnable));

		HashCombine(seed, hash<size_t>()(desc.InputLayouts.size()));
		for (const auto& layout : desc.InputLayouts)
		{
			HashCombine(seed, hash<uint32>()(layout.Stride));
			for (const auto& attr : layout.Elements)
			{
				HashCombine(seed, hash<uint32>()(attr.Location));
				HashCombine(seed, hash<int>()((int)attr.Format));
				HashCombine(seed, hash<uint32>()(attr.Offset));
			}
		}

		return seed;
	}
};

BEGIN(Engine)
class ENGINE_API RHIPipeline abstract : public RHIResource
{
protected:
	using DESC = tagRHIPipelineDesc;
	RHIPipeline(RHI* rhi) : RHIResource(rhi, ERHIResourceType::Pipeline) {}
	RHIPipeline(RHI* rhi, const DESC& desc) : RHIResource(rhi, ERHIResourceType::Pipeline), m_Desc(desc) {}
	virtual ~RHIPipeline() = default;
public:
	EPipelineType GetPipelineType() const { return m_Desc.PipelineType; }
	tagBlendState GetBlendState() const { return m_Desc.BlendState; }
	EFillMode GetFillMode() const { return m_Desc.FillMode; }
	ECullMode GetCullMode() const { return m_Desc.CullMode; }
	EFrontFace GetFrontFace() const { return m_Desc.FrontFace; }
	ETopology GetTopology() const { return m_Desc.Topology; }

	void SetPipelineType(EPipelineType type) { m_Desc.PipelineType = type; }
	void SetBlendState(const tagBlendState& blendState) { m_Desc.BlendState = blendState; }
	void SetFillMode(EFillMode mode) { m_Desc.FillMode = mode; }
	void SetCullMode(ECullMode mode) { m_Desc.CullMode = mode; }
	void SetFrontFace(EFrontFace face) { m_Desc.FrontFace = face; }
	void SetTopology(ETopology topology) { m_Desc.Topology = topology; }
protected:
	DESC m_Desc;
};
END