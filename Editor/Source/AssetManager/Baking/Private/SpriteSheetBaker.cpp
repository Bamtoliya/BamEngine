#include "SpriteSheetBaker.h"
#include "ThumbnailRenderer.h"
#include "Renderer.h"
#include "RHI.h"
#include "RHITexture.h"
#include "ResourceBinaryHeader.h"
#include "PlatformPath.h"
#ifdef STB_IMAGE_WRITE_IMPLEMENTATION
#undef STB_IMAGE_WRITE_IMPLEMENTATION
#endif
#include <stb_image_write.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <system_error>
#include <thread>

namespace SpriteSheetBakerDetail
{
	void SetError(std::string* outError, const std::string& message)
	{
		if (outError)
			*outError = message;
	}

	std::string EscapeJson(const std::string& input)
	{
		std::string out;
		out.reserve(input.size() + 8);
		for (char ch : input)
		{
			switch (ch)
			{
			case '"':  out += "\\\""; break;
			case '\\': out += "\\\\"; break;
			case '\b': out += "\\b"; break;
			case '\f': out += "\\f"; break;
			case '\n': out += "\\n"; break;
			case '\r': out += "\\r"; break;
			case '\t': out += "\\t"; break;
			default:
				if (static_cast<unsigned char>(ch) < 0x20)
				{
					char buf[8];
					std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned int>(static_cast<unsigned char>(ch)));
					out += buf;
				}
				else
				{
					out += ch;
				}
				break;
			}
		}
		return out;
	}

	bool EnsureParentDirectory(const std::filesystem::path& targetPath, std::string* outError)
	{
		const auto parent = targetPath.parent_path();
		if (parent.empty())
			return true;

		std::error_code ec;
		if (std::filesystem::exists(parent, ec))
			return true;

		if (!std::filesystem::create_directories(parent, ec) && ec)
		{
			SetError(outError, "Failed to create output directory: " + parent.generic_string());
			return false;
		}
		return true;
	}

	bool WriteFileAtomic(const std::filesystem::path& targetPath, const std::string& content, std::string* outError)
	{
		if (!EnsureParentDirectory(targetPath, outError))
			return false;

		const auto threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
		std::filesystem::path tmpPath = targetPath;
		tmpPath += ".tmp_" + std::to_string(threadId);

		{
			std::ofstream file(tmpPath, std::ios::binary | std::ios::trunc);
			if (!file.is_open())
			{
				SetError(outError, "Failed to open temporary metadata file: " + tmpPath.generic_string());
				return false;
			}
			file.write(content.data(), static_cast<std::streamsize>(content.size()));
			if (!file.good())
			{
				SetError(outError, "Failed to write temporary metadata file: " + tmpPath.generic_string());
				file.close();
				std::error_code rmEc;
				std::filesystem::remove(tmpPath, rmEc);
				return false;
			}
		}

		std::error_code ec;
		std::filesystem::rename(tmpPath, targetPath, ec);
		if (ec)
		{
			std::error_code rmEc;
			std::filesystem::remove(tmpPath, rmEc);
			SetError(outError, "Failed to commit metadata file: " + targetPath.generic_string());
			return false;
		}
		return true;
	}

	int32 ResolveColumns(const Editor::SpriteSheetBakeConfig& config, int32 frameCount)
	{
		if (config.columns > 0)
			return config.columns;
		if (config.framesPerDirection > 0)
			return config.framesPerDirection;
		return std::max(1, frameCount);
	}

	void ExtrudeFramePadding(
		std::vector<uint8>& atlasPixels,
		int32 atlasWidth,
		int32 atlasHeight,
		int32 dstX,
		int32 dstY,
		int32 frameWidth,
		int32 frameHeight,
		int32 padding)
	{
		if (padding <= 0 || frameWidth <= 0 || frameHeight <= 0)
			return;

		const size_t rowBytes = static_cast<size_t>(frameWidth) * 4;

		for (int32 p = 1; p <= padding; ++p)
		{
			const int32 topY = dstY - p;
			if (topY >= 0)
			{
				const uint8* srcRow = atlasPixels.data() + (static_cast<size_t>(dstY) * atlasWidth + dstX) * 4;
				uint8* dstRow = atlasPixels.data() + (static_cast<size_t>(topY) * atlasWidth + dstX) * 4;
				std::memcpy(dstRow, srcRow, rowBytes);
			}

			const int32 bottomY = dstY + frameHeight - 1 + p;
			if (bottomY < atlasHeight)
			{
				const uint8* srcRow = atlasPixels.data() + (static_cast<size_t>(dstY + frameHeight - 1) * atlasWidth + dstX) * 4;
				uint8* dstRow = atlasPixels.data() + (static_cast<size_t>(bottomY) * atlasWidth + dstX) * 4;
				std::memcpy(dstRow, srcRow, rowBytes);
			}
		}

		const int32 minY = std::max(0, dstY - padding);
		const int32 maxY = std::min(atlasHeight - 1, dstY + frameHeight - 1 + padding);
		for (int32 y = minY; y <= maxY; ++y)
		{
			const uint8* leftSrc = atlasPixels.data() + (static_cast<size_t>(y) * atlasWidth + dstX) * 4;
			const uint8* rightSrc = atlasPixels.data() + (static_cast<size_t>(y) * atlasWidth + (dstX + frameWidth - 1)) * 4;

			for (int32 p = 1; p <= padding; ++p)
			{
				const int32 leftX = dstX - p;
				if (leftX >= 0)
				{
					uint8* leftDst = atlasPixels.data() + (static_cast<size_t>(y) * atlasWidth + leftX) * 4;
					std::memcpy(leftDst, leftSrc, 4);
				}

				const int32 rightX = dstX + frameWidth - 1 + p;
				if (rightX < atlasWidth)
				{
					uint8* rightDst = atlasPixels.data() + (static_cast<size_t>(y) * atlasWidth + rightX) * 4;
					std::memcpy(rightDst, rightSrc, 4);
				}
			}
		}
	}

	std::string BuildSpriteSheetMetaJson(
		int32 atlasWidth,
		int32 atlasHeight,
		int32 frameCount,
		const Editor::SpriteSheetBakeConfig& config)
	{
		const int32 cols = ResolveColumns(config, frameCount);
		const int32 padding = std::max(0, config.padding);
		const int32 cellW = config.frameWidth + padding * 2;
		const int32 cellH = config.frameHeight + padding * 2;
		const int32 directions = std::max(1, config.directions);
		const int32 framesPerDir = config.framesPerDirection > 0
			? config.framesPerDirection
			: std::max(1, (frameCount + directions - 1) / directions);
		const float ppu = (std::isfinite(config.pixelsPerUnit) && config.pixelsPerUnit > 0.0f)
			? config.pixelsPerUnit : DEFAULT_PPU;

		const std::filesystem::path resolvedMeta = Engine::PlatformPath::ResolveUri(config.outputSpriteMetaPath);
		const std::filesystem::path resolvedPng = Engine::PlatformPath::ResolveUri(config.outputPngPath);
		const std::string metaKey = Engine::PlatformPath::ToProjectRelative(resolvedMeta).generic_string();
		const std::string metaPathStr = resolvedMeta.generic_string();
		const std::string pngRel = Engine::PlatformPath::ToProjectRelative(resolvedPng).generic_string();
		const std::string pngKey = pngRel.rfind("Assets/", 0) == 0
			? ("Project://" + pngRel.substr(7))
			: pngRel;
		const std::string pngPathStr = resolvedPng.generic_string();

		const uint64 magicNumber = Engine::ENGINE_ASSET_MAGIC;
		const uint32 spriteResourceType = static_cast<uint32>(Engine::EResourceType::Sprite);

		std::ostringstream oss;
		oss << std::fixed << std::setprecision(6);
		oss << "{\n";
		oss << "    \"AssetHeader\": {\n";
		oss << "        \"MagicNumber\": " << magicNumber << ",\n";
		oss << "        \"ResourceType\": " << spriteResourceType << ",\n";
		oss << "        \"Version\": 0\n";
		oss << "    },\n";
		oss << "    \"m_Border\": [0, 0, 0, 0],\n";
		oss << "    \"m_Key\": \"" << EscapeJson(metaKey) << "\",\n";
		oss << "    \"m_Path\": \"" << EscapeJson(metaPathStr) << "\",\n";
		oss << "    \"m_Pivot\": [" << config.pivot.x << ", " << config.pivot.y << "],\n";
		oss << "    \"m_PixelPerUnit\": " << ppu << ",\n";
		oss << "    \"m_Region\": {\n";
		oss << "        \"height\": " << config.frameHeight << ",\n";
		oss << "        \"left\": " << padding << ",\n";
		oss << "        \"top\": " << padding << ",\n";
		oss << "        \"width\": " << config.frameWidth << "\n";
		oss << "    },\n";
		oss << "    \"m_ResourceType\": " << spriteResourceType << ",\n";
		oss << "    \"m_Texture\": {\n";
		oss << "        \"AssetKey\": \"" << EscapeJson(pngKey) << "\",\n";
		oss << "        \"AssetPath\": \"" << EscapeJson(pngPathStr) << "\",\n";
		oss << "        \"Null\": false\n";
		oss << "    },\n";
		oss << "    \"SpriteSheet\": {\n";
		oss << "        \"atlasWidth\": " << atlasWidth << ",\n";
		oss << "        \"atlasHeight\": " << atlasHeight << ",\n";
		oss << "        \"frameWidth\": " << config.frameWidth << ",\n";
		oss << "        \"frameHeight\": " << config.frameHeight << ",\n";
		oss << "        \"columns\": " << cols << ",\n";
		oss << "        \"padding\": " << padding << ",\n";
		oss << "        \"directions\": " << directions << ",\n";
		oss << "        \"framesPerDirection\": " << framesPerDir << ",\n";
		oss << "        \"totalFrames\": " << frameCount << ",\n";
		oss << "        \"captureFps\": " << config.captureFps << ",\n";
		oss << "        \"yawOffsetDegrees\": " << config.yawOffsetDegrees << ",\n";
		oss << "        \"frames\": [\n";

		for (int32 i = 0; i < frameCount; ++i)
		{
			const int32 col = i % cols;
			const int32 row = i / cols;
			const int32 rectX = col * cellW + padding;
			const int32 rectY = row * cellH + padding;
			const int32 dirIdx = i / framesPerDir;
			const int32 localFrame = i % framesPerDir;
			const float yawDeg = config.yawOffsetDegrees + (360.0f * static_cast<float>(dirIdx) / static_cast<float>(directions));

			const float u = static_cast<float>(rectX) / static_cast<float>(atlasWidth);
			const float v = static_cast<float>(rectY) / static_cast<float>(atlasHeight);
			const float uw = static_cast<float>(config.frameWidth) / static_cast<float>(atlasWidth);
			const float vh = static_cast<float>(config.frameHeight) / static_cast<float>(atlasHeight);

			oss << "            {\n";
			oss << "                \"index\": " << i << ",\n";
			oss << "                \"direction\": " << dirIdx << ",\n";
			oss << "                \"frame\": " << localFrame << ",\n";
			oss << "                \"yawDegrees\": " << yawDeg << ",\n";
			oss << "                \"rect\": [" << rectX << ", " << rectY << ", " << config.frameWidth << ", " << config.frameHeight << "],\n";
			oss << "                \"uv\": [" << u << ", " << v << ", " << uw << ", " << vh << "],\n";
			oss << "                \"pivot\": [" << config.pivot.x << ", " << config.pivot.y << "]\n";
			oss << "            }" << (i + 1 < frameCount ? "," : "") << "\n";
		}

		oss << "        ]\n";
		oss << "    }\n";
		oss << "}\n";
		return oss.str();
	}
}

BEGIN(Editor)

bool SpriteSheetBaker::AssembleAtlasRGBA(
	const std::vector<std::vector<uint8>>& frames,
	const SpriteSheetBakeConfig& config,
	std::vector<uint8>& outAtlasPixels,
	int32& outAtlasWidth,
	int32& outAtlasHeight,
	std::string* outError)
{
	outAtlasPixels.clear();
	outAtlasWidth = 0;
	outAtlasHeight = 0;

	if (frames.empty())
	{
		SpriteSheetBakerDetail::SetError(outError, "No frames provided for sprite sheet assembly.");
		return false;
	}
	if (config.frameWidth <= 0 || config.frameHeight <= 0)
	{
		SpriteSheetBakerDetail::SetError(outError, "Frame width and height must be positive.");
		return false;
	}
	if (config.padding < 0)
	{
		SpriteSheetBakerDetail::SetError(outError, "Padding must be non-negative.");
		return false;
	}

	const size_t expectedFrameBytes =
		static_cast<size_t>(config.frameWidth) * static_cast<size_t>(config.frameHeight) * 4;
	for (size_t i = 0; i < frames.size(); ++i)
	{
		if (frames[i].size() != expectedFrameBytes)
		{
			SpriteSheetBakerDetail::SetError(
				outError,
				"Frame " + std::to_string(i) + " byte size mismatch: expected " +
				std::to_string(expectedFrameBytes) + ", got " + std::to_string(frames[i].size()));
			return false;
		}
	}

	const int32 frameCount = static_cast<int32>(frames.size());
	const int32 cols = SpriteSheetBakerDetail::ResolveColumns(config, frameCount);
	const int32 rows = (frameCount + cols - 1) / cols;

	const int64 cellW = static_cast<int64>(config.frameWidth) + static_cast<int64>(config.padding) * 2;
	const int64 cellH = static_cast<int64>(config.frameHeight) + static_cast<int64>(config.padding) * 2;
	const int64 atlasW64 = static_cast<int64>(cols) * cellW;
	const int64 atlasH64 = static_cast<int64>(rows) * cellH;
	if (atlasW64 <= 0 || atlasH64 <= 0 ||
		atlasW64 > (std::numeric_limits<int32>::max)() ||
		atlasH64 > (std::numeric_limits<int32>::max)())
	{
		SpriteSheetBakerDetail::SetError(outError, "Computed atlas dimensions overflow 32-bit limits.");
		return false;
	}

	const int32 atlasW = static_cast<int32>(atlasW64);
	const int32 atlasH = static_cast<int32>(atlasH64);
	const size_t totalBytes = static_cast<size_t>(atlasW) * static_cast<size_t>(atlasH) * 4;
	outAtlasPixels.assign(totalBytes, 0);

	const size_t frameRowBytes = static_cast<size_t>(config.frameWidth) * 4;
	for (int32 i = 0; i < frameCount; ++i)
	{
		const int32 col = i % cols;
		const int32 row = i / cols;
		const int32 dstX = col * static_cast<int32>(cellW) + config.padding;
		const int32 dstY = row * static_cast<int32>(cellH) + config.padding;
		const std::vector<uint8>& frame = frames[static_cast<size_t>(i)];

		for (int32 y = 0; y < config.frameHeight; ++y)
		{
			const uint8* srcRow = frame.data() + static_cast<size_t>(y) * frameRowBytes;
			uint8* dstRow = outAtlasPixels.data() + (static_cast<size_t>(dstY + y) * atlasW + dstX) * 4;
			std::memcpy(dstRow, srcRow, frameRowBytes);
		}

		SpriteSheetBakerDetail::ExtrudeFramePadding(
			outAtlasPixels, atlasW, atlasH, dstX, dstY,
			config.frameWidth, config.frameHeight, config.padding);
	}

	outAtlasWidth = atlasW;
	outAtlasHeight = atlasH;
	return true;
}

bool SpriteSheetBaker::WriteSpriteSheetOutputs(
	const std::vector<uint8>& atlasPixels,
	int32 atlasWidth,
	int32 atlasHeight,
	int32 frameCount,
	const SpriteSheetBakeConfig& config,
	SpriteSheetBakeResult& outResult)
{
	outResult = {};
	outResult.outputPngPath = config.outputPngPath;
	outResult.outputSpriteMetaPath = config.outputSpriteMetaPath;

	if (atlasWidth <= 0 || atlasHeight <= 0 || frameCount <= 0)
	{
		outResult.errorMessage = "Invalid atlas dimensions or frame count.";
		return false;
	}

	const size_t requiredBytes = static_cast<size_t>(atlasWidth) * static_cast<size_t>(atlasHeight) * 4;
	if (atlasPixels.size() < requiredBytes)
	{
		outResult.errorMessage = "Atlas pixel buffer is smaller than expected RGBA8 size.";
		return false;
	}

	if (config.outputPngPath.empty() || config.outputSpriteMetaPath.empty())
	{
		outResult.errorMessage = "Output PNG path and sprite metadata path must not be empty.";
		return false;
	}

	const std::filesystem::path resolvedPng = Engine::PlatformPath::ResolveUri(config.outputPngPath);
	const std::filesystem::path resolvedMeta = Engine::PlatformPath::ResolveUri(config.outputSpriteMetaPath);
	if (Engine::PlatformPath::IsReadOnlyTarget(resolvedPng) ||
		Engine::PlatformPath::IsReadOnlyTarget(resolvedMeta))
	{
		outResult.errorMessage = "Target output path is read-only.";
		return false;
	}

	if (!SpriteSheetBakerDetail::EnsureParentDirectory(resolvedPng, &outResult.errorMessage))
		return false;

	const auto threadId = std::hash<std::thread::id>{}(std::this_thread::get_id());
	std::filesystem::path tmpPngPath = resolvedPng;
	tmpPngPath += ".tmp_" + std::to_string(threadId);
	const std::string tmpPngUtf8 = Engine::WStrToStr(tmpPngPath.wstring());

	const int stride = atlasWidth * 4;
	if (stbi_write_png(tmpPngUtf8.c_str(), atlasWidth, atlasHeight, 4, atlasPixels.data(), stride) == 0)
	{
		std::error_code rmEc;
		std::filesystem::remove(tmpPngPath, rmEc);
		outResult.errorMessage = "stbi_write_png failed for: " + resolvedPng.generic_string();
		return false;
	}

	std::error_code renameEc;
	std::filesystem::rename(tmpPngPath, resolvedPng, renameEc);
	if (renameEc)
	{
		std::error_code rmEc;
		std::filesystem::remove(tmpPngPath, rmEc);
		outResult.errorMessage = "Failed to finalize output PNG: " + resolvedPng.generic_string();
		return false;
	}

	const std::string metaJson = SpriteSheetBakerDetail::BuildSpriteSheetMetaJson(
		atlasWidth, atlasHeight, frameCount, config);
	if (!SpriteSheetBakerDetail::WriteFileAtomic(resolvedMeta, metaJson, &outResult.errorMessage))
		return false;

	outResult.succeeded = true;
	outResult.atlasWidth = atlasWidth;
	outResult.atlasHeight = atlasHeight;
	outResult.totalFrames = frameCount;
	outResult.outputPngPath = resolvedPng;
	outResult.outputSpriteMetaPath = resolvedMeta;
	return true;
}

SpriteSheetBakeResult SpriteSheetBaker::BakeTurntableFromFrames(
	const std::vector<std::vector<uint8>>& frames,
	const SpriteSheetBakeConfig& config)
{
	SpriteSheetBakeResult result;
	std::vector<uint8> atlasPixels;
	int32 atlasW = 0;
	int32 atlasH = 0;

	if (!AssembleAtlasRGBA(frames, config, atlasPixels, atlasW, atlasH, &result.errorMessage))
		return result;

	WriteSpriteSheetOutputs(atlasPixels, atlasW, atlasH, static_cast<int32>(frames.size()), config, result);
	return result;
}

SpriteSheetBakeResult SpriteSheetBaker::BakeTurntableFromCapture(
	const SpriteSheetBakeConfig& config,
	const TurntableCaptureCallback& captureFrameFn)
{
	SpriteSheetBakeResult result;
	if (!captureFrameFn)
	{
		result.errorMessage = "Turntable capture callback is null.";
		return result;
	}

	const int32 directions = std::max(1, config.directions);
	const int32 framesPerDir = std::max(1, config.framesPerDirection);
	const size_t totalFrames = static_cast<size_t>(directions) * static_cast<size_t>(framesPerDir);

	std::vector<std::vector<uint8>> capturedFrames;
	capturedFrames.reserve(totalFrames);

	for (int32 dir = 0; dir < directions; ++dir)
	{
		const float yaw = config.yawOffsetDegrees + (360.0f * static_cast<float>(dir) / static_cast<float>(directions));
		for (int32 f = 0; f < framesPerDir; ++f)
		{
			std::vector<uint8> framePixels;
			if (!captureFrameFn(dir, yaw, f, framesPerDir, framePixels))
			{
				result.errorMessage = "Frame capture failed at direction " + std::to_string(dir) +
					", frame " + std::to_string(f) + ".";
				return result;
			}
			capturedFrames.push_back(std::move(framePixels));
		}
	}

	return BakeTurntableFromFrames(capturedFrames, config);
}

bool SpriteSheetBaker::CaptureThumbnailFrameRGBA(
	Engine::Model* model,
	std::vector<uint8>& outRgbaPixels,
	int32& outWidth,
	int32& outHeight,
	std::string* outError,
	f32 yawDegrees)
{
	outRgbaPixels.clear();
	outWidth = 0;
	outHeight = 0;

	if (!model)
	{
		SpriteSheetBakerDetail::SetError(outError, "Model pointer is null.");
		return false;
	}

	if (!ThumbnailRenderer::IsCreated())
	{
		SpriteSheetBakerDetail::SetError(outError, "ThumbnailRenderer is not initialized.");
		return false;
	}

	auto* rhi = Engine::Renderer::IsCreated() ? Engine::Renderer::Get().GetRHI() : nullptr;
	if (!rhi)
	{
		SpriteSheetBakerDetail::SetError(outError, "Renderer RHI is not available.");
		return false;
	}

	auto& thumbRenderer = ThumbnailRenderer::Get();
	if (!thumbRenderer.SubmitModel(model, yawDegrees))
	{
		SpriteSheetBakerDetail::SetError(outError, "ThumbnailRenderer::SubmitModel failed.");
		return false;
	}

	Engine::RHITexture* snapshotTex = thumbRenderer.CreateThumbnailTexture();
	if (!snapshotTex)
	{
		SpriteSheetBakerDetail::SetError(outError, "ThumbnailRenderer::CreateThumbnailTexture returned null.");
		return false;
	}

	const uint32 texW = snapshotTex->GetWidth();
	const uint32 texH = snapshotTex->GetHeight();
	if (texW == 0 || texH == 0)
	{
		Engine::Safe_Release(snapshotTex);
		SpriteSheetBakerDetail::SetError(outError, "Captured thumbnail texture has zero dimensions.");
		return false;
	}

	std::vector<uint8> pixels(static_cast<size_t>(texW) * static_cast<size_t>(texH) * 4);
	const Engine::EResult readResult = rhi->ReadTexturePixels(
		snapshotTex, pixels.data(), static_cast<uint32>(pixels.size()));
	Engine::Safe_Release(snapshotTex);

	if (Engine::IsFailure(readResult))
	{
		SpriteSheetBakerDetail::SetError(outError, "RHI::ReadTexturePixels failed for captured thumbnail.");
		return false;
	}

	outRgbaPixels = std::move(pixels);
	outWidth = static_cast<int32>(texW);
	outHeight = static_cast<int32>(texH);
	return true;
}

END
