#pragma once

#include "LZ4Compressor.h"

namespace Engine
{
	uint32 LZ4Compressor::GetMaxCompressedSize(uint32 srcSize)
	{
		return static_cast<uint32>(LZ4_compressBound(static_cast<int>(srcSize)));
	}

	bool LZ4Compressor::Compress(std::span<const uint8> src, vector<uint8>& outCompressed)
	{
		outCompressed.clear();

		if (src.empty())
		{
			return true;
		}

		const int srcSize = static_cast<int>(src.size());
		const int bound = LZ4_compressBound(srcSize);

		outCompressed.resize(static_cast<size_t>(bound));

		const int compressedSize = LZ4_compress_default(
			reinterpret_cast<const char*>(src.data()),
			reinterpret_cast<char*>(outCompressed.data()),
			srcSize,
			bound
		);

		if (compressedSize <= 0)
		{
			outCompressed.clear();
			return false;
		}

		outCompressed.resize(static_cast<size_t>(compressedSize));
		return true;
	}

	bool LZ4Compressor::Decompress(std::span<const uint8> compressed, uint32 uncompressedSize, vector<uint8>& outDecompressed)
	{
		outDecompressed.clear();

		if (uncompressedSize == 0)
		{
			return compressed.empty();
		}

		if (compressed.empty())
		{
			return false;
		}

		outDecompressed.resize(uncompressedSize);

		const int decodedSize = LZ4_decompress_safe(
			reinterpret_cast<const char*>(compressed.data()),
			reinterpret_cast<char*>(outDecompressed.data()),
			static_cast<int>(compressed.size()),
			static_cast<int>(uncompressedSize)
		);

		if (decodedSize < 0 || static_cast<uint32>(decodedSize) != uncompressedSize)
		{
			outDecompressed.clear();
			return false;
		}

		return true;
	}

	bool LZ4Compressor::CompressWithHeader(std::span<const uint8> src, vector<uint8>& outPacked, uint32 rawThreshold)
	{
		outPacked.clear();

		LZ4BlockHeader header{};
		header.UncompressedSize = static_cast<uint32>(src.size());

		// 작은 블록은 무압축 저장
		if (src.size() < rawThreshold)
		{
			header.Method = static_cast<uint8>(ELZ4CompressionMethod::None);
			header.CompressedSize = static_cast<uint32>(src.size());

			outPacked.resize(sizeof(LZ4BlockHeader) + src.size());
			memcpy(outPacked.data(), &header, sizeof(LZ4BlockHeader));

			if (!src.empty())
			{
				memcpy(outPacked.data() + sizeof(LZ4BlockHeader), src.data(), src.size());
			}
			return true;
		}

		// 그 외에는 LZ4 압축
		vector<uint8> compressed;
		if (!Compress(src, compressed))
		{
			return false;
		}

		header.Method = static_cast<uint8>(ELZ4CompressionMethod::LZ4);
		header.CompressedSize = static_cast<uint32>(compressed.size());

		outPacked.resize(sizeof(LZ4BlockHeader) + compressed.size());
		memcpy(outPacked.data(), &header, sizeof(LZ4BlockHeader));

		if (!compressed.empty())
		{
			memcpy(outPacked.data() + sizeof(LZ4BlockHeader), compressed.data(), compressed.size());
		}

		return true;
	}

	bool LZ4Compressor::DecompressWithHeader(std::span<const uint8> packed, vector<uint8>& outDecompressed)
	{
		outDecompressed.clear();

		if (packed.size() < sizeof(LZ4BlockHeader))
		{
			return false;
		}

		LZ4BlockHeader header{};
		memcpy(&header, packed.data(), sizeof(LZ4BlockHeader));

		const size_t payloadOffset = sizeof(LZ4BlockHeader);
		const size_t payloadSize = packed.size() - payloadOffset;

		if (payloadSize != header.CompressedSize)
		{
			return false;
		}

		std::span<const uint8> payload(packed.data() + payloadOffset, payloadSize);

		const ELZ4CompressionMethod method = static_cast<ELZ4CompressionMethod>(header.Method);
		if (method == ELZ4CompressionMethod::None)
		{
			// 무압축 블록
			if (header.UncompressedSize != header.CompressedSize)
			{
				return false;
			}

			outDecompressed.resize(payloadSize);
			if (!payload.empty())
			{
				memcpy(outDecompressed.data(), payload.data(), payload.size());
			}
			return true;
		}
		else if (method == ELZ4CompressionMethod::LZ4)
		{
			// 압축 블록
			return Decompress(payload, header.UncompressedSize, outDecompressed);
		}

		// 알 수 없는 메서드
		return false;
	}
}