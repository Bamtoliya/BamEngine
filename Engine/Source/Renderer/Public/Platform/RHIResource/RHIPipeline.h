#pragma once

#include "RHIResource.h"
#include "RHIShader.h"

enum class EPipelineType
{
	Graphics,
	Compute,
};

enum class EPipelineFillMode
{
	Wireframe,
	Solid,
};

enum class EPipelineCullMode
{
	None,
	Front,
	Back,
};

enum class EPipelineFrontFace
{
	Clockwise,
	CounterClockwise,
};

enum class EPipelineTopology
{
	TriangleList,
	TriangleStrip,
	LineList,
	LineStrip,
	PointList,
};

enum class EPipelineBlendMode
{
	Opaque,
	AlphaBlend,
	Additive,
	NonPremultiplied,
};

struct tagRHIPipelineDesc
{
	EPipelineType PipelineType = EPipelineType::Graphics;
	EPipelineBlendMode BlendMode = EPipelineBlendMode::Opaque;
	EPipelineFillMode FillMode = EPipelineFillMode::Solid;
	EPipelineCullMode CullMode = EPipelineCullMode::Back;
	EPipelineFrontFace FrontFace = EPipelineFrontFace::CounterClockwise;
	EPipelineTopology Topology = EPipelineTopology::TriangleList;
	class RHIShader* VertexShader = nullptr;
	class RHIShader* PixelShader = nullptr;
	class RHIShader* ComputeShader = nullptr;
	class RHIShader* GeometryShader = nullptr;
	class RHIShader* HullShader = nullptr;
	class RHIShader* DomainShader = nullptr;
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
	EPipelineBlendMode GetBlendMode() const { return m_Desc.BlendMode; }
	EPipelineFillMode GetFillMode() const { return m_Desc.FillMode; }
	EPipelineCullMode GetCullMode() const { return m_Desc.CullMode; }
	EPipelineFrontFace GetFrontFace() const { return m_Desc.FrontFace; }
	EPipelineTopology GetTopology() const { return m_Desc.Topology; }

	void SetPipelineType(EPipelineType type) { m_Desc.PipelineType = type; }
	void SetBlendMode(EPipelineBlendMode mode) { m_Desc.BlendMode = mode; }
	void SetFillMode(EPipelineFillMode mode) { m_Desc.FillMode = mode; }
	void SetCullMode(EPipelineCullMode mode) { m_Desc.CullMode = mode; }
	void SetFrontFace(EPipelineFrontFace face) { m_Desc.FrontFace = face; }
	void SetTopology(EPipelineTopology topology) { m_Desc.Topology = topology; }
protected:
	DESC m_Desc;
};
END