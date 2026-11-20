#pragma once

#include "ResourceEditorInterface.h"
#include "ResourceHandle.h"
#include "Texture.h"
#include "TextureImporter.h"
#include <memory>

BEGIN(Editor)
class TextureEditor final : public ResourceEditorInterface
{
public:
	bool PrepareAssetPathRelocation(AssetPathRelocation& relocation) override;
	std::vector<std::filesystem::path> GetRetainedResourcePaths() const override;
	TextureEditor() { m_Name = L"Texture Editor"; m_DisplayNameKey = "ResourceEditor.TextureTitle"; }
	virtual ~TextureEditor() = default;
public:
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override;
	void SetTargetResource(const std::filesystem::path& assetPath) override;
	bool IsConversionOutput(const std::filesystem::path& path) const;
protected:
	void DrawCustomQuickAccess() override;
private:
	void DrawCustomContent() override;
	ImVec2 GetInitialResourceWindowSize() const override { return ImVec2(720.0f, 650.0f); }
	void ReleaseResourceDocument() override;
	void DrawTextureConversion();
	void DrawTextureConversionConfirmation();
	void StartTextureConversion();

	struct ConversionState
	{
		bool InProgress = false;
		std::filesystem::path SourcePath;
		std::filesystem::path BamtexPath;
		std::filesystem::path DdsPath;
		std::filesystem::path SpritePath;
	};
	ResourceHandle<Engine::Texture> m_TextureHandle;
	std::shared_ptr<ConversionState> m_Conversion = std::make_shared<ConversionState>();
	tagTextureImportDesc m_ImportDesc{};
	ETextureCompressionMode m_CompressionMode = ETextureCompressionMode::CpuStandard;
	bool m_ShowConversionConfirmation = false;
	bool m_AutoCreateSprite = false;
	float m_PreviewSize = 256.0f;
	std::filesystem::path m_PendingSource;
};
END