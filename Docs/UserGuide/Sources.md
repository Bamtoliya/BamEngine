# 소스 및 참고 문서

[사용자 가이드 목차](README.md)

> 기준: [main / 770e3adb](https://github.com/Bamtoliya/BamEngine/commit/770e3adb81e3a4e9ea692996df09942e74587f28), 2026-11-28 KST. 정적 조사 기반이며 빌드·실행 검증은 별도다. [상태 표기](README.md#상태-표기)

아래 링크는 작성 시점의 커밋으로 고정했다. 현재 main과 달라진 경우 새 코드와 비교한다.

## 기존 사용자/기술 문서

- [GettingStarted.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/UserGuide/GettingStarted.md)
- [EditorWorkflow.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/UserGuide/EditorWorkflow.md)
- [문서 인덱스](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/README.md)
- [MultiProjectArchitecturePlan.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/MultiProjectArchitecturePlan.md)
- [PrefabSceneWorkflowPlan.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/PrefabSceneWorkflowPlan.md)
- [GraphPanel.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/GraphPanel.md), [GraphExecution.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/GraphExecution.md)
- [UIInteraction.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/UIInteraction.md)
- [EngineEditorClientGuidelines.md](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Docs/EngineEditorClientGuidelines.md)

## 직접 확인한 소스 진입점

- [`CMakePresets.json`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/CMakePresets.json) — `CMakePresets.json`
- [`InputActions.json`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Config/InputActions.json) — `Config/InputActions.json`
- [`ToolBar.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/ToolBar/Private/ToolBar.cpp) — `Editor/Source/ImGui/ToolBar/Private/ToolBar.cpp`
- [`ProjectHubModal.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/ProjectHub/Private/ProjectHubModal.cpp) — `Editor/Source/ImGui/ProjectHub/Private/ProjectHubModal.cpp`
- [`ProjectSessionManager.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/Project/Private/ProjectSessionManager.cpp) — `Editor/Source/Project/Private/ProjectSessionManager.cpp`
- [`ProjectDescriptor.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Path/Public/ProjectDescriptor.h) — `Engine/Source/Core/Path/Public/ProjectDescriptor.h`
- [`ProjectDescriptor.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Path/Private/ProjectDescriptor.cpp) — `Engine/Source/Core/Path/Private/ProjectDescriptor.cpp`
- [`EntityFactory.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/Application/Private/EntityFactory.cpp) — `Editor/Source/Application/Private/EntityFactory.cpp`
- [`EditorCameraController.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/Application/Camera/Private/EditorCameraController.cpp) — `Editor/Source/Application/Camera/Private/EditorCameraController.cpp`
- [`HierarchyPanel.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/HierarchyPanel/Private/HierarchyPanel.cpp) — `Editor/Source/ImGui/HierarchyPanel/Private/HierarchyPanel.cpp`
- [`ContentBrowserPanel.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserPanel.cpp) — `Editor/Source/ImGui/ContentBrowserPanel/Private/ContentBrowserPanel.cpp`
- [`BuildProfilePanel.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/BuildProfilePanel/Private/BuildProfilePanel.cpp) — `Editor/Source/ImGui/BuildProfilePanel/Private/BuildProfilePanel.cpp`
- [`SpriteEditor.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/ResourceEditor/Private/SpriteEditor.cpp) — `Editor/Source/ImGui/ResourceEditor/Private/SpriteEditor.cpp`
- [`TilePalettePanel.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/TilemapEditor/Private/TilePalettePanel.cpp) — `Editor/Source/ImGui/TilemapEditor/Private/TilePalettePanel.cpp`
- [`TilemapComponent.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Tilemap/Public/TilemapComponent.h) — `Engine/Source/Tilemap/Public/TilemapComponent.h`
- [`ThirdPersonControllerComponent.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Physics/Public/ThirdPersonControllerComponent.h) — `Engine/Source/Physics/Public/ThirdPersonControllerComponent.h`
- [`RigidBodyComponent.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Physics/Public/RigidBodyComponent.h) — `Engine/Source/Physics/Public/RigidBodyComponent.h`
- [`ColliderComponent.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Physics/Public/ColliderComponent.h) — `Engine/Source/Physics/Public/ColliderComponent.h`
- [`GraphBindingComponent.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Graph/Public/GraphBindingComponent.h) — `Engine/Source/Graph/Public/GraphBindingComponent.h`
- [`InputManager.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Input/Public/InputManager.h) — `Engine/Source/Input/Public/InputManager.h`
- [`InputActionSettingsWidget.cpp`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Editor/Source/ImGui/SettingsPanel/Private/InputActionSettingsWidget.cpp) — `Editor/Source/ImGui/SettingsPanel/Private/InputActionSettingsWidget.cpp`
- [`Scene.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Scene/Public/Scene.h) — `Engine/Source/World/Scene/Public/Scene.h`
- [`Entity.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Entity/Public/Entity.h) — `Engine/Source/World/Entity/Public/Entity.h`
- [`NativeScriptComponent.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Entity/Public/NativeScriptComponent.h) — `Engine/Source/World/Entity/Public/NativeScriptComponent.h`
- [`ScriptableEntity.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/World/Entity/Public/ScriptableEntity.h) — `Engine/Source/World/Entity/Public/ScriptableEntity.h`
- [`ModuleInterface.h`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/Source/Core/Module/Public/ModuleInterface.h) — `Engine/Source/Core/Module/Public/ModuleInterface.h`
- [`BamEngineConfig.cmake`](https://github.com/Bamtoliya/BamEngine/blob/770e3adb81e3a4e9ea692996df09942e74587f28/Engine/CMake/BamEngineConfig.cmake) — `Engine/CMake/BamEngineConfig.cmake`

기능 목록은 동일 커밋의 [Git 트리](https://api.github.com/repos/Bamtoliya/BamEngine/git/trees/ff9c1f8ed3b7f8e14dcbce7bcc725d9213f4212b?recursive=1)에서 확인했다. 전체 파일 2,188개 트리 항목을 열거한 뒤 사용 흐름에 필요한 위 소스를 선별하여 읽었다. 모든 파일의 구현을 검토한 것은 아니다.
