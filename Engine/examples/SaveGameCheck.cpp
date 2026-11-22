#include "SaveGameManager.h"
#include "SaveGameTypes.h"
#include "Scene.h"
#include "TransformComponent.h"
#include "HealthComponent.h"
#include <archive/JsonArchive.h>

#include <iostream>
#include <filesystem>
#include <fstream>
#include <cmath>
#include <memory>
#include <string>
#include <cassert>

namespace
{
    using namespace Engine;

    struct ReleaseObject
    {
        void operator()(Base* object) const
        {
            if (object)
            {
                object->Release();
            }
        }
    };

    bool ApproxEqual(float a, float b, float epsilon = 0.0001f)
    {
        return std::fabs(a - b) <= epsilon;
    }

    bool ApproxEqual(double a, double b, double epsilon = 0.0001)
    {
        return std::fabs(a - b) <= epsilon;
    }

    bool ApproxEqualVec3(const vec3& a, const vec3& b, float epsilon = 0.0001f)
    {
        return ApproxEqual(a.x, b.x, epsilon) &&
            ApproxEqual(a.y, b.y, epsilon) &&
            ApproxEqual(a.z, b.z, epsilon);
    }

    std::filesystem::path CreateTestTempDirectory(const std::string& testName)
    {
        const auto tempRoot = std::filesystem::temp_directory_path() / "BamEngine_SaveGameCheck" / testName;
        std::error_code ec;
        std::filesystem::remove_all(tempRoot, ec);
        std::filesystem::create_directories(tempRoot, ec);
        return tempRoot;
    }

    void CleanupDirectory(const std::filesystem::path& dir)
    {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec);
    }

    bool TestPathTraversalRejection()
    {
        std::cout << "[Test 1] Slot Name Validation and Path Traversal Rejection..." << std::endl;

        // IsValidSlotName 정적 함수 검증
        if (SaveGameManager::IsValidSlotName("")) return false;
        if (SaveGameManager::IsValidSlotName(".")) return false;
        if (SaveGameManager::IsValidSlotName("..")) return false;
        if (SaveGameManager::IsValidSlotName("../evil")) return false;
        if (SaveGameManager::IsValidSlotName("evil/slot")) return false;
        if (SaveGameManager::IsValidSlotName("evil\\slot")) return false;
        if (SaveGameManager::IsValidSlotName("slot:name")) return false;
        if (SaveGameManager::IsValidSlotName("slot*name")) return false;
        if (SaveGameManager::IsValidSlotName("slot?name")) return false;
        if (SaveGameManager::IsValidSlotName("slot\"name")) return false;
        if (SaveGameManager::IsValidSlotName("slot<name")) return false;
        if (SaveGameManager::IsValidSlotName("slot>name")) return false;
        if (SaveGameManager::IsValidSlotName("slot|name")) return false;
        if (!SaveGameManager::IsValidSlotName("Valid_Slot-01")) return false;

        const auto testDir = CreateTestTempDirectory("PathTraversal");
        SaveGameManager manager(testDir);

        SaveGameData dummy;
        if (manager.WriteSlot("../evil", dummy)) return false;
        if (manager.ReadSlot("../evil", dummy)) return false;
        if (manager.HasSlot("../evil")) return false;
        if (manager.DeleteSlot("../evil")) return false;

        CleanupDirectory(testDir);
        std::cout << "  -> Passed!" << std::endl;
        return true;
    }

    bool TestRoundtripAllTypesAndMetadata()
    {
        std::cout << "[Test 2] Slot Write/Read Roundtrip (All Flag Types & Metadata)..." << std::endl;

        const auto testDir = CreateTestTempDirectory("Roundtrip");
        SaveGameManager manager(testDir);

        SaveGameData data;
        data.metadata.slotName = "Slot_AllTypes";
        data.metadata.sceneUri = "Project://Scenes/Level_Dungeon.json";
        data.metadata.checkpointId = "CP_BossChamber_01";
        data.metadata.timestampIso8601 = "2026-11-22T15:30:00Z";
        data.metadata.playTimeSeconds = 7245.5;
        data.metadata.formatVersion = 1;

        data.playerPosition = vec3(123.5f, -45.25f, 678.0f);
        data.playerEulerRotation = vec3(10.0f, 45.0f, -5.0f);
        data.playerHealth = 88.5f;
        data.playerMaxHealth = 150.0f;

        data.intFlags["coins"] = 1050;
        data.intFlags["keys_found"] = 4;
        data.intFlags["negative_flag"] = -999999LL;

        data.floatFlags["stamina"] = 65.25f;
        data.floatFlags["attack_multiplier"] = 1.35f;

        data.boolFlags["boss_unlocked"] = true;
        data.boolFlags["permadeath"] = false;

        data.stringFlags["hero_name"] = "BamHero";
        data.stringFlags["weapon"] = "Excalibur";

        if (!manager.WriteSlot("Slot_AllTypes", data))
        {
            std::cerr << "  Failed WriteSlot" << std::endl;
            return false;
        }

        if (!manager.HasSlot("Slot_AllTypes"))
        {
            std::cerr << "  Failed HasSlot" << std::endl;
            return false;
        }

        SaveGameData loaded;
        if (!manager.ReadSlot("Slot_AllTypes", loaded))
        {
            std::cerr << "  Failed ReadSlot" << std::endl;
            return false;
        }

        if (loaded.metadata.slotName != "Slot_AllTypes") return false;
        if (loaded.metadata.sceneUri != data.metadata.sceneUri) return false;
        if (loaded.metadata.checkpointId != data.metadata.checkpointId) return false;
        if (loaded.metadata.timestampIso8601 != data.metadata.timestampIso8601) return false;
        if (!ApproxEqual(loaded.metadata.playTimeSeconds, data.metadata.playTimeSeconds)) return false;
        if (loaded.metadata.formatVersion != 1) return false;

        if (!ApproxEqualVec3(loaded.playerPosition, data.playerPosition)) return false;
        if (!ApproxEqualVec3(loaded.playerEulerRotation, data.playerEulerRotation)) return false;
        if (!ApproxEqual(loaded.playerHealth, data.playerHealth)) return false;
        if (!ApproxEqual(loaded.playerMaxHealth, data.playerMaxHealth)) return false;

        if (loaded.intFlags.size() != data.intFlags.size()) return false;
        if (loaded.intFlags.at("coins") != 1050) return false;
        if (loaded.intFlags.at("keys_found") != 4) return false;
        if (loaded.intFlags.at("negative_flag") != -999999LL) return false;

        if (loaded.floatFlags.size() != data.floatFlags.size()) return false;
        if (!ApproxEqual(loaded.floatFlags.at("stamina"), 65.25f)) return false;
        if (!ApproxEqual(loaded.floatFlags.at("attack_multiplier"), 1.35f)) return false;

        if (loaded.boolFlags.size() != data.boolFlags.size()) return false;
        if (loaded.boolFlags.at("boss_unlocked") != true) return false;
        if (loaded.boolFlags.at("permadeath") != false) return false;

        if (loaded.stringFlags.size() != data.stringFlags.size()) return false;
        if (loaded.stringFlags.at("hero_name") != "BamHero") return false;
        if (loaded.stringFlags.at("weapon") != "Excalibur") return false;

        // JsonArchive 파싱 안전성 확인
        std::string jsonStr;
        if (!SerializeToJson(data, jsonStr)) return false;
        archive::JsonArchive ar;
        if (!ar.Parse(jsonStr)) return false;

        CleanupDirectory(testDir);
        std::cout << "  -> Passed!" << std::endl;
        return true;
    }

    bool TestAtomicOverwrite()
    {
        std::cout << "[Test 3] Atomic Overwrite (.tmp atomic replacement)..." << std::endl;

        const auto testDir = CreateTestTempDirectory("AtomicOverwrite");
        SaveGameManager manager(testDir);

        SaveGameData initialData;
        initialData.metadata.playTimeSeconds = 100.0;
        initialData.playerPosition = vec3(1.0f, 2.0f, 3.0f);

        if (!manager.WriteSlot("AtomicSlot", initialData)) return false;

        SaveGameData checkInitial;
        if (!manager.ReadSlot("AtomicSlot", checkInitial)) return false;
        if (!ApproxEqual(checkInitial.metadata.playTimeSeconds, 100.0)) return false;

        // 원자적 덮어쓰기
        SaveGameData updatedData;
        updatedData.metadata.playTimeSeconds = 9999.0;
        updatedData.playerPosition = vec3(999.0f, 888.0f, 777.0f);

        if (!manager.WriteSlot("AtomicSlot", updatedData)) return false;

        SaveGameData checkUpdated;
        if (!manager.ReadSlot("AtomicSlot", checkUpdated)) return false;
        if (!ApproxEqual(checkUpdated.metadata.playTimeSeconds, 9999.0)) return false;
        if (!ApproxEqualVec3(checkUpdated.playerPosition, vec3(999.0f, 888.0f, 777.0f))) return false;

        // 디렉터리 내에 미완료된 .tmp 파일이 남아있지 않은지 검증
        std::error_code ec;
        for (const auto& entry : std::filesystem::directory_iterator(testDir, ec))
        {
            if (entry.path().extension() == ".tmp")
            {
                std::cerr << "  Found dangling .tmp file: " << entry.path().string() << std::endl;
                return false;
            }
        }

        CleanupDirectory(testDir);
        std::cout << "  -> Passed!" << std::endl;
        return true;
    }

    bool TestListSlotsHasSlotDeleteSlot()
    {
        std::cout << "[Test 4] ListSlots / HasSlot / DeleteSlot..." << std::endl;

        const auto testDir = CreateTestTempDirectory("SlotCRUD");
        SaveGameManager manager(testDir);

        SaveGameData slot1, slot2, slot3;
        slot1.metadata.sceneUri = "Scene1.json";
        slot2.metadata.sceneUri = "Scene2.json";
        slot3.metadata.sceneUri = "Scene3.json";

        if (!manager.WriteSlot("Slot1", slot1)) return false;
        if (!manager.WriteSlot("Slot2", slot2)) return false;
        if (!manager.WriteSlot("Slot3", slot3)) return false;

        auto slots = manager.ListSlots();
        if (slots.size() != 3) return false;
        if (slots[0].slotName != "Slot1" || slots[1].slotName != "Slot2" || slots[2].slotName != "Slot3") return false;

        if (!manager.HasSlot("Slot1") || !manager.HasSlot("Slot2") || !manager.HasSlot("Slot3")) return false;

        // Slot2 삭제
        if (!manager.DeleteSlot("Slot2")) return false;
        if (manager.HasSlot("Slot2")) return false;

        slots = manager.ListSlots();
        if (slots.size() != 2) return false;
        if (slots[0].slotName != "Slot1" || slots[1].slotName != "Slot3") return false;

        // Slot1, Slot3 삭제
        if (!manager.DeleteSlot("Slot1")) return false;
        if (!manager.DeleteSlot("Slot3")) return false;

        slots = manager.ListSlots();
        if (!slots.empty()) return false;

        CleanupDirectory(testDir);
        std::cout << "  -> Passed!" << std::endl;
        return true;
    }

    bool TestCorruptedJsonAndVersionDefense()
    {
        std::cout << "[Test 5] Corrupted JSON and Version Defense..." << std::endl;

        const auto testDir = CreateTestTempDirectory("CorruptedDefense");
        SaveGameManager manager(testDir);

        // 1. 손상된 문법 JSON 파일
        {
            std::ofstream f(testDir / "Corrupted.json");
            f << "{ \"metadata\": { \"slotName\": ";
            f.close();
        }

        SaveGameData loaded;
        if (manager.ReadSlot("Corrupted", loaded))
        {
            std::cerr << "  Expected ReadSlot to fail on corrupted JSON, but it succeeded!" << std::endl;
            return false;
        }

        // 2. formatVersion = 0 방어
        {
            std::ofstream f(testDir / "VersionZero.json");
            f << "{ \"metadata\": { \"slotName\": \"VersionZero\", \"formatVersion\": 0 } }";
            f.close();
        }
        if (manager.ReadSlot("VersionZero", loaded))
        {
            std::cerr << "  Expected ReadSlot to fail on formatVersion 0, but it succeeded!" << std::endl;
            return false;
        }

        // 3. 미지원 미래 버전 (예: 9999) 방어
        {
            std::ofstream f(testDir / "FutureVersion.json");
            f << "{ \"metadata\": { \"slotName\": \"FutureVersion\", \"formatVersion\": 9999 } }";
            f.close();
        }
        if (manager.ReadSlot("FutureVersion", loaded))
        {
            std::cerr << "  Expected ReadSlot to fail on future formatVersion, but it succeeded!" << std::endl;
            return false;
        }

        CleanupDirectory(testDir);
        std::cout << "  -> Passed!" << std::endl;
        return true;
    }

    bool TestMissingFieldsBackwardCompatibility()
    {
        std::cout << "[Test 6] Missing Fields Backward Compatibility..." << std::endl;

        const auto testDir = CreateTestTempDirectory("BackwardCompat");
        SaveGameManager manager(testDir);

        // 플래그 컬렉션, 회전 등이 누락된 최소 JSON
        {
            std::ofstream f(testDir / "Minimal.json");
            f << "{\n"
                 "  \"metadata\": {\n"
                 "    \"slotName\": \"Minimal\",\n"
                 "    \"formatVersion\": 1\n"
                 "  },\n"
                 "  \"playerPosition\": [50.0, 10.0, -20.0]\n"
                 "}\n";
            f.close();
        }

        SaveGameData loaded;
        if (!manager.ReadSlot("Minimal", loaded))
        {
            std::cerr << "  Expected ReadSlot to succeed on partial JSON, but it failed!" << std::endl;
            return false;
        }

        if (!ApproxEqualVec3(loaded.playerPosition, vec3(50.0f, 10.0f, -20.0f))) return false;
        if (!ApproxEqual(loaded.playerHealth, 100.0f)) return false;
        if (!ApproxEqual(loaded.playerMaxHealth, 100.0f)) return false;
        if (!loaded.intFlags.empty()) return false;
        if (!loaded.boolFlags.empty()) return false;
        if (!loaded.floatFlags.empty()) return false;
        if (!loaded.stringFlags.empty()) return false;

        CleanupDirectory(testDir);
        std::cout << "  -> Passed!" << std::endl;
        return true;
    }

    bool TestSceneCheckpointEntityCaptureAndApply()
    {
        std::cout << "[Test 7] Scene Entity Checkpoint Capture and Apply..." << std::endl;

        std::unique_ptr<Scene, ReleaseObject> scene(Scene::Create());
        if (!scene)
        {
            std::cerr << "  Failed to create scene" << std::endl;
            return false;
        }

        Entity& player = scene->CreateEntity();
        const auto playerHandle = player.GetEntityHandle();

        auto& tf = scene->AddComponent<TransformComponent>(player);
        if (!tf.SetLocalPosition(vec3(15.0f, 25.0f, 35.0f))) return false;
        if (!tf.SetEulerRotation(vec3(0.0f, 90.0f, 0.0f))) return false;

        auto& hp = scene->AddComponent<HealthComponent>(player);
        hp.maxHealth = 250.0f;
        hp.currentHealth = 180.0f;
        hp.isDead = false;

        // 1. 체크포인트 캡처
        SaveGameData cpData;
        if (!SaveGameManager::CaptureCheckpointFromEntity(*scene, playerHandle,
            "Project://Scenes/Level1.json", "CP_Gate", cpData))
        {
            std::cerr << "  Failed to capture checkpoint" << std::endl;
            return false;
        }

        if (cpData.metadata.sceneUri != "Project://Scenes/Level1.json") return false;
        if (cpData.metadata.checkpointId != "CP_Gate") return false;
        if (cpData.metadata.timestampIso8601.empty()) return false;
        if (cpData.metadata.formatVersion != 1) return false;
        if (!ApproxEqualVec3(cpData.playerPosition, vec3(15.0f, 25.0f, 35.0f))) return false;
        if (!ApproxEqualVec3(cpData.playerEulerRotation, vec3(0.0f, 90.0f, 0.0f))) return false;
        if (!ApproxEqual(cpData.playerHealth, 180.0f)) return false;
        if (!ApproxEqual(cpData.playerMaxHealth, 250.0f)) return false;

        // 2. 엔티티 상태 변경
        if (!tf.SetLocalPosition(vec3(0.0f, 0.0f, 0.0f))) return false;
        if (!tf.SetEulerRotation(vec3(0.0f, 0.0f, 0.0f))) return false;
        hp.currentHealth = 10.0f;

        // 3. 체크포인트 복원 적용
        if (!SaveGameManager::ApplyCheckpointToEntity(*scene, playerHandle, cpData))
        {
            std::cerr << "  Failed to apply checkpoint" << std::endl;
            return false;
        }

        if (!ApproxEqualVec3(tf.position, vec3(15.0f, 25.0f, 35.0f))) return false;
        if (!ApproxEqualVec3(tf.eulerRotation, vec3(0.0f, 90.0f, 0.0f))) return false;
        if (!ApproxEqual(hp.currentHealth, 180.0f)) return false;
        if (!ApproxEqual(hp.maxHealth, 250.0f)) return false;
        if (hp.isDead) return false;

        // 4. 유효하지 않은 엔티티 핸들 가드 검증
        if (SaveGameManager::CaptureCheckpointFromEntity(*scene, entt::null, "", "", cpData)) return false;
        if (SaveGameManager::ApplyCheckpointToEntity(*scene, entt::null, cpData)) return false;

        std::cout << "  -> Passed!" << std::endl;
        return true;
    }
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << "  SaveGameSystem Unit Verification Checks" << std::endl;
    std::cout << "========================================" << std::endl;

    if (!TestPathTraversalRejection())
    {
        std::cerr << "[FAIL] TestPathTraversalRejection failed!" << std::endl;
        return 1;
    }

    if (!TestRoundtripAllTypesAndMetadata())
    {
        std::cerr << "[FAIL] TestRoundtripAllTypesAndMetadata failed!" << std::endl;
        return 2;
    }

    if (!TestAtomicOverwrite())
    {
        std::cerr << "[FAIL] TestAtomicOverwrite failed!" << std::endl;
        return 3;
    }

    if (!TestListSlotsHasSlotDeleteSlot())
    {
        std::cerr << "[FAIL] TestListSlotsHasSlotDeleteSlot failed!" << std::endl;
        return 4;
    }

    if (!TestCorruptedJsonAndVersionDefense())
    {
        std::cerr << "[FAIL] TestCorruptedJsonAndVersionDefense failed!" << std::endl;
        return 5;
    }

    if (!TestMissingFieldsBackwardCompatibility())
    {
        std::cerr << "[FAIL] TestMissingFieldsBackwardCompatibility failed!" << std::endl;
        return 6;
    }

    if (!TestSceneCheckpointEntityCaptureAndApply())
    {
        std::cerr << "[FAIL] TestSceneCheckpointEntityCaptureAndApply failed!" << std::endl;
        return 7;
    }

    std::cout << "========================================" << std::endl;
    std::cout << "  ALL SAVE GAME SYSTEM CHECKS PASSED!" << std::endl;
    std::cout << "========================================" << std::endl;
    return 0;
}
