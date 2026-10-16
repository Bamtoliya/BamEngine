#pragma	once

#include "TextureInspector.h"
#include <DirectXTex.h>
#include "TextureImporter.h"
#include "TextureCompressorInterface.h"
#include "AssetManager.h"

static Engine::ETextureFormat s_TargetFormat = Engine::ETextureFormat::BC1_RGBA_UNORM;
static uint32 s_CompressFlagsValue = static_cast<uint32>(ECompressFlags::GenerateMipMaps | ECompressFlags::HighQuality);
static bool s_AutoGenerateSprite = true;

static bool s_IsImporting = false;       // 비동기 작업 진행 중 여부
static bool s_ShowSuccessPopup = false;  // 작업 완료 팝업 트리거

bool TextureInspector::IsSupported(const std::filesystem::path& assetPath)
{
	return assetPath.extension() == L".png" || assetPath.extension() == L".jpg" || assetPath.extension() == L".jpeg";
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

    // 2. DirectXTex를 이용한 실제 메타데이터 추출
    DirectX::TexMetadata meta;
    HRESULT hr = DirectX::GetMetadataFromWICFile(assetPath.wstring().c_str(), DirectX::WIC_FLAGS_NONE, meta);

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
    DrawFlag("TextureInspector.UseGPU", ECompressFlags::UseGPU);
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
    if (s_IsImporting)
    {
        // 작업 중일 때는 중복 클릭 방지를 위해 버튼 비활성화
        ImGui::BeginDisabled();
        ImGui::Button(LOCAL_LABEL("TextureInspector.Importing").c_str(), ImVec2(-1, 30));
        ImGui::EndDisabled();

        // AssetManager의 Update() 루프에서 작업 완료 시 m_ActiveTasks를 지우므로,
        // 카운트가 0이 되면 작업이 끝난 것으로 간주하고 팝업을 띄웁니다.
        if (AssetManager::Get().GetActiveTaskCount() == 0)
        {
            s_IsImporting = false;
            s_ShowSuccessPopup = true;
        }
    }
    else
    {
        if (ImGui::Button(LOCAL_LABEL("TextureInspector.Import").c_str(), ImVec2(-1, 30)))
        {
            s_IsImporting = true;

            // [핵심 변경점] 절대 경로를 프로젝트 루트 기준의 '상대 경로'로 변환합니다.
            // (std::filesystem::current_path()가 엔진의 작업 디렉토리 즉, 프로젝트 루트라고 가정)
            std::filesystem::path relSourcePath = std::filesystem::relative(assetPath, std::filesystem::current_path());
            std::filesystem::path relDestDir = relSourcePath;

            bool autoGenSprite = s_AutoGenerateSprite;

            tagTextureImportDesc desc = {};
            desc.CompressFlags = static_cast<ECompressFlags>(s_CompressFlagsValue);
            desc.TargetFormat = s_TargetFormat;

            // 이제 AssetManager와 Importer들은 모두 '상대 경로'만을 가지고 작업하게 됩니다.
            AssetManager::Get().ExecuteAsync([relSourcePath, relDestDir, autoGenSprite, desc]() -> EResult
                {
                    std::filesystem::path outputPath = relSourcePath;
                    outputPath.replace_extension(".bamtex");

                    // 1. 텍스처 임포트 (.bamtex 생성)
                    EResult result = AssetManager::Get().Import(relSourcePath, relDestDir, (void*)&desc);

                    // 2. 스프라이트 임포트 연쇄 호출 (.bamsprite 생성)
                    if (result == EResult::Success && autoGenSprite)
                    {
                        result = AssetManager::Get().Import(outputPath, relDestDir, nullptr);
                    }

                    return result;
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
        if (s_AutoGenerateSprite)
            ImGui::Text("%s", LOCAL_CSTR("TextureInspector.SpriteConverted"));
        else
            ImGui::Text("%s", LOCAL_CSTR("TextureInspector.TextureConverted"));

        ImGui::Separator();
        if (ImGui::Button(LOCAL_LABEL("EditorCommon.OK").c_str(), ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    return true;
}