#pragma once

#include "ResourceEditorInterface.h"
#include "MaterialInterface.h"
#include "MaterialPreviewRenderer.h"
#include <string>

BEGIN(Engine)
class MaterialInterface;
END

BEGIN(Editor)
class MaterialEditor final : public ResourceEditorInterface
{
public:
	MaterialEditor();
	virtual ~MaterialEditor() = default;

public:
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override;
	virtual void SetTargetResource(const std::filesystem::path& assetPath) override;
	virtual void Update(f32 dt) override; // 렌더 패스 실행 전 커맨드 사전 제출용
	virtual void Draw() override;

private:
	void DrawToolbar();
	void DrawPipelineSettings();
	void DrawShaderSettings();
	void DrawTextureBindings();
	void DrawParameters();

private:
	Engine::ResourceHandle<Engine::MaterialInterface> m_MaterialHandle;
	Engine::MaterialInterface* m_Material = nullptr;
	bool m_IsDirty = false;

	// 새 파라미터 추가용 입력 버퍼
	char m_NewParamName[128] = "";
	int m_NewParamTypeIndex = 0;

	// 새 텍스처 바인딩 추가용 입력 버퍼
	char m_NewTextureSlotName[128] = "";
	int m_NewTextureSlotNumber = 0;

	int m_NewPassTypeIndex = 3;

	// 3D 머티리얼 프리뷰 렌더러
	MaterialPreviewRenderer m_PreviewRenderer;
};
END