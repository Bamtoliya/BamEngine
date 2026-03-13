#pragma once

#define LOCAL(key) LocalizationManager::Get().GetText(key)
#define LOCAL_FMT(key, ...) fmt::format(LocalizationManager::Get().GetText(key), __VA_ARGS__)