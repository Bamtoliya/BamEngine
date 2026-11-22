#pragma once

#include "Resource.h"
#include "ResourceHandle.h"
#include "Texture.h"
#include "Types.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace Engine
{
	STRUCT()
	struct AtlasEntry
	{
		REFLECT_BODY()

		PROPERTY(EDITABLE)
		std::string name;

		PROPERTY(EDITABLE)
		Rect pixelRegion; // (left, top, width, height) in atlas pixels

		PROPERTY(EDITABLE)
		vec4 uvTransform = vec4(0.0f, 0.0f, 1.0f, 1.0f); // (u, v, width, height) in 0.0 ~ 1.0
	};

	struct AtlasInputItem
	{
		std::string name;
		int32 width = 0;
		int32 height = 0;
		const uint8* rgbaPixels = nullptr; // 선택 사항: CPU 픽셀 결합용
	};

	struct SpriteAtlasCreateDesc : public ResourceCreateDesc
	{
		int32 width = 2048;
		int32 height = 2048;
		int32 padding = 2;
		std::vector<AtlasInputItem> items;
	};

	CLASS()
	class ENGINE_API SpriteAtlas final : public Resource
	{
		REFLECT_CLASS()
		DECLARE_RESOURCE(SpriteAtlas)

	public:
		using DESC = SpriteAtlasCreateDesc;

	private:
		SpriteAtlas() : Resource(EResourceType::Texture) {}
		virtual ~SpriteAtlas() = default;
		EResult Initialize(void* arg = nullptr) override;

	public:
		static SpriteAtlas* Create(void* arg = nullptr);
		virtual void Free() override;

	public:
		// MaxRects 2D Bin Packing 알고리즘 실행
		static bool PackMaxRects(
			int32 atlasWidth, int32 atlasHeight, int32 padding,
			const std::vector<AtlasInputItem>& items,
			std::vector<AtlasEntry>& outEntries);

		// RGBA8 픽셀 결합 (CPU atlas baking)
		static bool BakeAtlasPixels(
			int32 atlasWidth, int32 atlasHeight, int32 padding,
			const std::vector<AtlasInputItem>& items,
			const std::vector<AtlasEntry>& entries,
			std::vector<uint8>& outRgbaBuffer);

		bool BakeAtlasPixels(
			const std::vector<AtlasInputItem>& items,
			std::vector<uint8>& outRgbaBuffer) const;

		// 텍스처 생성 훅 (헤더 기반 Texture 생성)
		Texture* CreateTextureFromBakedPixels(
			const std::vector<uint8>& rgbaPixels,
			const std::wstring& textureKey = L"");

		bool CreateAndAssignTexture(
			const std::vector<AtlasInputItem>& items,
			const std::wstring& textureKey = L"");

		bool TryGetEntry(const std::string& name, AtlasEntry& outEntry) const;
		bool HasEntry(const std::string& name) const;

		const std::vector<AtlasEntry>& GetEntries() const { return m_Entries; }
		const std::vector<uint8>& GetBakedPixels() const { return m_BakedPixels; }
		int32 GetAtlasWidth() const { return m_Width; }
		int32 GetAtlasHeight() const { return m_Height; }
		int32 GetPadding() const { return m_Padding; }

		ResourceHandle<Texture> GetTexture() const { return m_Texture; }
		void SetTexture(ResourceHandle<Texture> texture) { m_Texture = texture; IncreaseVersion(); }

	private:
		PROPERTY(EDITABLE)
		int32 m_Width = 2048;

		PROPERTY(EDITABLE)
		int32 m_Height = 2048;

		PROPERTY(EDITABLE)
		int32 m_Padding = 2;

		PROPERTY(EDITABLE)
		std::vector<AtlasEntry> m_Entries;

		PROPERTY(EDITABLE)
		ResourceHandle<Texture> m_Texture;

		std::vector<uint8> m_BakedPixels;
		std::unordered_map<std::string, size_t> m_EntryLookup;
	};
}
