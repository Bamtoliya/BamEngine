#pragma once

#include <cstddef>
#include <cstdint>
#include <imgui.h>

namespace Engine { class RHITexture; }

namespace Editor
{
	// Internal ABI for plugins built with the same Engine, ImGui and toolchain.
	struct ResourcePanelPluginHost
	{
		ImGuiContext* ImGui;
		ImGuiMemAllocFunc Allocate;
		ImGuiMemFreeFunc Free;
		void* AllocatorData;
		ImTextureID (*GetTextureId)(Engine::RHITexture*);
		void (*DrawActionMenuItem)(const char*, const char*);
		const char* (*GetShortcutHint)(const char*);
		bool (*CanExecuteAction)(const char*);
		bool (*ExecuteAction)(const char*);
		void* Panel;
		void (*DrawCloseMenuItem)(void* panel);
	};

	struct ResourcePanelPluginApi
	{
		std::uint32_t ApiSize;
		std::uint32_t HostSize;
		std::uint32_t StateVersion;
		std::size_t StateSize;
		const wchar_t* PanelName;
		const char* TitleKey;
		const char* DescriptionKey;
		const char* SaveActionId;
		ImVec2 InitialSize;
		bool (*Supports)(const wchar_t* path);
		void* (*Create)(const void* state);
		void (*Destroy)(void* session);
		void (*Capture)(const void* session, void* state);
		bool (*Open)(void* session, const wchar_t* path, const wchar_t* previousPath);
		bool (*Discard)(void* session, const wchar_t* path);
		void (*Close)(void* session);
		bool (*IsDirty)(const void* session);
		bool (*CanSave)(const void* session);
		bool (*Save)(void* session, const wchar_t* path);
		bool (*CanUndo)(const void* session);
		bool (*CanRedo)(const void* session);
		void (*Undo)(void* session);
		void (*Redo)(void* session);
		void (*Update)(void* session, float dt);
		void (*DrawToolbar)(void* session, const ResourcePanelPluginHost&);
		void (*DrawQuickAccess)(void* session, const ResourcePanelPluginHost&);
		void (*DrawContent)(void* session, const ResourcePanelPluginHost&);
		void (*SaveWorkspace)(const void* session, void*, void (*set)(void*, const char*, float));
		void (*RestoreWorkspace)(void* session, const void*, float (*get)(const void*, const char*, float));
	};

	using GetResourcePanelPluginApi = const ResourcePanelPluginApi* (*)();
	inline constexpr char ResourcePanelPluginEntry[] = "BamGetResourcePanelPluginV3";
}
