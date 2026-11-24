#pragma	once

#include "TextureInspector.h"
#include "EditorUI.h"
#include <DirectXTex.h>
#include "TextureImporter.h"
#include "TextureCompressorInterface.h"
#include "AssetManager.h"

static Engine::ETextureFormat s_TargetFormat = Engine::ETextureFormat::BC1_RGBA_UNORM;
static uint32 s_CompressFlagsValue = static_cast<uint32>(ECompressFlags::GenerateMipMaps | ECompressFlags::HighQuality);
static bool s_AutoGenerateSprite = true;
static ETextureCompressionMode s_CompressionMode = ETextureCompressionMode::CpuStandard;

static bool s_IsImporting = false;       // 비동기 작업 진행 중 여부
static bool s_ShowSuccessPopup = false;  // 작업 완료 팝업 트리거
static bool s_ImportSucceeded = false;
static bool s_ImportedSprite = false;

bool TextureInspector::IsSupported(const std::filesystem::path& assetPath)
{
	auto ext = assetPath.extension().wstring();
	std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);
	return ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".tga" || ext == L".bmp" || ext == L".dds";
}

bool TextureInspector::OnInspectorGUI(const std::filesystem::path& assetPath)
{

    ImGui::TextColored(ImVec4(0.2f, 0.7f, 1.0f, 1.0f), "%s", LOCAL_CSTR("TextureInspector.Title"));
    void* previewID = AssetCache::Get().GetThumbnail(assetPath);

    if (previewID)
    {
        ImGui::Image(previewID, ImVec2(128, 128));
    }
    ImGui::Separator();

    // 1. 파일 기본 정보
    ImGui::Text("%s", LOCAL_FMT("TextureInspector.FileName", assetPath.filename().string().c_str()).c_str());

    auto ext = assetPath.extension().wstring();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::towlower);

    // 2. DirectXTex를 이용한 실제 메타데이터 추출 (WIC / TGA / DDS 포맷 분기)
    DirectX::TexMetadata meta;
    HRESULT hr;
    if (ext == L".dds")
    {
        hr = DirectX::GetMetadataFromDDSFile(assetPath.wstring().c_str(), DirectX::DDS_FLAGS_NONE, meta);
    }
    else if (ext == L".tga")
    {
        hr = DirectX::GetMetadataFromTGAFile(assetPath.wstring().c_str(), DirectX::TGA_FLAGS_NONE, meta);
    }
    else
    {
        hr = DirectX::GetMetadataFromWICFile(assetPath.wstring().c_str(), DirectX::WIC_FLAGS_NONE, meta);
    }

    if (SUCCEEDED(hr)) {
        ImGui::Text("%s", LOCAL_FMT("TextureInspector.Resolution", meta.width, meta.height).c_str());
        ImGui::Text("%s", LOCAL_FMT("TextureInspector.MipLevels", meta.mipLevels).c_str());
        ImGui::Text("%s", LOCAL_FMT("TextureInspector.Dimension",
            (meta.dimension == DirectX::TEX_DIMENSION_TEXTURE2D) ? "2D" : LOCAL_CSTR("EditorCommon.Other")).c_str());
    }
    else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "%s", LOCAL_CSTR("TextureInspector.MetadataFailed"));
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("%s", LOCAL_CSTR("TextureInspector.ImportSettings"));

    // 3. Target Format 선택 (Combo)
    const string formatLabels[] = { "BC1 (DXT1)", "BC2 (DXT3)", "BC3 (DXT5)",
        LOCAL("TextureInspector.BC4"), LOCAL("TextureInspector.BC5"), LOCAL("TextureInspector.BC7") };
    const char* formatNames[] = { formatLabels[0].c_str(), formatLabels[1].c_str(), formatLabels[2].c_str(),
        formatLabels[3].c_str(), formatLabels[4].c_str(), formatLabels[5].c_str() };
    Engine::ETextureFormat formatValues[] = {
        Engine::ETextureFormat::BC1_RGBA_UNORM,
        Engine::ETextureFormat::BC2_RGBA_UNORM,
        Engine::ETextureFormat::BC3_RGBA_UNORM,
        Engine::ETextureFormat::BC4_R_UNORM,
        Engine::ETextureFormat::BC5_RG_UNORM,
        Engine::ETextureFormat::BC7_RGBA_UNORM
    };

    int currentFormatIdx = 5; // Default BC7
    for (int i = 0; i < 6; ++i) {
        if (formatValues[i] == s_TargetFormat) {
            currentFormatIdx = i;
            break;
        }
    }

    if (ImGui::Combo(LOCAL_LABEL("TextureInspector.TargetFormat").c_str(), &currentFormatIdx, formatNames,
        IM_ARRAYSIZE(formatNames)))
    {
        s_TargetFormat = formatValues[currentFormatIdx];
    }

    ImGui::Spacing();
    ImGui::Text("%s", LOCAL_CSTR("TextureInspector.CompressionFlags"));

    // 4. ECompressFlags 개별 체크박스 제어
    auto DrawFlag = [](const char* key, ECompressFlags flag) {
        bool isSet = (s_CompressFlagsValue & static_cast<uint32>(flag)) != 0;
        if (ImGui::Checkbox(LOCAL_LABEL(key).c_str(), &isSet)) {
            if (isSet) s_CompressFlagsValue |= static_cast<uint32>(flag);
            else s_CompressFlagsValue &= ~static_cast<uint32>(flag);
        }
        };

    DrawFlag("TextureInspector.GenerateMipMaps", ECompressFlags::GenerateMipMaps);
    DrawFlag("TextureInspector.HighQuality", ECompressFlags::HighQuality);
    const string modeLabels[] = {
        LOCAL("TextureInspector.CompressionCpuStandard"), LOCAL("TextureInspector.CompressionCpuFast"),
        LOCAL("TextureInspector.CompressionGpu")
    };
    const char* modes[] = { modeLabels[0].c_str(), modeLabels[1].c_str(), modeLabels[2].c_str() };
    int mode = static_cast<int>(s_CompressionMode);
    if (ImGui::Combo(LOCAL_LABEL("TextureInspector.CompressionMode").c_str(), &mode, modes, IM_ARRAYSIZE(modes)))
        s_CompressionMode = static_cast<ETextureCompressionMode>(mode);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("%s", LOCAL_CSTR("TextureInspector.CompressionModeTooltip"));
    DrawFlag("TextureInspector.SRGB", ECompressFlags::SRGB);
    DrawFlag("TextureInspector.FlipY", ECompressFlags::FilpY);
    DrawFlag("TextureInspector.PremultiplyAlpha", ECompressFlags::PremultiplyAlpha);
    DrawFlag("TextureInspector.NormalMap", ECompressFlags::NormalMap);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("%s", LOCAL_CSTR("TextureInspector.SpriteGeneration"));

    // 스프라이트 자동 생성 체크박스
    ImGui::Checkbox(LOCAL_LABEL("TextureInspector.AutoSprite").c_str(), &s_AutoGenerateSprite);

    ImGui::Spacing();

    // 5. 실행 버튼
    EditorUI::TextButtonOptions importBtnOpt;
    importBtnOpt.FullWidth = true;
    if (s_IsImporting)
    {
        // 작업 중일 때는 중복 클릭 방지를 위해 버튼 비활성화
        importBtnOpt.Enabled = false;
        EditorUI::TextButton("TextureImporting", "TextureInspector.Importing", importBtnOpt);
    }
    else
    {
        importBtnOpt.Enabled = (ext != L".dds");
        if (ext == L".dds")
            importBtnOpt.TooltipKey = "TextureInspector.DdsImportUnsupported";
        if (EditorUI::TextButton("TextureImport", "TextureInspector.Import", importBtnOpt))
        {
            std::error_code error;
            const std::filesystem::path sourcePath = std::filesystem::absolute(assetPath, error);
            if (error || sourcePath.empty())
            {
                BAM_LOG(Error, "Editor", "Asset", "Failed to resolve texture import path: {}", error.message());
                s_ImportSucceeded = false;
                s_ShowSuccessPopup = true;
                return true;
            }
            const std::filesystem::path destDir = sourcePath.parent_path();

            bool autoGenSprite = s_AutoGenerateSprite;
            s_IsImporting = true;

            tagTextureImportDesc desc = {};
            desc.TargetFormat = s_TargetFormat;
            desc.CompressFlags = ApplyTextureCompressionMode(
                static_cast<ECompressFlags>(s_CompressFlagsValue), s_CompressionMode, desc.TargetFormat);

            AssetManager::Get().ImportAsync(sourcePath, destDir, &desc,
                [sourcePath, autoGenSprite](EResult result)
                {
                    s_IsImporting = false;
                    s_ImportSucceeded = false;
                    s_ImportedSprite = autoGenSprite;
                    s_ShowSuccessPopup = true;
                    std::filesystem::path outputPath = sourcePath;
                    outputPath.replace_extension(".bamtex");
                    if (result == EResult::Success && autoGenSprite)
                        result = AssetManager::Get().Import(outputPath, outputPath, nullptr);
                    s_ImportSucceeded = result == EResult::Success;
                });
        }
    }

    // 팝업 오픈 트리거 (비동기 작업이 끝났을 때 1프레임 동안 호출)
    if (s_ShowSuccessPopup)
    {
        ImGui::OpenPopup(LOCAL_LABEL("TextureInspector.ImportResult").c_str());
        s_ShowSuccessPopup = false;
    }

    // 결과 팝업 창
    if (ImGui::BeginPopupModal(LOCAL_LABEL("TextureInspector.ImportResult").c_str(), NULL,
        ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (!s_ImportSucceeded)
            ImGui::TextWrapped("%s", LOCAL_CSTR("TextureInspector.ImportFailed"));
        else if (s_ImportedSprite)
            ImGui::Text("%s", LOCAL_CSTR("TextureInspector.SpriteConverted"));
        else
            ImGui::Text("%s", LOCAL_CSTR("TextureInspector.TextureConverted"));

        ImGui::Separator();
        const EditorUI::DialogAction actions[] = { { "EditorCommon.OK" } };
        if (EditorUI::DialogButtonBar("ImportResultActions", actions, IM_ARRAYSIZE(actions)) == 0)
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

    return true;
}