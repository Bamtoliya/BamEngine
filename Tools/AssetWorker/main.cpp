#include <Windows.h>
#include <shellapi.h>
#include <fcntl.h>
#include <io.h>

#include "HeadlessThumbnailRasterizer.h"
#include "BuiltInPrimitiveMeshes.h"
#include "Engine_Includes.h"
#include "Functions.h"
#include "Logger.h"
#include "PlatformPath.h"
#include "ReflectionRegistry.h"
#include "ResourceManager.h"
#include "ModelConverter.h"
#include "TextureCompressorInterface.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <locale>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
    bool HandleImportModel(uint64_t requestId, const std::string& source, const std::string& snapshot,
        const std::string& optionFields)
    {
        try
        {
            Tools::ModelConversion::Options options;
            std::istringstream fields(optionFields);
            fields.imbue(std::locale::classic());
            if (!(fields >> options.FlipUVs >> options.ConvertToLeftHanded >> options.UniformScale
                >> options.FlipWindingOrder >> options.FlipPitch >> options.FlipHeading
                >> options.GenerateLODs >> options.LODCount >> options.TargetError))
                return false;

            options.LODReductionRates.clear();
            while (fields >> std::ws && fields.peek() != std::char_traits<char>::eof())
            {
                float rate = 0.0f;
                if (!(fields >> rate) || !std::isfinite(rate) || options.LODReductionRates.size() == 16)
                    return false;
                options.LODReductionRates.push_back(rate);
            }
            if (!std::isfinite(options.TargetError))
                return false;

            std::cout << "PROGRESS\t" << requestId << "\t1\n" << std::flush;
            const auto result = Tools::ModelConversion::ConvertModelToSnapshot(
                Engine::Utf8ToPath(source), Engine::Utf8ToPath(snapshot), options);
            if (!result)
            {
                std::cerr << result.Message << '\n';
                return false;
            }

            std::cout << "PROGRESS\t" << requestId << "\t2\n" << std::flush;
            return true;
        }
        catch (const std::exception& exception)
        {
            std::cerr << exception.what() << '\n';
            return false;
        }
    }

    bool HandleImportTexture(uint64_t requestId, const std::string& source, const std::string& finalBamtex,
        const std::string& finalDds, const std::string& stagedBamtex, const std::string& stagedDds,
        const std::string& formatField, const std::string& flagsField)
    {
        try
        {
            if (source.empty() || finalBamtex.empty() || finalDds.empty() || stagedBamtex.empty() || stagedDds.empty())
                return false;

            size_t parsed = 0;
            const auto formatValue = std::stoul(formatField, &parsed);
            if (parsed != formatField.size() || formatValue > UINT8_MAX)
                return false;

            const auto flagsValue = std::stoul(flagsField, &parsed);
            if (parsed != flagsField.size() || flagsValue > 0xffu)
                return false;

            const auto format = static_cast<Engine::ETextureFormat>(formatValue);
            switch (format)
            {
            case Engine::ETextureFormat::R8G8B8A8_UNORM:
            case Engine::ETextureFormat::BC1_RGBA_UNORM:
            case Engine::ETextureFormat::BC2_RGBA_UNORM:
            case Engine::ETextureFormat::BC3_RGBA_UNORM:
            case Engine::ETextureFormat::BC4_R_UNORM:
            case Engine::ETextureFormat::BC5_RG_UNORM:
            case Engine::ETextureFormat::BC7_RGBA_UNORM:
                break;
            default:
                return false;
            }

            const auto flags = static_cast<ECompressFlags>(flagsValue);
            const auto sourcePath = Engine::Utf8ToPath(source);
            const auto finalBamtexPath = Engine::Utf8ToPath(finalBamtex);
            const auto finalDdsPath = Engine::Utf8ToPath(finalDds);
            const auto stagedBamtexPath = Engine::Utf8ToPath(stagedBamtex);
            const auto stagedDdsPath = Engine::Utf8ToPath(stagedDds);

            std::unique_ptr<Tools::TextureCompressorInterface> compressor(Tools::TextureCompressorFactory::Create());
            if (!compressor)
                return false;

            std::cout << "PROGRESS\t" << requestId << "\t1\n" << std::flush;
            std::vector<uint8> compressedData;
            Engine::TextureBinaryHeader header{};
            TextureCompressionResult compressionResult{};
            if (!compressor->GetRawCompressedData(sourcePath.wstring(), compressedData, header, format, flags, &compressionResult))
                return false;

            std::cout << "BACKEND\t" << requestId << "\t" << static_cast<unsigned>(compressionResult.Backend)
                << "\t" << compressionResult.GpuError << "\n" << std::flush;
            std::cout << "PROGRESS\t" << requestId << "\t2\n" << std::flush;
            const auto destroyTexture = [](Engine::Texture* value)
            {
                if (value) Engine::ResourceManager::Get().DestroyResource(value);
            };
            std::unique_ptr<Engine::Texture, decltype(destroyTexture)> texture(
                Engine::Texture::Create(header, compressedData, finalBamtexPath.wstring()), destroyTexture);
            if (!texture)
                return false;

            const EResult saveResult = Engine::ResourceManager::Get().SaveToBinaryFile(texture.get(), stagedBamtexPath.wstring());
            if (saveResult != EResult::Success)
                return false;

            std::cout << "PROGRESS\t" << requestId << "\t3\n" << std::flush;
            if (!compressor->SaveCompressedDataToDDS(stagedDdsPath.wstring(), compressedData, header))
                return false;

            std::cout << "PROGRESS\t" << requestId << "\t4\n" << std::flush;
            return std::filesystem::exists(stagedBamtexPath) && std::filesystem::exists(stagedDdsPath)
                && !finalBamtexPath.empty() && !finalDdsPath.empty();
        }
        catch (const std::exception& exception)
        {
            std::cerr << exception.what() << '\n';
            return false;
        }
    }
}

namespace
{
    std::vector<std::wstring> ParseWideArgs()
    {
        std::vector<std::wstring> args;
        int argc = 0;
        LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argv) return args;

        args.reserve(static_cast<size_t>(argc));
        for (int i = 0; i < argc; ++i) args.emplace_back(argv[i]);
        LocalFree(argv);
        return args;
    }

    int RunDaemonLoop()
    {
        // 파이프 바이너리 모드 설정으로 CRLF 변환 및 인코딩 왜곡 방지
        (void)_setmode(_fileno(stdin), _O_BINARY);
        (void)_setmode(_fileno(stdout), _O_BINARY);

        std::string line;
        while (std::getline(std::cin, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty()) continue;
            if (line == "QUIT") break;

            // 프로토콜: THUMB\t<requestId>\t<resolution>\t<sourcePathUtf8>\t<outputPathUtf8>
            std::vector<std::string> fields;
            std::stringstream ss(line);
            std::string token;
            while (std::getline(ss, token, '\t')) fields.push_back(std::move(token));

            if (fields.size() >= 5 && fields[0] == "THUMB")
            {
                const uint64_t requestId = std::strtoull(fields[1].c_str(), nullptr, 10);
                int32_t resolution = std::atoi(fields[2].c_str());
                if (resolution <= 0) resolution = 128;

                const std::filesystem::path sourcePath = Engine::Utf8ToPath(fields[3]);
                const std::filesystem::path outputPath = Engine::Utf8ToPath(fields[4]);
                const bool ok = Tools::AssetWorker::HeadlessThumbnailRasterizer::BakeThumbnail(sourcePath, outputPath, resolution);

                std::cout << "RESULT\t" << requestId << "\t" << (ok ? 1 : 0) << "\n" << std::flush;
            }
            else if (fields.size() == 2 && fields[0] == "MODEL_WORKER_VERSION" && fields[1] == "1")
            {
                std::cout << "MODEL_WORKER_VERSION\t1\n" << std::flush;
            }
            else if (fields.size() == 5 && fields[0] == "IMPORT_MODEL")
            {
                const uint64_t requestId = std::strtoull(fields[1].c_str(), nullptr, 10);
                const bool ok = HandleImportModel(requestId, fields[2], fields[3], fields[4]);
                std::cout << "RESULT\t" << requestId << "\t" << (ok ? 1 : 0) << "\n" << std::flush;
            }
            else if (fields.size() == 2 && fields[0] == "TEXTURE_WORKER_VERSION" && fields[1] == "2")
            {
                std::cout << "TEXTURE_WORKER_VERSION\t2\n" << std::flush;
            }
            else if (fields.size() == 9 && fields[0] == "IMPORT_TEXTURE")
            {
                const uint64_t requestId = std::strtoull(fields[1].c_str(), nullptr, 10);
                const bool ok = HandleImportTexture(requestId, fields[2], fields[3], fields[4],
                    fields[5], fields[6], fields[7], fields[8]);
                std::cout << "RESULT\t" << requestId << "\t" << (ok ? 1 : 0) << "\n" << std::flush;
            }
        }
        return 0;
    }
}

int main()
{
    const auto args = ParseWideArgs();

    bool daemonMode = false;
    std::wstring mode;
    std::filesystem::path sourcePath;
    std::filesystem::path outputPath;
    std::filesystem::path projectRoot;
    int32_t resolution = 128;

    for (size_t i = 1; i < args.size(); ++i)
    {
        const auto& arg = args[i];
        if (arg == L"--daemon") daemonMode = true;
        else if (arg == L"--mode" && i + 1 < args.size()) mode = args[++i];
        else if (arg == L"--source" && i + 1 < args.size()) sourcePath = args[++i];
        else if (arg == L"--output" && i + 1 < args.size()) outputPath = args[++i];
        else if (arg == L"--project-root" && i + 1 < args.size()) projectRoot = args[++i];
        else if (arg == L"--resolution" && i + 1 < args.size()) resolution = (std::max)(16, _wtoi(args[++i].c_str()));
    }

    if (!projectRoot.empty()) Engine::PlatformPath::SetProjectRoot(projectRoot);
    Engine::PlatformPath::RegisterVirtualScheme({ L"Editor", Engine::PlatformPath::GetEngineRoot() / "Editor/Resources", true, true });
    if (daemonMode) Engine::Logger::SetMinLevel(Engine::ELogLevel::Error);

    struct EngineScope
    {
        bool Ready = false;
        EngineScope()
        {
            if (Engine::ReflectionRegistry::Create() && Engine::ResourceManager::Create())
            {
                Engine::RegisterBuiltInPrimitiveMeshes(Engine::ResourceManager::Get());
                Ready = true;
            }
        }
        ~EngineScope()
        {
            if (Engine::ResourceManager::IsCreated()) Engine::ResourceManager::Destroy();
            if (Engine::ReflectionRegistry::IsCreated()) Engine::ReflectionRegistry::Destroy();
        }
    } engineScope;

    if (!engineScope.Ready) return 1;
    if (daemonMode) return RunDaemonLoop();

    if (mode == L"thumbnail" && !sourcePath.empty() && !outputPath.empty())
    {
        const bool ok = Tools::AssetWorker::HeadlessThumbnailRasterizer::BakeThumbnail(sourcePath, outputPath, resolution);
        return ok ? 0 : 1;
    }

    return 1;
}
