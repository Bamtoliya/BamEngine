#pragma once

namespace Editor
{
    // 파일창의 저장된 상태 식별자이므로 기존 숫자는 변경하지 않습니다.
    enum class EFileDialogContext
    {
        General = 0,
        SceneOpen = 1,
        ResourceImport = 2,
        ResourceOpen = 3,
        SceneSave = 4,
        GraphOpen = 5,
        GraphSave = 6,
        PrefabSave = 7,
        HeightmapOpen = 8,
        HeightmapSave = 9,
        ParticleOpen = 10,
        ParticleSave = 11,
        SpriteOpen = 12,
        SpriteSave = 13,
        TimelineOpen = 14,
        TimelineSave = 15,
        ModelImport = 16,
        MaterialOpen = 17,
        MaterialSave = 18,
        PrefabOpen = 19
    };
}