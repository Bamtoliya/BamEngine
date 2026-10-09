#pragma once

#define LOCAL(key) LocalizationManager::Get().GetText(key)
#define LOCAL_CSTR(key) LocalizationManager::Get().GetText(key).c_str()
#define LOCAL_FMT(key, ...) fmt::format(fmt::runtime(LOCAL(key)), __VA_ARGS__)

#define RHI_TYPE ERHIType::DirectX12
#define GRAPHICS_BACKEND EGraphicsBackend::Vulkan
#define RESOURCE_PATH L"Resources/"
#define LOG_PATH L"Logs/EngineLog.log"