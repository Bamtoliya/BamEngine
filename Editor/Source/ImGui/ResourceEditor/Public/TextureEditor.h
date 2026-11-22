#pragma once

#include "ResourceEditorInterface.h"
#include "ResourceHandle.h"
#include "Texture.h"
#include "TextureImporter.h"
#include <memory>
#include <chrono>

BEGIN(Editor)
class TextureEditor final : public ResourceEditorInterface
{
public:
	bool PrepareAssetPathRelocation(AssetPathRelocation& relocation) override;
	std::vector<std::filesystem::path> GetRetainedResourcePaths() const override;
	TextureEditor() { m_Name = L"Texture Editor"; m_DisplayNameKey = "ResourceEditor.TextureTitle"; }
	virtual ~TextureEditor() override;
public:
	virtual bool IsSupported(const std::filesystem::path& assetPath) const override;
	void SetTargetResource(const std::filesystem::path& assetPath) override;
	bool BeginAsyncOpen(const std::filesystem::path& assetPath) override;
	bool StepAsyncOpen(float maxTimeSeconds = 0.002f) override;
	bool IsAsyncLoading() const override { return m_AsyncStage != EAsyncStage::None; }
	void DrawAsyncLoadingOverlay() override;
	void CancelAsyncOpen() override;
	bool IsConversionOutput(const std::filesystem::path& path) const;
protected:
	void DrawCustomQuickAccess() override;
private:
	void ApplyLoadedTexture(const std::filesystem::path& path, ResourceHandle<Engine::Texture> handle);
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

	// 비동기 열기 상태
	enum class EAsyncStage
	{
		None,
		WaitingBackground,
		LoadingResource,
		Finalizing,
		Failed
	};
	EAsyncStage m_AsyncStage = EAsyncStage::None;
	float m_AsyncProgress = 0.0f;
	std::filesystem::path m_AsyncTargetPath;
	ResourceHandle<Engine::Texture> m_AsyncPendingTexture;
	std::chrono::steady_clock::time_point m_AsyncStartTime{};
	std::chrono::steady_clock::time_point m_AsyncLoadStart{};
	double m_AsyncLoadDurationMs = 0.0;
};
END