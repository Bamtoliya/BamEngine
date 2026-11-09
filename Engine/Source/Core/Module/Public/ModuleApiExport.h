#pragma once

#define EXPORT_PLUGIN_API_ENTRY(EntryPoint, ApiType, ApiInstance) \
    extern "C" __declspec(dllexport) const ApiType* EntryPoint() noexcept \
    { \
        return &(ApiInstance); \
    }