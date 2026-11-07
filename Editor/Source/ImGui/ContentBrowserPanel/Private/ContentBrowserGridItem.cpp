#pragma once
#include "ImGuiManager.h"
#include "ContentBrowserGridItem.h"
#include "SelectionManager.h"
#include "ModelImporter.h"
#include "Mesh.h"
#include <archive/JsonArchive.h>
#include <archive/BeveArchive.h>
#include "SceneManager.h"
#include "NotificationManager.h"
#include "Application.h"
#include "AssetManager.h"
#include "HotReloadManager.h"
#include "ShaderHotReloadManager.h"
#include "PIEManager.h"
#include "PrefabModeManager.h"
#include "ShortcutManager.h"
#include "ModelImportModal.h"
#include "ContentBrowserPanel.h"
#include "Editor_Macros.h"
#include "ContentBrowserRename.h"
#include "SpriteEditor.h"
#include "TilePalettePanel.h"
#include "InputManager.h"
#include <algorithm>

namespace
{
	struct AssetVisualDesc
	{
		const char* Icon = ICON_FA_FILE;
		ImVec4 BgColor = ImVec4(1.0f, 1.0f, 1.0f, 0.10f);
		ImVec4 AccentColor = ImVec4(0.65f, 0.65f, 0.65f, 1.0f);
		std::string BadgeText;
		std::string CleanName;
	};
	std::string ToLowerAscii(std::string_view str)
	{
		std::string out;
		out.reserve(str.size());
		for (char c : str) out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
		return out;
	}
	bool StripMatchedSuffix(std::string_view rawFilename, std::string_view lowerFilename, std::string_view lowerSuffix, std::string& outCleanName)
	{
		if (lowerFilename.size() > lowerSuffix.size() && lowerFilename.ends_with(lowerSuffix))
		{
			outCleanName.assign(rawFilename.substr(0, rawFilename.size() - lowerSuffix.size()));
			return true;
		}
		return false;
	}

	bool HasDirectorySegment(const std::filesystem::path& parentPath, std::string_view targetLower)
	{
		for (const auto& part : parentPath)
		{
			if (ToLowerAscii(part.string()) == targetLower) return true;
		}
		return false;
	}
	// 디스크 I/O 없이 파일명·복합 확장자·상위 폴더만으로 즉시 시각 정보를 결정합니다.
	AssetVisualDesc DescribeAssetVisual(const std::filesystem::path& path, bool isDirectory)
	{
		AssetVisualDesc desc;
		const std::string rawFilename = path.filename().string();
		if (isDirectory)
		{
			desc.Icon = ICON_FA_FOLDER;
			desc.BgColor = ImVec4(0.90f, 0.70f, 0.20f, 0.28f);
			desc.AccentColor = ImVec4(0.95f, 0.78f, 0.25f, 1.0f);
			desc.BadgeText = LOCAL("ContentBrowser.Folder");
			desc.CleanName = rawFilename;
			return desc;
		}
		const std::string lowerFilename = ToLowerAscii(rawFilename);
		const std::filesystem::path parentPath = path.parent_path();
		struct CompoundRule
		{
			std::string_view Suffix;
			const char* Icon;
			ImVec4 BgColor;
			ImVec4 AccentColor;
			const char* Badge;
		};
		// 긴 복합 확장자부터 우선 매칭합니다.
		static constexpr CompoundRule kRules[] = {
			// Scene
			{ ".bamscene.json",      ICON_FA_CLAPPERBOARD,        ImVec4(0.95f, 0.55f, 0.15f, 0.35f), ImVec4(1.00f, 0.65f, 0.25f, 1.0f), "SCENE" },
			{ ".scene.json",         ICON_FA_CLAPPERBOARD,        ImVec4(0.95f, 0.55f, 0.15f, 0.35f), ImVec4(1.00f, 0.65f, 0.25f, 1.0f), "SCENE" },
			{ ".bamscene",           ICON_FA_CLAPPERBOARD,        ImVec4(0.95f, 0.55f, 0.15f, 0.35f), ImVec4(1.00f, 0.65f, 0.25f, 1.0f), "SCENE" },
			// Prefab
			{ ".bamprefab.json",     ICON_FA_CUBES,               ImVec4(0.20f, 0.75f, 0.95f, 0.35f), ImVec4(0.35f, 0.85f, 1.00f, 1.0f), "PREFAB" },
			{ ".prefab.json",        ICON_FA_CUBES,               ImVec4(0.20f, 0.75f, 0.95f, 0.35f), ImVec4(0.35f, 0.85f, 1.00f, 1.0f), "PREFAB" },
			{ ".bamprefab",          ICON_FA_CUBES,               ImVec4(0.20f, 0.75f, 0.95f, 0.35f), ImVec4(0.35f, 0.85f, 1.00f, 1.0f), "PREFAB" },
			{ ".prefab",             ICON_FA_CUBES,               ImVec4(0.20f, 0.75f, 0.95f, 0.35f), ImVec4(0.35f, 0.85f, 1.00f, 1.0f), "PREFAB" },
			// Material Instance & Material
			{ ".bammatinst.json",    ICON_FA_CLONE,               ImVec4(0.90f, 0.45f, 0.30f, 0.38f), ImVec4(1.00f, 0.58f, 0.42f, 1.0f), "MAT INST" },
			{ ".bammatinst",         ICON_FA_CLONE,               ImVec4(0.90f, 0.45f, 0.30f, 0.38f), ImVec4(1.00f, 0.58f, 0.42f, 1.0f), "MAT INST" },
			{ ".materialinstance",   ICON_FA_CLONE,               ImVec4(0.90f, 0.45f, 0.30f, 0.38f), ImVec4(1.00f, 0.58f, 0.42f, 1.0f), "MAT INST" },
			{ ".bammat.json",        ICON_FA_CIRCLE_HALF_STROKE,  ImVec4(0.90f, 0.25f, 0.30f, 0.38f), ImVec4(1.00f, 0.40f, 0.45f, 1.0f), "MAT" },
			{ ".bammat",             ICON_FA_CIRCLE_HALF_STROKE,  ImVec4(0.90f, 0.25f, 0.30f, 0.38f), ImVec4(1.00f, 0.40f, 0.45f, 1.0f), "MAT" },
			{ ".mat",                ICON_FA_CIRCLE_HALF_STROKE,  ImVec4(0.90f, 0.25f, 0.30f, 0.38f), ImVec4(1.00f, 0.40f, 0.45f, 1.0f), "MAT" },
			// Mesh & Model
			{ ".bammesh.json",       ICON_FA_DRAW_POLYGON,        ImVec4(0.30f, 0.55f, 0.95f, 0.38f), ImVec4(0.45f, 0.70f, 1.00f, 1.0f), "MESH" },
			{ ".bammesh",            ICON_FA_DRAW_POLYGON,        ImVec4(0.30f, 0.55f, 0.95f, 0.38f), ImVec4(0.45f, 0.70f, 1.00f, 1.0f), "MESH" },
			{ ".bammodel.json",      ICON_FA_CUBE,                ImVec4(0.20f, 0.40f, 0.90f, 0.38f), ImVec4(0.40f, 0.60f, 1.00f, 1.0f), "MODEL" },
			{ ".bammodel",           ICON_FA_CUBE,                ImVec4(0.20f, 0.40f, 0.90f, 0.38f), ImVec4(0.40f, 0.60f, 1.00f, 1.0f), "MODEL" },
			// Sprite Animation & Sprite
			{ ".bamspriteanim.json", ICON_FA_FILM,                ImVec4(0.35f, 0.90f, 0.65f, 0.38f), ImVec4(0.45f, 1.00f, 0.75f, 1.0f), "SPRITE ANIM" },
			{ ".spriteanim.json",    ICON_FA_FILM,                ImVec4(0.35f, 0.90f, 0.65f, 0.38f), ImVec4(0.45f, 1.00f, 0.75f, 1.0f), "SPRITE ANIM" },
			{ ".bamspriteanim",      ICON_FA_FILM,                ImVec4(0.35f, 0.90f, 0.65f, 0.38f), ImVec4(0.45f, 1.00f, 0.75f, 1.0f), "SPRITE ANIM" },
			{ ".spriteanim",         ICON_FA_FILM,                ImVec4(0.35f, 0.90f, 0.65f, 0.38f), ImVec4(0.45f, 1.00f, 0.75f, 1.0f), "SPRITE ANIM" },
			{ ".bamsprite.json",     ICON_FA_IMAGES,              ImVec4(0.30f, 0.85f, 0.50f, 0.38f), ImVec4(0.45f, 0.95f, 0.65f, 1.0f), "SPRITE" },
			{ ".bamsprite",          ICON_FA_IMAGES,              ImVec4(0.30f, 0.85f, 0.50f, 0.38f), ImVec4(0.45f, 0.95f, 0.65f, 1.0f), "SPRITE" },
			// Texture
			{ ".bamtex.json",        ICON_FA_IMAGE,               ImVec4(0.20f, 0.80f, 0.35f, 0.38f), ImVec4(0.35f, 0.92f, 0.50f, 1.0f), "TEX" },
			{ ".bamtex",             ICON_FA_IMAGE,               ImVec4(0.20f, 0.80f, 0.35f, 0.38f), ImVec4(0.35f, 0.92f, 0.50f, 1.0f), "TEX" },
			// Animator / Animation / Skeleton
			{ ".bamanimator.json",   ICON_FA_DIAGRAM_PROJECT,     ImVec4(0.85f, 0.35f, 0.75f, 0.38f), ImVec4(0.95f, 0.50f, 0.85f, 1.0f), "ANIMATOR" },
			{ ".animator.json",      ICON_FA_DIAGRAM_PROJECT,     ImVec4(0.85f, 0.35f, 0.75f, 0.38f), ImVec4(0.95f, 0.50f, 0.85f, 1.0f), "ANIMATOR" },
			{ ".bamanimator",        ICON_FA_DIAGRAM_PROJECT,     ImVec4(0.85f, 0.35f, 0.75f, 0.38f), ImVec4(0.95f, 0.50f, 0.85f, 1.0f), "ANIMATOR" },
			{ ".animator",           ICON_FA_DIAGRAM_PROJECT,     ImVec4(0.85f, 0.35f, 0.75f, 0.38f), ImVec4(0.95f, 0.50f, 0.85f, 1.0f), "ANIMATOR" },
			{ ".bamanim.json",       ICON_FA_PERSON_RUNNING,      ImVec4(0.95f, 0.35f, 0.65f, 0.38f), ImVec4(1.00f, 0.50f, 0.78f, 1.0f), "ANIM" },
			{ ".anim.json",          ICON_FA_PERSON_RUNNING,      ImVec4(0.95f, 0.35f, 0.65f, 0.38f), ImVec4(1.00f, 0.50f, 0.78f, 1.0f), "ANIM" },
			{ ".bamanim",            ICON_FA_PERSON_RUNNING,      ImVec4(0.95f, 0.35f, 0.65f, 0.38f), ImVec4(1.00f, 0.50f, 0.78f, 1.0f), "ANIM" },
			{ ".anim",               ICON_FA_PERSON_RUNNING,      ImVec4(0.95f, 0.35f, 0.65f, 0.38f), ImVec4(1.00f, 0.50f, 0.78f, 1.0f), "ANIM" },
			{ ".bamskel.json",       ICON_FA_BONE,                ImVec4(0.80f, 0.75f, 0.60f, 0.38f), ImVec4(0.92f, 0.88f, 0.72f, 1.0f), "SKEL" },
			{ ".bamskel",            ICON_FA_BONE,                ImVec4(0.80f, 0.75f, 0.60f, 0.38f), ImVec4(0.92f, 0.88f, 0.72f, 1.0f), "SKEL" },
			// Script
			{ ".bamscript.json",     ICON_FA_SCROLL,              ImVec4(0.20f, 0.75f, 0.65f, 0.38f), ImVec4(0.35f, 0.95f, 0.82f, 1.0f), "SCRIPT" },
			{ ".script.json",        ICON_FA_SCROLL,              ImVec4(0.20f, 0.75f, 0.65f, 0.38f), ImVec4(0.35f, 0.95f, 0.82f, 1.0f), "SCRIPT" },
			{ ".bamscript",          ICON_FA_SCROLL,              ImVec4(0.20f, 0.75f, 0.65f, 0.38f), ImVec4(0.35f, 0.95f, 0.82f, 1.0f), "SCRIPT" },
			// Node Graph & PostProcess & Config
			{ ".bamgraph.json",      ICON_FA_CIRCLE_NODES,        ImVec4(0.45f, 0.40f, 0.90f, 0.38f), ImVec4(0.60f, 0.55f, 1.00f, 1.0f), "GRAPH" },
			{ ".graph.json",         ICON_FA_CIRCLE_NODES,        ImVec4(0.45f, 0.40f, 0.90f, 0.38f), ImVec4(0.60f, 0.55f, 1.00f, 1.0f), "GRAPH" },
			{ ".bamgraph",           ICON_FA_CIRCLE_NODES,        ImVec4(0.45f, 0.40f, 0.90f, 0.38f), ImVec4(0.60f, 0.55f, 1.00f, 1.0f), "GRAPH" },
			{ ".bampostfx.json",     ICON_FA_WAND_MAGIC_SPARKLES, ImVec4(0.60f, 0.40f, 0.85f, 0.38f), ImVec4(0.75f, 0.55f, 0.98f, 1.0f), "POST FX" },
			{ ".postfx.json",        ICON_FA_WAND_MAGIC_SPARKLES, ImVec4(0.60f, 0.40f, 0.85f, 0.38f), ImVec4(0.75f, 0.55f, 0.98f, 1.0f), "POST FX" },
			{ ".bamconfig.json",     ICON_FA_GEARS,               ImVec4(0.45f, 0.60f, 0.75f, 0.38f), ImVec4(0.65f, 0.80f, 0.95f, 1.0f), "CONFIG" },
			{ ".config.json",        ICON_FA_GEARS,               ImVec4(0.45f, 0.60f, 0.75f, 0.38f), ImVec4(0.65f, 0.80f, 0.95f, 1.0f), "CONFIG" },
			{ ".cfg.json",           ICON_FA_GEARS,               ImVec4(0.45f, 0.60f, 0.75f, 0.38f), ImVec4(0.65f, 0.80f, 0.95f, 1.0f), "CONFIG" },
			// Shader Binary & Metadata
			{ ".bamshader.json",     ICON_FA_CODE,                ImVec4(0.70f, 0.30f, 0.90f, 0.40f), ImVec4(0.82f, 0.48f, 1.00f, 1.0f), "SHADER" },
			{ ".bamshader",          ICON_FA_CODE,                ImVec4(0.70f, 0.30f, 0.90f, 0.40f), ImVec4(0.82f, 0.48f, 1.00f, 1.0f), "SHADER" },
		};
		for (const auto& rule : kRules)
		{
			if (StripMatchedSuffix(rawFilename, lowerFilename, rule.Suffix, desc.CleanName))
			{
				desc.Icon = rule.Icon;
				desc.BgColor = rule.BgColor;
				desc.AccentColor = rule.AccentColor;
				desc.BadgeText = rule.Badge;
				return desc;
			}
		}
		// 단일 확장자 분류
		std::string ext = ToLowerAscii(path.extension().string());
		desc.CleanName = path.stem().string();
		if (!ext.empty() && ext[0] == '.')
		{
			desc.BadgeText = ext.substr(1);
			for (char& c : desc.BadgeText) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
		}
		if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".dds" || ext == ".hdr")
		{
			desc.Icon = ICON_FA_IMAGE;
			desc.BgColor = ImVec4(0.20f, 0.80f, 0.35f, 0.38f);
			desc.AccentColor = ImVec4(0.35f, 0.92f, 0.50f, 1.0f);
		}
		else if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".blend")
		{
			desc.Icon = ICON_FA_CUBE;
			desc.BgColor = ImVec4(0.20f, 0.40f, 0.90f, 0.38f);
			desc.AccentColor = ImVec4(0.40f, 0.60f, 1.00f, 1.0f);
		}
		else if (ext == ".hlsl" || ext == ".glsl" || ext == ".vert" || ext == ".frag" || ext == ".comp" || ext == ".cso" || ext == ".spv")
		{
			desc.Icon = ICON_FA_CODE;
			desc.BgColor = ImVec4(0.70f, 0.30f, 0.90f, 0.40f);
			desc.AccentColor = ImVec4(0.82f, 0.48f, 1.00f, 1.0f);
		}
		else if (ext == ".hlsli")
		{
			desc.Icon = ICON_FA_FILE_CODE;
			desc.BgColor = ImVec4(0.25f, 0.60f, 0.85f, 0.40f);
			desc.AccentColor = ImVec4(0.40f, 0.75f, 0.98f, 1.0f);
		}
		else if (ext == ".lua" || ext == ".cs" || ext == ".py" || ext == ".cpp" || ext == ".hpp" || ext == ".c" || ext == ".h")
		{
			desc.Icon = ICON_FA_SCROLL;
			desc.BgColor = ImVec4(0.20f, 0.75f, 0.65f, 0.38f);
			desc.AccentColor = ImVec4(0.35f, 0.95f, 0.82f, 1.0f);
			desc.BadgeText = "SCRIPT";
		}
		else if (ext == ".ini" || ext == ".cfg" || ext == ".config" || ext == ".toml" || ext == ".yaml" || ext == ".yml" || ext == ".xml")
		{
			desc.Icon = ICON_FA_GEARS;
			desc.BgColor = ImVec4(0.45f, 0.60f, 0.75f, 0.38f);
			desc.AccentColor = ImVec4(0.65f, 0.80f, 0.95f, 1.0f);
			desc.BadgeText = "CONFIG";
		}
		else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac")
		{
			desc.Icon = ICON_FA_VOLUME_HIGH;
			desc.BgColor = ImVec4(0.95f, 0.75f, 0.20f, 0.38f);
			desc.AccentColor = ImVec4(1.00f, 0.85f, 0.35f, 1.0f);
		}
		else if (ext == ".json")
		{
			// 폴더 문맥 기반 분류 (디스크 I/O 없이 기존 단일 .json 에셋을 정확히 식별)
			if (HasDirectorySegment(parentPath, "scenes"))
			{
				desc.Icon = ICON_FA_CLAPPERBOARD;
				desc.BgColor = ImVec4(0.95f, 0.55f, 0.15f, 0.35f);
				desc.AccentColor = ImVec4(1.00f, 0.65f, 0.25f, 1.0f);
				desc.BadgeText = "SCENE";
			}
			else if (HasDirectorySegment(parentPath, "prefabs"))
			{
				desc.Icon = ICON_FA_CUBES;
				desc.BgColor = ImVec4(0.20f, 0.75f, 0.95f, 0.35f);
				desc.AccentColor = ImVec4(0.35f, 0.85f, 1.00f, 1.0f);
				desc.BadgeText = "PREFAB";
			}
			else if (HasDirectorySegment(parentPath, "scripts") || HasDirectorySegment(parentPath, "script"))
			{
				desc.Icon = ICON_FA_SCROLL;
				desc.BgColor = ImVec4(0.20f, 0.75f, 0.65f, 0.38f);
				desc.AccentColor = ImVec4(0.35f, 0.95f, 0.82f, 1.0f);
				desc.BadgeText = "SCRIPT";
			}
			else if (HasDirectorySegment(parentPath, "postprocess") || HasDirectorySegment(parentPath, "presets"))
			{
				desc.Icon = ICON_FA_WAND_MAGIC_SPARKLES;
				desc.BgColor = ImVec4(0.60f, 0.40f, 0.85f, 0.38f);
				desc.AccentColor = ImVec4(0.75f, 0.55f, 0.98f, 1.0f);
				desc.BadgeText = "POST FX";
			}
			else if (HasDirectorySegment(parentPath, "config") || HasDirectorySegment(parentPath, "startup"))
			{
				desc.Icon = ICON_FA_GEARS;
				desc.BgColor = ImVec4(0.45f, 0.60f, 0.75f, 0.38f);
				desc.AccentColor = ImVec4(0.65f, 0.80f, 0.95f, 1.0f);
				desc.BadgeText = "CONFIG";
			}
			else if (HasDirectorySegment(parentPath, "animation") || HasDirectorySegment(parentPath, "animations"))
			{
				desc.Icon = ICON_FA_PERSON_RUNNING;
				desc.BgColor = ImVec4(0.95f, 0.35f, 0.65f, 0.38f);
				desc.AccentColor = ImVec4(1.00f, 0.50f, 0.78f, 1.0f);
				desc.BadgeText = "ANIM";
			}
			else
			{
				desc.Icon = ICON_FA_FILE_LINES;
				desc.BgColor = ImVec4(0.55f, 0.55f, 0.55f, 0.35f);
				desc.AccentColor = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
			}
		}
		else if (ext == ".csv" || ext == ".beve")
		{
			desc.Icon = ICON_FA_FILE_LINES;
			desc.BgColor = ImVec4(0.55f, 0.55f, 0.55f, 0.35f);
			desc.AccentColor = ImVec4(0.75f, 0.75f, 0.75f, 1.0f);
		}
		return desc;
	}
	Engine::EResourceType PeekResourceType(const std::filesystem::path& path)
	{
		std::string ext = ToLowerAscii(path.extension().string());
		Engine::ResourceBinaryHeader header;
		archive::ArchiveResult res;
		if (ext == ".json")
		{
			archive::JsonArchive ar;
			if (!ar.LoadFromFile(path)) return Engine::EResourceType::Unknown;
			res = ar.ReadObject("AssetHeader", header, [](archive::IArchive& fields, Engine::ResourceBinaryHeader& h) {
				return fields.Read("ResourceType", h.ResourceType);
				});
		}
		else
		{
			archive::BeveArchive ar;
			if (!ar.LoadFromFile(path)) return Engine::EResourceType::Unknown;
			res = ar.ReadObject("AssetHeader", header, [](archive::IArchive& fields, Engine::ResourceBinaryHeader& h) {
				return fields.Read("ResourceType", h.ResourceType);
				});
		}
		return res ? header.ResourceType : Engine::EResourceType::Unknown;
	}
	bool IsSceneFile(const std::filesystem::path& path)
	{
		std::string ext = ToLowerAscii(path.extension().string());
		if (ext != ".json") return false;
		std::string stem = ToLowerAscii(path.stem().string());
		if (stem.ends_with(".scene") || stem.ends_with(".bamscene")) return true;
		// 다른 복합 에셋 확장자(.bammat.json 등)인 경우 불필요한 JSON 파싱 없이 즉시 제외
		if (stem.find(".bam") != std::string::npos || stem.ends_with(".prefab") ||
			stem.ends_with(".spriteanim") || stem.ends_with(".animator") ||
			stem.ends_with(".graph") || stem.ends_with(".postfx"))
		{
			return false;
		}
		// 일반 .json 파일인 경우 루트의 "Scene" 키 존재 여부 확인
		archive::JsonArchive ar;
		if (!ar.LoadFromFile(path)) return false;
		bool hasScene = false;
		return ar.HasField("Scene", hasScene) && hasScene;
	}
	bool IsPrefabFile(const std::filesystem::path& path)
	{
		std::string ext = ToLowerAscii(path.extension().string());
		if (ext == ".bamprefab" || ext == ".prefab") return true;
		if (ext != ".json") return false;
		std::string filename = ToLowerAscii(path.filename().string());
		if (filename.ends_with(".bamprefab.json") || filename.ends_with(".prefab.json")) return true;
		std::string stem = ToLowerAscii(path.stem().string());
		if (stem.find(".bam") != std::string::npos || stem.ends_with(".scene") ||
			stem.ends_with(".spriteanim") || stem.ends_with(".animator") ||
			stem.ends_with(".graph") || stem.ends_with(".postfx"))
		{
			return false;
		}
		archive::JsonArchive ar;
		if (!ar.LoadFromFile(path)) return false;
		bool hasPrefab = false;
		return ar.HasField("Prefab", hasPrefab) && hasPrefab;
	}
	bool InstantiatePrefabFromFile(const std::filesystem::path& path, Engine::Entity* parent = nullptr)
	{
		if (!Engine::SceneManager::IsCreated()) return false;
		auto* currentScene = Engine::SceneManager::Get().GetCurrentScene();
		if (!currentScene) return false;
		auto& history = Application::Get().GetEditHistory();
		auto before = history.Enabled() ? SceneEditSnapshot::Capture(*currentScene) : SceneEditSnapshot{};
		archive::JsonArchive ar;
		Engine::Entity* spawnedEntity = nullptr;
		const Engine::EResult result = Engine::SceneManager::Get().InstantiatePrefab(ar, path.wstring(), spawnedEntity, parent);
		if (result == Engine::EResult::Success && spawnedEntity)
		{
			if (before) history.Record(std::move(before), SceneEditSnapshot::Capture(*currentScene), "History.Command.InstantiatePrefab");
			SelectionManager::Get().SetSelectedEntity(spawnedEntity);
			NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL_FMT("Notification.PrefabInstantiated", path.stem().string()), ELogLevel::Info, 3.0f);
			return true;
		}
		else
		{
			NotificationManager::Get().AddNotification(LOCAL("ResourceType.Prefab"), LOCAL_FMT("Notification.PrefabInstantiateFailed", path.stem().string()), ELogLevel::Error, 3.0f);
			return false;
		}
	}
	bool OpenSceneFromFile(const std::filesystem::path& path)
	{
		return Editor::ImGuiManager::Get().RequestOpenScene(path);
	}
	void OpenAssetOrFolder(const std::filesystem::path& path, bool isDirectory, std::filesystem::path& currentDirectory, char* searchBuffer)
	{
		if (isDirectory)
		{
			currentDirectory /= path.filename();
			if (searchBuffer) searchBuffer[0] = '\0';
			return;
		}
		// 1. Scene JSON 파일인 경우
		if (IsSceneFile(path))
		{
			OpenSceneFromFile(path);
			return;
		}
		// 2. Prefab 파일인 경우
		if (IsPrefabFile(path))
		{
			PrefabModeManager::Get().EnterPrefabMode(path);
			return;
		}
		// 3. 일반 리소스 에디터 패널 오픈
		Editor::SelectionManager::Get().SetSelectedAsset(path);
		if (Editor::ImGuiManager::Get().RequestOpenResourceEditor(path))
			return;
		// 4. 지원하는 내부 에디터가 없는 일반 파일/소스 파일: OS 기본 연결 프로그램으로 열기
		const std::wstring absPath = std::filesystem::absolute(path).wstring();
		HINSTANCE result = ShellExecuteW(nullptr, L"open", absPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		if (reinterpret_cast<INT_PTR>(result) <= 32)
		{
			NotificationManager::Get().AddNotification(LOCAL("EditorCommon.Open"),
				LOCAL_FMT("Notification.OpenExternalFailed", path.filename().string()), ELogLevel::Warn, 3.0f);
		}
	}
}
float ContentBrowserGridItem::GetNameBoxHeight()
{
	return std::max(ImGui::GetTextLineHeight() * 2.0f + 12.0f, ImGui::GetFrameHeight());
}

bool ContentBrowserGridItem::Draw(const std::filesystem::directory_entry& directoryEntry,
	const std::filesystem::path& rootPath, ImTextureID thumbnailTexID, f32 thumbnailSize,
	ImGuiSelectionUserData itemId, bool selected, std::filesystem::path& outRenamingPath,
	char* renameBuffer, size_t renameBufferSize, std::filesystem::path& currentDirectory, char* searchBuffer)
{
	const auto& path = directoryEntry.path();
	const bool renaming = (outRenamingPath == path);
	const float nameHeight = GetNameBoxHeight();
	const ImVec2 tileMin = ImGui::GetCursorScreenPos();
	const ImVec2 nameMin(tileMin.x, tileMin.y + thumbnailSize);
	const ImVec2 tileMax(tileMin.x + thumbnailSize, nameMin.y + nameHeight);

	ImGui::PushID(path.string().c_str());
	// 타일 사이에 박스 선택을 시작할 여백을 남긴다.
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
	ImGui::BeginDisabled(renaming);
	ImGui::SetNextItemSelectionUserData(itemId);
	ImGui::Selectable("##AssetTile", selected, ImGuiSelectableFlags_None,
		ImVec2(thumbnailSize, thumbnailSize + nameHeight));
	ImGui::EndDisabled();
	ImGui::PopStyleVar();

	const bool active = ImGui::IsItemActive();
	const bool hovered = ImGui::IsItemHovered();
	const bool visible = ImGui::IsItemVisible();
	if (ImGui::IsItemToggledSelection())
		selected = !selected;

	bool contextMenuOpen = false;
	bool dragging = false;
	if (!renaming)
	{
		auto& input = InputManager::Get();
		const bool open = hovered && input.IsMouseButtonDoubleClicked(EMouseButton::Left) &&
			!input.IsCtrlPressed() && !input.IsShiftPressed();

		ToolTip(directoryEntry);
		ContextMenu(path, outRenamingPath, renameBuffer, renameBufferSize, currentDirectory, searchBuffer);
		contextMenuOpen = ImGui::IsPopupOpen("##AssetContextMenu");
		const auto payloadPath = rootPath.filename() / path.lexically_relative(rootPath);
		dragging = DragAndDropTarget(payloadPath);
		if (open && !dragging)
			OpenAssetOrFolder(path, directoryEntry.is_directory(), currentDirectory, searchBuffer);
	}

	if (visible)
		DrawThumbnail(directoryEntry, thumbnailTexID, thumbnailSize, tileMin);

	if (outRenamingPath == path)
	{
		ImGui::SetCursorScreenPos(nameMin);
		DrawRename(path, thumbnailSize, outRenamingPath, renameBuffer, renameBufferSize, currentDirectory);
	}
	else if (visible)
	{
		DrawNameBox(directoryEntry, nameMin, thumbnailSize, nameHeight);
	}

	if (visible && selected)
	{
		ImGui::GetWindowDrawList()->AddRect(tileMin, tileMax, ImGui::GetColorU32(ImGuiCol_CheckMark),
			4.0f, ImDrawFlags_None, 2.0f);
	}

	ImGui::PopID();
	return active || contextMenuOpen || dragging;
}

void ContentBrowserGridItem::DrawThumbnail(const std::filesystem::directory_entry& directoryEntry,
	ImTextureID thumbnailTexID, float thumbnailSize, const ImVec2& cursorPos)
{
	const AssetVisualDesc visual = DescribeAssetVisual(directoryEntry.path(), directoryEntry.is_directory());
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	if (!thumbnailTexID)
	{
		// 썸네일이 없을 때: 상단 모서리가 둥근 타입별 틴트 배경 + 밝은 타입 포인트 컬러 아이콘 렌더링
		drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + thumbnailSize, cursorPos.y + thumbnailSize),
			ImGui::GetColorU32(visual.BgColor), 4.0f, ImDrawFlags_RoundCornersTop);
		ImFont* font = ImGui::GetFont();
		float iconFontSize = thumbnailSize * 0.52f;
		ImVec2 iconSize = font->CalcTextSizeA(iconFontSize, FLT_MAX, 0.0f, visual.Icon);
		ImVec2 iconPos = ImVec2(cursorPos.x + (thumbnailSize - iconSize.x) * 0.5f, cursorPos.y + (thumbnailSize - iconSize.y) * 0.5f);
		drawList->AddText(font, iconFontSize, iconPos, ImGui::GetColorU32(visual.AccentColor), visual.Icon);
	}
	else
	{
		// 썸네일 이미지가 있는 경우에도 배경 틴트를 은은하게 깔아 투명 텍스처/스프라이트 가독성을 높입니다.
		drawList->AddRectFilled(cursorPos, ImVec2(cursorPos.x + thumbnailSize, cursorPos.y + thumbnailSize),
			ImGui::GetColorU32(ImVec4(visual.BgColor.x, visual.BgColor.y, visual.BgColor.z, 0.18f)), 4.0f, ImDrawFlags_RoundCornersTop);

		ImVec2 uv0(0.0f, 0.0f);
		ImVec2 uv1(1.0f, 1.0f);
		ImVec2 imgMin = cursorPos;
		ImVec2 imgMax(cursorPos.x + thumbnailSize, cursorPos.y + thumbnailSize);

		const auto& path = directoryEntry.path();
		const std::string lowerFilename = ToLowerAscii(path.filename().string());
		if (lowerFilename.ends_with(".bamsprite") || lowerFilename.ends_with(".bamsprite.json"))
		{
			auto& rm = Engine::ResourceManager::Get();
			Engine::Handle handle = rm.FindHandleByFilePath(path);
			if (!handle.IsValid())
			{
				const std::wstring relPath = Engine::PlatformPath::ToProjectRelative(path).wstring();
				handle = rm.FindHandleByKey(relPath);
				if (!handle.IsValid() && lowerFilename.ends_with(".bamsprite.json"))
				{
					handle = rm.FindHandleByKey(relPath.substr(0, relPath.size() - 5));
				}
			}
			if (auto* sprite = dynamic_cast<Engine::Sprite*>(rm.GetResource(handle)))
			{
				const vec4 uv = sprite->GetUVTransform();
				if (std::isfinite(uv.x) && std::isfinite(uv.y) && std::isfinite(uv.z) && std::isfinite(uv.w) && uv.z > 0.0f && uv.w > 0.0f)
				{
					uv0 = ImVec2(uv.x, uv.y);
					uv1 = ImVec2(uv.x + uv.z, uv.y + uv.w);

					if (auto* tex = sprite->GetTexture())
					{
						if (auto* rhiTex = tex->GetRHITexture())
						{
							const float cropW = static_cast<float>(rhiTex->GetWidth()) * uv.z;
							const float cropH = static_cast<float>(rhiTex->GetHeight()) * uv.w;
							if (cropW > 0.0f && cropH > 0.0f)
							{
								const float scale = thumbnailSize / (std::max)(cropW, cropH);
								const ImVec2 fitSize(cropW * scale, cropH * scale);
								imgMin = ImVec2(cursorPos.x + (thumbnailSize - fitSize.x) * 0.5f,
									cursorPos.y + (thumbnailSize - fitSize.y) * 0.5f);
								imgMax = ImVec2(imgMin.x + fitSize.x, imgMin.y + fitSize.y);
							}
						}
					}
				}
			}
		}

		drawList->AddImage(thumbnailTexID, imgMin, imgMax, uv0, uv1);
	}
	// 썸네일 하단 2px 타입 액센트 스트립 (썸네일 유무와 무관하게 리소스 종류를 한눈에 식별)
	drawList->AddRectFilled(
		ImVec2(cursorPos.x, cursorPos.y + thumbnailSize - 2.0f),
		ImVec2(cursorPos.x + thumbnailSize, cursorPos.y + thumbnailSize),
		ImGui::GetColorU32(visual.AccentColor));
}
void ContentBrowserGridItem::ContextMenu(const std::filesystem::path& path,
	std::filesystem::path& outRenamingPath, char* renameBuffer, size_t renameBufferSize,
	std::filesystem::path& currentDirectory, char* searchBuffer)
{
	auto* browser = dynamic_cast<ContentBrowserPanel*>(ImGuiManager::Get().GetImGuiPanel(L"Content Browser"));
	if (browser && ImGui::IsItemHovered())
		browser->SetRenameTarget(path);
	if (ImGui::BeginPopupContextItem("##AssetContextMenu"))
	{
		const bool isDir = std::filesystem::is_directory(path);
		if (browser)
			browser->SetRenameTarget(path);
		// 1. 최상단 [열기]: 더블클릭과 동일한 공통 동작
		if (ImGui::MenuItem((ICON_FA_FOLDER_OPEN " " + LOCAL_LABEL("EditorCommon.Open")).c_str()))
			OpenAssetOrFolder(path, isDir, currentDirectory, searchBuffer);
		// 2. 파일인 경우: [외부 프로그램으로 열기]
		if (!isDir)
		{
			if (ImGui::MenuItem((ICON_FA_ARROW_UP_RIGHT_FROM_SQUARE " " + LOCAL_LABEL("EditorCommon.OpenWithDefaultProgram")).c_str()))
			{
				const std::wstring absPath = std::filesystem::absolute(path).wstring();
				ShellExecuteW(nullptr, L"open", absPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
			}
		}
		// 3. [탐색기에서 표시]: 유니코드 및 공백 경로 대응
		if (ImGui::MenuItem((ICON_FA_FOLDER_CLOSED " " + LOCAL_LABEL("EditorCommon.ShowInExplorer")).c_str()))
		{
			const std::wstring absPath = std::filesystem::absolute(path).wstring();
			if (isDir)
			{
				ShellExecuteW(nullptr, L"open", absPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
			}
			else
			{
				const std::wstring param = L"/select,\"" + absPath + L"\"";
				ShellExecuteW(nullptr, L"open", L"explorer.exe", param.c_str(), nullptr, SW_SHOWNORMAL);
			}
		}
		ImGui::Separator();
		// 4. [이름 변경], [경로 복사]
		ACTION_MENU_ITEM((ICON_FA_PEN " " + LOCAL_LABEL("EditorCommon.Rename")).c_str(), "Edit.Rename");

		if (ImGui::MenuItem((ICON_FA_COPY " " + LOCAL_LABEL("EditorCommon.CopyPath")).c_str()))
			ImGui::SetClipboardText(path.string().c_str());
		if (!isDir && IsPrefabFile(path))
		{
			ImGui::Separator();
			if (ImGui::MenuItem((ICON_FA_PEN " " + LOCAL_LABEL("ContentBrowser.EditPrefab")).c_str()))
			{
				PrefabModeManager::Get().EnterPrefabMode(path);
			}
			if (ImGui::MenuItem((ICON_FA_CUBES " " + LOCAL_LABEL("ContentBrowser.InstantiatePrefab")).c_str()))
			{
				InstantiatePrefabFromFile(path);
			}
		}
		if (!isDir)
		{
			std::string filename = ToLowerAscii(path.filename().string());
			Engine::EResourceType peekedType = Engine::EResourceType::Unknown;
			if (filename.ends_with(".json") || filename.ends_with(".beve"))
				peekedType = PeekResourceType(path);
			if (filename.ends_with(".bammat") || filename.ends_with(".bammat.json") ||
				filename.ends_with(".bammatinst") || filename.ends_with(".bammatinst.json") ||
				peekedType == Engine::EResourceType::Material ||
				peekedType == Engine::EResourceType::MaterialInstance)
			{
				ContextMenu_Material(path);
			}
			else if (filename.ends_with(".fbx") || filename.ends_with(".obj") ||
				filename.ends_with(".gltf") || filename.ends_with(".glb") ||
				filename.ends_with(".bammesh") || filename.ends_with(".bammesh.json") ||
				filename.ends_with(".bammodel") || filename.ends_with(".bammodel.json") ||
				peekedType == Engine::EResourceType::Mesh || peekedType == Engine::EResourceType::Model)
			{
				ContextMenu_Model(path);
			}
			else if (filename.ends_with(".png") || filename.ends_with(".jpg") ||
				filename.ends_with(".tga") || filename.ends_with(".jpeg") || filename.ends_with(".bmp") ||
				filename.ends_with(".dds") || filename.ends_with(".bamtex") || filename.ends_with(".bamtex.json") ||
				peekedType == Engine::EResourceType::Texture)
			{
				ContextMenu_Texture(path);
			}
			else if (filename.ends_with(".bamsprite") || filename.ends_with(".bamsprite.json") ||
				peekedType == Engine::EResourceType::Sprite)
			{
				ContextMenu_Sprite(path);
			}
			else if (filename.ends_with(".hlsl") || filename.ends_with(".vert") ||
				filename.ends_with(".frag") || filename.ends_with(".comp") || filename.ends_with(".hlsli"))
			{
				ContextMenu_Shader(path);
			}
		}
		if (!isDir && IsSceneFile(path))
		{
			ImGui::Separator();
			if (ImGui::MenuItem((ICON_FA_FOLDER_OPEN " " + LOCAL_LABEL("Action.File.LoadScene")).c_str()))
			{
				OpenSceneFromFile(path);
			}
		}
		// JSON <-> Binary 변환 메뉴 (Scene / Prefab / Graph 등 비-Resource 직렬화 포맷은 제외)
		if (!isDir)
		{
			std::string filename = ToLowerAscii(path.filename().string());
			const bool isConvertibleJson = filename.ends_with(".bammat.json") || filename.ends_with(".bammatinst.json") ||
				filename.ends_with(".bammesh.json") || filename.ends_with(".bammodel.json") ||
				filename.ends_with(".bamsprite.json") || filename.ends_with(".bamanim.json") ||
				filename.ends_with(".bamskel.json") || filename.ends_with(".bamtex.json");
			if (isConvertibleJson || (filename.ends_with(".json") && PeekResourceType(path) != Engine::EResourceType::Unknown))
			{
				ImGui::Separator();
				if (ImGui::MenuItem((ICON_FA_FILE_EXPORT " " + LOCAL_LABEL("ContentBrowser.BakeToBinary")).c_str()))
				{
					ConvertJsonToBinary(path);
				}
			}
			else if ((filename.find(".bam") != std::string::npos && !filename.ends_with(".json") && !filename.ends_with(".bamprefab")) ||
				filename.ends_with(".beve"))
			{
				ImGui::Separator();
				if (ImGui::MenuItem((ICON_FA_FILE_CODE " " + LOCAL_LABEL("ContentBrowser.ExportToJson")).c_str()))
				{
					ConvertBinaryToJson(path);
				}
			}
		}
		ImGui::Separator();
		if (ImGui::MenuItem((ICON_FA_TRASH " " + LOCAL_LABEL("EditorCommon.Delete")).c_str()))
		{
			std::error_code ec;
			if (isDir)
				std::filesystem::remove_all(path, ec);
			else
				std::filesystem::remove(path, ec);
			if (ec) EDITOR_LOG_ERROR("Failed to delete asset {}: {}", path.string(), ec.message());
			else EDITOR_LOG_INFO("Deleted asset: {}", path.string());
		}
		ImGui::EndPopup();
	}
}
void ContentBrowserGridItem::ToolTip(const std::filesystem::directory_entry& directoryEntry)
{
	if (ImGui::IsItemHovered())
	{
		std::filesystem::path path = directoryEntry.path();
		std::string filenameString = path.filename().string();
		const AssetVisualDesc visual = DescribeAssetVisual(path, directoryEntry.is_directory());
		ImGui::BeginTooltip();
		ImGui::TextUnformatted(filenameString.c_str());
		ImGui::Separator();
		if (directoryEntry.is_directory())
		{
			ImGui::Text("%s", LOCAL_CSTR("ContentBrowser.FolderType"));
		}
		else
		{
			ImGui::TextColored(visual.AccentColor, "%s %s", visual.Icon, visual.BadgeText.c_str());
			try
			{
				uintmax_t fileSize = std::filesystem::file_size(path);
				if (fileSize < 1024)
					ImGui::Text("%s", LOCAL_FMT("ContentBrowser.SizeBytes", fileSize).c_str());
				else if (fileSize < 1024 * 1024)
					ImGui::Text("%s", LOCAL_FMT("ContentBrowser.SizeKB", fileSize / 1024).c_str());
				else
					ImGui::Text("%s", LOCAL_FMT("ContentBrowser.SizeMB", (float)fileSize / (1024.0f * 1024.0f)).c_str());
			}
			catch (...) {}
		}
		ImGui::EndTooltip();
	}
}
bool ContentBrowserGridItem::DragAndDropTarget(const std::filesystem::path& relativePath)
{
	if (!ImGui::BeginDragDropSource())
		return false;
	std::wstring wPath = Engine::NormalizePath(relativePath.wstring());
	ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", wPath.c_str(), (wPath.length() + 1) * sizeof(wchar_t));
	ImGui::Text("%s", relativePath.filename().string().c_str());
	ImGui::EndDragDropSource();
	return true;
}
void ContentBrowserGridItem::DrawRename(const std::filesystem::path& path, f32 thumbnailSize, std::filesystem::path& outRenamingPath, char* renameBuffer, size_t renameBufferSize, std::filesystem::path& currentDirectory)
{
	float width = (currentDirectory == path.parent_path()) ? thumbnailSize : ImGui::GetContentRegionAvail().x * 0.8f;
	ImGui::SetNextItemWidth(width);
	Editor::ContentBrowser::DrawRenameInput("##RenameInput", path, outRenamingPath, renameBuffer, renameBufferSize, currentDirectory);
}
void ContentBrowserGridItem::DrawNameBox(const std::filesystem::directory_entry& directoryEntry,
	const ImVec2& boxMin, float boxWidth, float boxHeight)
{
	const auto& path = directoryEntry.path();
	const AssetVisualDesc visual = DescribeAssetVisual(path, directoryEntry.is_directory());
	const std::string& filenameString = visual.CleanName;
	const std::string& typeStr = visual.BadgeText;
	ImVec2 typeTextSize = ImGui::CalcTextSize(typeStr.c_str());
	ImVec2 boxMax = ImVec2(boxMin.x + boxWidth, boxMin.y + boxHeight);
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	// 1. 네임 박스 배경
	ImU32 boxColor = ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.15f, 0.85f));
	drawList->AddRectFilled(boxMin, boxMax, boxColor, 4.0f, ImDrawFlags_RoundCornersBottom);
	// 2. 텍스트 축약(Ellipsis) 로직 (UTF-8 한글 깨짐 방지 처리 포함)
	float maxTextWidth = boxWidth - 8.0f;
	std::string displayFileName = filenameString;
	if (ImGui::CalcTextSize(displayFileName.c_str()).x > maxTextWidth)
	{
		while (!displayFileName.empty() && ImGui::CalcTextSize((displayFileName + "...").c_str()).x > maxTextWidth)
		{
			while (!displayFileName.empty())
			{
				char c = displayFileName.back();
				displayFileName.pop_back();
				if ((c & 0xC0) != 0x80) break;
			}
		}
		displayFileName += "...";
	}
	// 3. 깔끔하게 정제된 파일 이름 렌더링 (.bam* 중간 확장자 제거됨)
	drawList->AddText(ImVec2(boxMin.x + 4.0f, boxMin.y + 4.0f), ImGui::GetColorU32(ImGuiCol_Text), displayFileName.c_str());
	// 4. 우측 하단 타입 배지 렌더링 (리소스 고유 AccentColor 적용)
	ImVec2 typePos = ImVec2(boxMax.x - typeTextSize.x - 4.0f, boxMax.y - typeTextSize.y - 4.0f);
	ImVec4 badgeColor = directoryEntry.is_directory()
		? ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)
		: ImVec4(visual.AccentColor.x, visual.AccentColor.y, visual.AccentColor.z, 0.90f);
	drawList->AddText(typePos, ImGui::GetColorU32(badgeColor), typeStr.c_str());
}

void ContentBrowserGridItem::ContextMenu_Material(const std::filesystem::path& path)
{
	std::string filename = path.filename().string();
	for (auto& c : filename) c = static_cast<char>(tolower(c));

	const bool isInstance = filename.ends_with(".bammatinst") || filename.ends_with(".bammatinst.json");
	const bool isBaseMat = !isInstance;

	ImGui::Separator();

	// 1. 수동 Reload Material 메뉴 (공통)
	if (ImGui::MenuItem((ICON_FA_ROTATE " " + LOCAL_LABEL("ContentBrowser.ReloadMaterial")).c_str()))
	{
		Engine::ResourceManager& rm = Engine::ResourceManager::Get();
		std::string targetStem = path.stem().string();
		for (char& c : targetStem) c = static_cast<char>(tolower(c));
		if (targetStem.ends_with(".bammat")) targetStem = targetStem.substr(0, targetStem.length() - 7);
		if (targetStem.ends_with(".bammatinst")) targetStem = targetStem.substr(0, targetStem.length() - 11);

		bool reloaded = false;
		auto tryReload = [&](const auto& handles) {
			for (const auto& handle : handles)
			{
				auto* mat = static_cast<Engine::MaterialInterface*>(rm.GetResource(handle));
				if (!mat) continue;

				std::string keyStem = std::filesystem::path(mat->GetKey()).stem().string();
				for (char& c : keyStem) c = static_cast<char>(tolower(c));
				if (keyStem.ends_with(".bammat")) keyStem = keyStem.substr(0, keyStem.length() - 7);
				if (keyStem.ends_with(".bammatinst")) keyStem = keyStem.substr(0, keyStem.length() - 11);

				std::string fileStem = std::filesystem::path(mat->GetPath()).stem().string();
				for (char& c : fileStem) c = static_cast<char>(tolower(c));
				if (fileStem.ends_with(".bammat")) fileStem = fileStem.substr(0, fileStem.length() - 7);
				if (fileStem.ends_with(".bammatinst")) fileStem = fileStem.substr(0, fileStem.length() - 11);

				if (keyStem == targetStem || fileStem == targetStem)
				{
					if (IsSuccess(mat->Reload(path)))
					{
						reloaded = true;
						NotificationManager::Get().AddNotification(LOCAL("Notification.MaterialReloaded"),
							LOCAL_FMT("Notification.ReloadedFile", path.filename().string()), ELogLevel::Info, 3.0f);
					}
				}
			}
			};

		tryReload(rm.GetResourceHandles<Engine::Material>());
		tryReload(rm.GetResourceHandles<Engine::MaterialInstance>());

		if (!reloaded)
		{
			NotificationManager::Get().AddNotification(LOCAL("Notification.MaterialNotLoaded"),
				LOCAL_FMT("Notification.InactiveFile", path.filename().string()), ELogLevel::Info, 3.0f);
		}
	}

	// 2. Base Material 전용: Material Instance (.bammatinst.json) 생성
	if (isBaseMat)
	{
		if (ImGui::MenuItem((ICON_FA_CLONE " " + LOCAL_LABEL("ContentBrowser.CreateMaterialInstance")).c_str()))
		{
			std::wstring baseStem = path.stem().wstring();
			if (baseStem.ends_with(L".bammat")) baseStem = baseStem.substr(0, baseStem.length() - 7);

			std::filesystem::path instPath = path.parent_path() / (baseStem + L".bammatinst.json");
			int suffix = 1;
			while (std::filesystem::exists(instPath))
			{
				instPath = path.parent_path() /
					(baseStem + L"_" + std::to_wstring(suffix++) + L".bammatinst.json");
			}

			auto& rm = Engine::ResourceManager::Get();
			std::wstring relBasePath = std::filesystem::relative(path).wstring();
			auto baseHandle = rm.GetResourceHandle<Engine::Material>(relBasePath);
			if (!baseHandle.IsValid())
			{
				baseHandle = static_cast<Engine::ResourceHandle<Engine::Material>>(rm.LoadFile(relBasePath));
			}

			if (baseHandle.IsValid())
			{
				MaterialInstanceDesc desc = {};
				desc.BaseMaterialHandle = baseHandle;
				desc.Key = std::filesystem::relative(instPath).wstring();
				desc.Path = instPath.wstring();

				// 1. 메모리에 인스턴스 객체 직접 생성
				auto* newInst = Engine::MaterialInstance::Create(&desc);
				if (newInst)
				{
					// 2. 디스크에 .bammatinst.json 파일로 저장
					if (rm.SaveToJsonFile(newInst, instPath.wstring()) == EResult::Success)
					{
						// 3. 에셋 매니저에 정식 등록 및 알림
						rm.LoadFile(std::filesystem::relative(instPath).wstring());
						EDITOR_LOG_INFO("Created Material Instance (JSON): {}", instPath.string());
						NotificationManager::Get().AddNotification(LOCAL("Notification.InstanceCreated"),
							instPath.filename().string(), ELogLevel::Info, 3.0f);
					}
					rm.DestroyResource(newInst);
				}
			}
		}
	}

	// 3. Material Instance 전용: Base Material 열기
	if (isInstance)
	{
		if (ImGui::MenuItem((ICON_FA_LINK " " + LOCAL_LABEL("ContentBrowser.OpenBaseMaterial")).c_str()))
		{
			auto& rm = Engine::ResourceManager::Get();
			std::wstring relPath = std::filesystem::relative(path).wstring();
			auto instHandle = rm.GetResourceHandle<Engine::MaterialInstance>(relPath);
			if (!instHandle.IsValid())
			{
				instHandle = static_cast<Engine::ResourceHandle<Engine::MaterialInstance>>(rm.LoadFile(relPath));
			}

			if (instHandle.IsValid() && instHandle.Get() && instHandle.Get()->GetBaseMaterial())
			{
				std::filesystem::path baseMatPath = instHandle.Get()->GetBaseMaterial()->GetPath();
				if (baseMatPath.empty())
					baseMatPath = instHandle.Get()->GetBaseMaterial()->GetKey();

				// 디스크에 .bammat.json 파일이 존재하는지 우선 탐색
				if (!std::filesystem::exists(baseMatPath))
				{
					if (std::filesystem::exists(baseMatPath.string() + ".bammat.json"))
						baseMatPath = baseMatPath.string() + ".bammat.json";
					else if (std::filesystem::exists(baseMatPath.string() + ".json"))
						baseMatPath = baseMatPath.string() + ".json";
				}

				if (std::filesystem::exists(baseMatPath))
				{
					SelectionManager::Get().SetSelectedAsset(baseMatPath);
					ImGuiManager::Get().RequestOpenResourceEditor(baseMatPath);
				}
				else
				{
					NotificationManager::Get().AddNotification(LOCAL("Notification.BaseMaterialNotFound"),
						baseMatPath.string(), ELogLevel::Error, 3.0f);
				}
			}
		}
	}
}

void ContentBrowserGridItem::ContextMenu_Model(const std::filesystem::path& path)
{
	std::string filename = ToLowerAscii(path.filename().string());
	std::string ext = ToLowerAscii(path.extension().string());
	ImGui::Separator();
	if (filename.ends_with(".bammesh") || filename.ends_with(".bammesh.json"))
	{
		if (ImGui::MenuItem((ICON_FA_ROTATE " " + LOCAL_LABEL("ContentBrowser.ReloadMesh")).c_str()))
		{
			Engine::ResourceManager& rm = Engine::ResourceManager::Get();
			auto meshHandles = rm.GetResourceHandles<Engine::Mesh>();

			std::string targetStem = path.stem().string();
			for (char& c : targetStem) c = static_cast<char>(tolower(c));
			if (targetStem.ends_with(".bammesh")) targetStem = targetStem.substr(0, targetStem.length() - 8);

			bool reloaded = false;
			for (const auto& handle : meshHandles)
			{
				auto* mesh = static_cast<Engine::Mesh*>(rm.GetResource(handle));
				if (!mesh) continue;

				std::string keyStem = std::filesystem::path(mesh->GetKey()).stem().string();
				for (char& c : keyStem) c = static_cast<char>(tolower(c));
				if (keyStem.ends_with(".bammesh")) keyStem = keyStem.substr(0, keyStem.length() - 8);

				std::string fileStem = std::filesystem::path(mesh->GetPath()).stem().string();
				for (char& c : fileStem) c = static_cast<char>(tolower(c));
				if (fileStem.ends_with(".bammesh")) fileStem = fileStem.substr(0, fileStem.length() - 8);

				if (keyStem == targetStem || fileStem == targetStem)
				{
					if (IsSuccess(mesh->Reload(path)))
					{
						reloaded = true;
						NotificationManager::Get().AddNotification(LOCAL("Notification.MeshReloaded"),
							LOCAL_FMT("Notification.ReloadedFile", path.filename().string()), ELogLevel::Info, 3.0f);
					}
				}
			}
			if (!reloaded)
			{
				NotificationManager::Get().AddNotification(LOCAL("Notification.MeshNotLoaded"),
					LOCAL_FMT("Notification.InactiveFile", path.filename().string()), ELogLevel::Info, 3.0f);
			}
		}
	}
	else if (ext == ".fbx" || ext == ".obj" || ext == ".gltf" || ext == ".glb")
	{
		if (ImGui::MenuItem((ICON_FA_FILE_IMPORT " " + LOCAL_LABEL("ContentBrowser.Reimport")).c_str()))
		{
			AssetManager::Get().ImportAsync(path, path.parent_path());
		}
		if (ImGui::MenuItem((ICON_FA_SLIDERS " " + LOCAL_LABEL("ContentBrowser.ReimportWithOptions")).c_str()))
		{
			if (auto* modal = ModelImportModal::Get())
				modal->Open(path, path.parent_path());
		}
	}
	else if (filename.ends_with(".bammodel") || filename.ends_with(".bammodel.json"))
	{
		std::filesystem::path sourceCandidate;
		std::string stem = path.stem().string();
		if (stem.ends_with(".bammodel")) stem = stem.substr(0, stem.length() - 9);

		const std::vector<std::string> exts = { ".glb", ".gltf", ".fbx", ".obj" };
		for (const auto& candidateExt : exts)
		{
			std::filesystem::path candidatePath = path.parent_path() / (stem + candidateExt);
			if (std::filesystem::exists(candidatePath))
			{
				sourceCandidate = candidatePath;
				break;
			}
		}

		if (!sourceCandidate.empty())
		{
			if (ImGui::MenuItem((ICON_FA_SLIDERS " " + LOCAL_LABEL("ContentBrowser.ReimportWithOptions")).c_str()))
			{
				if (auto* modal = ModelImportModal::Get())
					modal->Open(sourceCandidate, path.parent_path());
			}
		}
	}
}

void ContentBrowserGridItem::ContextMenu_Sprite(const std::filesystem::path& path)
{
	ImGui::Separator();
	if (ImGui::MenuItem((ICON_FA_CROP_SIMPLE " " + LOCAL_LABEL("ContentBrowser.OpenInSpriteEditor")).c_str()))
	{
		if (auto* spriteEditor = dynamic_cast<SpriteEditor*>(ImGuiManager::Get().GetImGuiPanel(L"Sprite Editor")))
		{
			spriteEditor->Open();
			spriteEditor->Focus();
			spriteEditor->SetTargetResource(path);
		}
	}
	if (ImGui::MenuItem((ICON_FA_BORDER_ALL " " + LOCAL_LABEL("ContentBrowser.AddToTilePalette")).c_str()))
	{
		if (auto* tilePalette = dynamic_cast<TilePalettePanel*>(ImGuiManager::Get().GetImGuiPanel(L"Tile Palette")))
		{
			tilePalette->Open();
			tilePalette->Focus();
			tilePalette->AddAssetToPalette(path);
		}
	}
}

void ContentBrowserGridItem::ContextMenu_Texture(const std::filesystem::path& path)
{
	ImGui::Separator();
	if (ImGui::MenuItem((ICON_FA_CROP_SIMPLE " " + LOCAL_LABEL("ContentBrowser.OpenInSpriteEditor")).c_str()))
	{
		if (auto* spriteEditor = dynamic_cast<SpriteEditor*>(ImGuiManager::Get().GetImGuiPanel(L"Sprite Editor")))
		{
			spriteEditor->Open();
			spriteEditor->Focus();
			spriteEditor->SetTargetResource(path);
		}
	}
	if (ImGui::MenuItem((ICON_FA_BORDER_ALL " " + LOCAL_LABEL("ContentBrowser.AddToTilePalette")).c_str()))
	{
		if (auto* tilePalette = dynamic_cast<TilePalettePanel*>(ImGuiManager::Get().GetImGuiPanel(L"Tile Palette")))
		{
			tilePalette->Open();
			tilePalette->Focus();
			tilePalette->AddAssetToPalette(path);
		}
	}
	if (ImGui::MenuItem((ICON_FA_ROTATE " " + LOCAL_LABEL("ContentBrowser.ReloadTexture")).c_str()))
	{
		Engine::ResourceManager& rm = Engine::ResourceManager::Get();
		auto texHandles = rm.GetResourceHandles<Engine::Texture>();
		std::string targetStem = path.stem().string();
		for (char& c : targetStem) c = static_cast<char>(tolower(c));
		bool reloaded = false;
		for (const auto& handle : texHandles)
		{
			auto* tex = static_cast<Engine::Texture*>(rm.GetResource(handle));
			if (!tex) continue;
			std::string keyStem = std::filesystem::path(tex->GetKey()).stem().string();
			for (char& c : keyStem) c = static_cast<char>(tolower(c));
			std::string fileStem = std::filesystem::path(tex->GetPath()).stem().string();
			for (char& c : fileStem) c = static_cast<char>(tolower(c));
			if (keyStem == targetStem || fileStem == targetStem)
			{
				if (IsSuccess(tex->Reload(path)))
				{
					reloaded = true;
					NotificationManager::Get().AddNotification(LOCAL("Notification.TextureReloaded"),
						LOCAL_FMT("Notification.ReloadedFile", path.filename().string()), ELogLevel::Info, 3.0f);
				}
			}
		}
		if (!reloaded)
		{
			NotificationManager::Get().AddNotification(LOCAL("Notification.TextureNotLoaded"),
				LOCAL_FMT("Notification.InactiveFile", path.filename().string()), ELogLevel::Info, 3.0f);
		}
	}
	if (ImGui::MenuItem((ICON_FA_IMAGE " " + LOCAL_LABEL("ContentBrowser.CreateSprite")).c_str()))
	{
		// 1. 중복 없는 .bamsprite.json 경로 결정
		std::filesystem::path spritePath = path.parent_path() / (path.stem().wstring() + L".bamsprite.json");
		int suffix = 1;
		while (std::filesystem::exists(spritePath))
		{
			spritePath = path.parent_path() / (path.stem().string() + "_" + std::to_string(suffix) + ".bamsprite.json");
			++suffix;
		}

		std::wstring relTexPath = std::filesystem::relative(path).wstring();
		auto& rm = Engine::ResourceManager::Get();
		auto texHandle = rm.LoadFile(relTexPath);

		if (texHandle.IsValid())
		{
			// 2. 텍스처를 래핑하는 임시 Sprite 객체 생성
			SpriteCreateDesc desc = {};
			desc.texture = static_cast<Engine::ResourceHandle<Engine::Texture>>(texHandle);
			desc.Key = std::filesystem::relative(spritePath).wstring();
			desc.Path = spritePath.wstring();
			desc.pivot = vec2(0.5f, 0.5f);
			desc.region = Rect(0.f, 0.f, 0.f, 0.f);

			Engine::Sprite* newSprite = Engine::Sprite::Create(&desc);
			if (newSprite)
			{
				// 3. 디스크에 .bamsprite.json 파일로 먼저 저장
				if (rm.SaveToJsonFile(newSprite, spritePath.wstring()) == EResult::Success)
				{
					// 4. 저장된 파일을 ResourceManager에 정식 로드 등록
					rm.LoadFile(std::filesystem::relative(spritePath).wstring());
					EDITOR_LOG_INFO("Successfully created sprite: {}", spritePath.string());
				}
				rm.DestroyResource(newSprite); // 임시 객체 해제
			}
		}
	}
}

void ContentBrowserGridItem::ContextMenu_Shader(const std::filesystem::path& path)
{
	std::string ext = path.extension().string();
	for (auto& c : ext) c = static_cast<char>(tolower(c));

	ImGui::Separator();

	if (ext == ".hlsli")
	{
		if (ImGui::MenuItem((ICON_FA_ROTATE " " + LOCAL_LABEL("ContentBrowser.RecompileAllShaders")).c_str()))
		{
			HotReloadManager::Get().GetShaderReloader().RequestRecompileAll();
		}
	}
	else
	{
		if (ImGui::MenuItem((ICON_FA_CODE " " + LOCAL_LABEL("ContentBrowser.RecompileShader")).c_str()))
		{
			HotReloadManager::Get().GetShaderReloader().RecompileFile(path);
		}
	}
}

bool ContentBrowserGridItem::ConvertJsonToBinary(const std::filesystem::path& path)
{
	// 1. 파일 헤더로부터 실제 리소스 타입 감지 (파일명 무관)
	Engine::EResourceType assetType = PeekResourceType(path);

	// 2. 해당 타입에 맞는 표준 바이너리 확장자 결정
	std::string binExt = Engine::GetResourceBinaryExtension(assetType);

	// 3. 출력 경로 결정 (Wood.json -> Wood.bammat, foo.bammat.json -> foo.bammat)
	std::string baseStem = path.stem().string();
	if (baseStem.ends_with(binExt))
		baseStem = baseStem.substr(0, baseStem.length() - binExt.length());

	std::filesystem::path outPath = path.parent_path() / (baseStem + binExt);

	auto& rm = Engine::ResourceManager::Get();
	std::wstring relPath = std::filesystem::relative(path).wstring();
	Engine::Handle handle = rm.LoadFile(relPath);
	Engine::Resource* res = rm.GetResource(handle);
	if (!res)
	{
		NotificationManager::Get().AddNotification(LOCAL("Notification.BakeFailed"),
			LOCAL_FMT("Notification.JsonLoadFailed", path.filename().string()), ELogLevel::Error, 3.0f);
		return false;
	}

	const EResult result = rm.SaveToBeveFile(res, outPath.wstring());
	if (IsSuccess(result))
	{
		NotificationManager::Get().AddNotification(LOCAL("Notification.BakeSucceeded"),
			LOCAL_FMT("Notification.CreatedFile", outPath.filename().string()), ELogLevel::Info, 3.0f);
		EDITOR_LOG_INFO("[Asset Baking] Baked {} (Type: {}) -> {}",
			path.string(), static_cast<uint32>(assetType), outPath.string());
		return true;
	}

	NotificationManager::Get().AddNotification(LOCAL("Notification.BakeFailed"),
		LOCAL_FMT("Notification.BinarySaveFailed", outPath.filename().string()), ELogLevel::Error, 3.0f);
	return false;
}

bool ContentBrowserGridItem::ConvertBinaryToJson(const std::filesystem::path& path)
{
	std::filesystem::path outPath = path.parent_path() / (path.filename().wstring() + L".json");

	auto& rm = Engine::ResourceManager::Get();
	std::wstring relPath = std::filesystem::relative(path).wstring();
	Engine::Handle handle = rm.LoadFile(relPath);
	Engine::Resource* res = rm.GetResource(handle);
	if (!res)
	{
		NotificationManager::Get().AddNotification(LOCAL("Notification.ExportFailed"),
			LOCAL_FMT("Notification.BinaryLoadFailed", path.filename().string()), ELogLevel::Error, 3.0f);
		return false;
	}

	const EResult result = rm.SaveToJsonFile(res, outPath.wstring());
	if (IsSuccess(result))
	{
		NotificationManager::Get().AddNotification(LOCAL("Notification.ExportSucceeded"),
			LOCAL_FMT("Notification.CreatedFile", outPath.filename().string()), ELogLevel::Info, 3.0f);
		EDITOR_LOG_INFO("[Asset Export] Successfully exported binary to JSON: {} -> {}",
			path.string(), outPath.string());
		return true;
	}

	NotificationManager::Get().AddNotification(LOCAL("Notification.ExportFailed"),
		LOCAL_FMT("Notification.JsonSaveFailed", outPath.filename().string()), ELogLevel::Error, 3.0f);
	return false;
}