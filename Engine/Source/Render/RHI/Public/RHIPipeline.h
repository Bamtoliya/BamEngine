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
	EBlendMode BlendMode =	EBlendMode::Opaque;
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
	ERenderTargetFormat ColorAttachmentFormats[MAX_RENDER_TARGET_COUNT] = { ERenderTargetFormat::RTF_RGBA8, };
	ERenderTargetFormat DepthStencilAttachmentFormat = ERenderTargetFormat::RTF_DEPTH24STENCIL8;

	RHIDepthStencilState DepthStencilState;

	tagInputLayoutDesc InputLayout;

	bool operator==(const tagRHIPipelineDesc& other) const
	{
		if (PipelineType != other.PipelineType) return false;
		if (BlendMode != other.BlendMode) return false;
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

		if (!(InputLayout == other.InputLayout)) return false;

		return true;
	}
};

inline void HashCombine(std::size_t& seed, std::size_t value)
{
	seed ^= value + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

// std::hash 특수화 (namespace Engine 밖, 전역 범위에 있어야 함)
namespace std
{
	template<>
	struct hash<tagRHIPipelineDesc>
	{
		size_t operator()(const tagRHIPipelineDesc& desc) const
		{
			size_t seed = 0;
			// 주요 필드들을 해싱합니다.
			// 주의: 구조체 패딩 이슈를 피하기 위해 멤버별로 해싱하는 것이 안전합니다.
			HashCombine(seed, hash<int>()((int)desc.PipelineType));
			HashCombine(seed, hash<int>()((int)desc.BlendMode));
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

			HashCombine(seed, hash<uint32>()(desc.InputLayout.Stride));
			HashCombine(seed, hash<size_t>()(desc.InputLayout.Elements.size())); // 속성 개수 해싱

			for (const auto& attr : desc.InputLayout.Elements)
			{
				// 각 속성의 세부 정보(Location, Format, Offset)를 섞어줌
				HashCombine(seed, hash<uint32>()(attr.Location));
				HashCombine(seed, hash<int>()((int)attr.Format));
				HashCombine(seed, hash<uint32>()(attr.Offset));
			}

			return seed;
		}
	};
}

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
	EBlendMode GetBlendMode() const { return m_Desc.BlendMode; }
	EFillMode GetFillMode() const { return m_Desc.FillMode; }
	ECullMode GetCullMode() const { return m_Desc.CullMode; }
	EFrontFace GetFrontFace() const { return m_Desc.FrontFace; }
	ETopology GetTopology() const { return m_Desc.Topology; }

	void SetPipelineType(EPipelineType type) { m_Desc.PipelineType = type; }
	void SetBlendMode(EBlendMode mode) { m_Desc.BlendMode = mode; }
	void SetFillMode(EFillMode mode) { m_Desc.FillMode = mode; }
	void SetCullMode(ECullMode mode) { m_Desc.CullMode = mode; }
	void SetFrontFace(EFrontFace face) { m_Desc.FrontFace = face; }
	void SetTopology(ETopology topology) { m_Desc.Topology = topology; }
protected:
	DESC m_Desc;
};
END