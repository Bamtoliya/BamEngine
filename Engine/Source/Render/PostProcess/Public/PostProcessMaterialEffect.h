#pragma once

#include "PostProcessInterface.h"
#include "MaterialInterface.h"

BEGIN(Engine)

class RHIPipeline;

class ENGINE_API PostProcessMaterialEffect : public IPostProcessEffect
{
public:
	PostProcessMaterialEffect(const wstring& name, const ResourceHandle<MaterialInterface>& material, const string& displayNameKey = "")
		: IPostProcessEffect(name, displayNameKey), m_Material(material) {
	}
	virtual ~PostProcessMaterialEffect() = default;

public:
	virtual void Initialize(const wstring& prefix) override;
	virtual void Free() override;
	virtual EResult SubmitPass(f32 dt, RenderPassID passID, const wstring& sourceRT) override;

public:
	ResourceHandle<MaterialInterface> GetMaterial() const { return m_Material; }
	void SetMaterial(const ResourceHandle<MaterialInterface>& material) { m_Material = material; }
	void UpdateParameterBuffer();

public:
	virtual const std::unordered_map<string, MaterialParameter>& GetParameters() const override
	{
		static const std::unordered_map<string, MaterialParameter> s_Empty;
		return m_Material ? m_Material->GetParameters() : s_Empty;
	}
	virtual void SetFloat(const string& name, f32 value) override { if (m_Material) m_Material->SetFloat(name, value); }
	virtual void SetFloat2(const string& name, const vec2& value) override { if (m_Material) m_Material->SetFloat2(name, value); }
	virtual void SetFloat3(const string& name, const vec3& value) override { if (m_Material) m_Material->SetFloat3(name, value); }
	virtual void SetFloat4(const string& name, const vec4& value) override { if (m_Material) m_Material->SetFloat4(name, value); }
	virtual void SetInt(const string& name, int32 value) override { if (m_Material) m_Material->SetInt(name, value); }
	virtual void SetBool(const string& name, bool value) override { if (m_Material) m_Material->SetBool(name, value); }
	virtual void ResetParameter(const string& name) override { if (m_Material) m_Material->ResetParameter(name); }
	virtual void ResetAllParameters() override { if (m_Material) m_Material->ResetAllParameters(); }

private:
	ResourceHandle<MaterialInterface> m_Material;
	RHIPipeline* m_Pipeline = nullptr;
	std::vector<uint8> m_ParamBuffer;
};

END