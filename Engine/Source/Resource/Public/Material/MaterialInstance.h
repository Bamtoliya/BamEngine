#pragma once

#include "Base.h"
#include "Material.h"

BEGIN(Engine)
class ENGINE_API MaterialInstance : public Base
{
#pragma region Constructor&Destructor
	private:
	MaterialInstance() {}
	virtual ~MaterialInstance() = default;
	EResult Initialize(void* arg = nullptr);
public:
	static MaterialInstance* Create(void* arg = nullptr);
	virtual MaterialInstance* Clone(void* arg = nullptr);
	virtual void Free() override;
#pragma endregion

#pragma region Setter
	public:
	void SetBaseMaterial(class Material* baseMaterial);
	void SetScalarParameter(const wstring& name, float value);
	void SetVectorParameter(const wstring& name, const Vector4& value);
	void SetTextureParameter(const wstring& name, class Texture* texture);
#pragma endregion

#pragma region Variable
private:
	class Material* m_BaseMaterial = { nullptr };
	unordered_map<wstring, f32> m_ScalarParameters;
	unordered_map<wstring, vec4> m_VectorParameters;
	unordered_map<wstring, class Texture*> m_TextureParameters;
#pragma endregion
}
END