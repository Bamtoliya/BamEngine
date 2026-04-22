#pragma once

#include "Engine_Includes.h"
#include <span>
#include <lz4.h>

namespace Engine
{
	enum class ELZ4CompressionMethod : uint8
	{
		None = 0,
		LZ4 = 1,
	};

	struct LZ4BlockHeader
	{
		uint32 UncompressedSize = 0;
		uint32 CompressedSize = 0;
		uint8 Method = static_cast<uint8>(ELZ4CompressionMethod::LZ4);
		uint8 Reserved0 = 0;
		uint8 Reserved1 = 0;
		uint8 Reserved2 = 0;
	};

	class ENGINE_API LZ4Compressor
	{
	public:
		// 기본 threshold (바이트)
		static constexpr uint32 DefaultRawThreshold = 256;

	public:
		// 순수 압축/해제 (헤더 없음)
		static bool Compress(std::span<const uint8> src, vector<uint8>& outCompressed);
		static bool Decompress(std::span<const uint8> compressed, uint32 uncompressedSize, vector<uint8>& outDecompressed);

		// 헤더 포함 패킹/언패킹
		// src.size() < rawThreshold 이면 무압축(Method=None)으로 저장
		static bool CompressWithHeader(
			std::span<const uint8> src,
			vector<uint8>& outPacked,
			uint32 rawThreshold = DefaultRawThreshold
		);

		static bool DecompressWithHeader(std::span<const uint8> packed, vector<uint8>& outDecompressed);

		// 유틸
		static uint32 GetMaxCompressedSize(uint32 srcSize);
	};
}