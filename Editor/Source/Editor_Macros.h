#pragma once

#define LOCAL(key) LocalizationManager::Get().GetText(key)
#define LOCAL_CSTR(key) LocalizationManager::Get().GetText(key).c_str()
// Owns the translated label; the suffix keeps ImGui identity independent of the language.
#define LOCAL_LABEL(key) (LOCAL(key) + "###" + (key))
#define LOCAL_FMT(key, ...) fmt::format(fmt::runtime(LOCAL(key)), __VA_ARGS__)

#define RHI_TYPE ERHIType::DirectX12
#define GRAPHICS_BACKEND EGraphicsBackend::Vulkan
#define RESOURCE_PATH L"Resources/"
#define LOG_PATH L"Logs/EngineLog.log"