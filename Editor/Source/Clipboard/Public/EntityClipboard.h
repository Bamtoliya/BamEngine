#pragma once

#include "Editor_Includes.h"
#include <string>
#include <vector>

namespace Engine { class Scene; class Entity; }

BEGIN(Editor)

struct CopiedEntityRecord
{
    uint64 OriginalParentId = 0;
    std::string SerializedJson;
};

class EntityClipboard
{
public:
    static bool HasData();
    static void Clear();
    static bool CopySelected(Engine::Scene* scene);
    static bool Paste(Engine::Scene* scene, Engine::Entity* targetParent = nullptr);
    static void OnSceneChanged();

private:
    static std::vector<CopiedEntityRecord> s_Clipboard;
    static void* s_SourceScene;
};

END