#pragma	once

#include "TextureInspector.h"
#include <DirectXTex.h>
#include "TextureImporter.h"
#include "TextureCompressorInterface.h"
#include "PropertyDrawer.h"
#include "AssetManager.h"

static Engine::ETextureFormat s_TargetFormat = Engine::ETextureFormat::BC7_RGBA_UNORM;
static uint32 s_CompressFlagsValue = static_cast<uint32>(ECompressFlags::GenerateMipMaps | ECompressFlags::HighQuality);

bool TextureInspector::IsSupported(const std::filesystem::path& assetPath)
{
	return assetPath.extension() == L".png" || assetPath.extension() == L".jpg" || assetPath.extension() == L".jpeg";
}

bool TextureInspector::OnInspectorGUI(const std::filesystem::path& assetPath)
{

    ImGui::TextColored(ImVec4(0.2f, 0.7f, 1.0f, 1.0f), "Texture Inspector");
    void* previewID = AssetCache::Get().GetThumbnail(assetPath);

    if (previewID)
    {
        ImGui::Image(previewID, ImVec2(128, 128));
    }
    ImGui::Separator();

    // 1. 파일 기본 정보
    ImGui::Text("File Name: %s", assetPath.filename().string().c_str());

    // 2. DirectXTex를 이용한 실제 메타데이터 추출
    DirectX::TexMetadata meta;
    HRESULT hr = DirectX::GetMetadataFromWICFile(assetPath.wstring().c_str(), DirectX::WIC_FLAGS_NONE, meta);

    if (SUCCEEDED(hr)) {
        ImGui::Text("Resolution: %llu x %llu", meta.width, meta.height);
        ImGui::Text("Mip Levels: %llu", meta.mipLevels);
        ImGui::Text("Dimension: %s", (meta.dimension == DirectX::TEX_DIMENSION_TEXTURE2D) ? "2D" : "Other");
    }
    else {
        ImGui::TextColored(ImVec4(1, 0, 0, 1), "Failed to load metadata!");
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Text("Import Settings");

    // 3. Target Format 선택 (Combo)
    const char* formatNames[] = { "BC1 (DXT1)", "BC2 (DXT3)", "BC3 (DXT5)", "BC4 (Grayscale)", "BC5 (Normal)", "BC7 (High Quality)" };
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

    if (ImGui::Combo("Target Format", &currentFormatIdx, formatNames, IM_ARRAYSIZE(formatNames)))
    {
        s_TargetFormat = formatValues[currentFormatIdx];
    }

    ImGui::Spacing();
    ImGui::Text("Compression Flags:");

    // 4. ECompressFlags 개별 체크박스 제어
    auto DrawFlag = [](const char* label, ECompressFlags flag) {
        bool isSet = (s_CompressFlagsValue & static_cast<uint32>(flag)) != 0;
        if (ImGui::Checkbox(label, &isSet)) {
            if (isSet) s_CompressFlagsValue |= static_cast<uint32>(flag);
            else s_CompressFlagsValue &= ~static_cast<uint32>(flag);
        }
        };

    DrawFlag("Generate MipMaps", ECompressFlags::GenerateMipMaps);
    DrawFlag("High Quality (Parallel)", ECompressFlags::HighQuality);
    DrawFlag("Use GPU Compression", ECompressFlags::UseGPU);
    DrawFlag("SRGB Color Space", ECompressFlags::SRGB);
    DrawFlag("Flip Y Axis", ECompressFlags::FilpY);
    DrawFlag("Premultiply Alpha", ECompressFlags::PremultiplyAlpha);
    DrawFlag("Normal Map Mode", ECompressFlags::NormalMap);

    ImGui::Spacing();

    // 5. 실행 버튼
    if (ImGui::Button("Import / Re-Compress", ImVec2(-1, 30)))
    {
        // 결과 파일 경로 설정 (예: Content 폴더 내 .bamtex)
        std::filesystem::path outputPath = assetPath;
        outputPath.replace_extension(".bamtex");

        // UI에서 설정된 값을 기반으로 변환 수행
        tagTextureImportDesc desc = {};
		desc.CompressFlags = static_cast<ECompressFlags>(s_CompressFlagsValue);
		desc.TargetFormat = s_TargetFormat;
        EResult result = AssetManager::Get().Import(assetPath, outputPath, &desc);

        // 결과 알림 팝업 (실제 구현 시 결과에 따라 메시지 변경 가능)
        ImGui::OpenPopup("Import Result");
    }

    // 결과 팝업 창
    if (ImGui::BeginPopupModal("Import Result", NULL, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Texture has been successfully converted to .bamtex!");
        ImGui::Separator();
        if (ImGui::Button("OK", ImVec2(120, 0))) { ImGui::CloseCurrentPopup(); }
        ImGui::EndPopup();
    }

    return true;
}