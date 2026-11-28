#include "ResourceManager.h"
#include "Mesh.h"
#include "Texture.h"
#include "Material.h"
#include "MaterialInstance.h"
#include "Shader.h"

#include <cstdio>

using namespace Engine;

static bool CheckManagerRecreation()
{
    if (!ResourceManager::Create())
        return false;

    struct ResourceScope
    {
        ~ResourceScope()
        {
            ResourceManager::Destroy();
        }
    } scope;

    // 첫 번째 Manager에서 리소스 생성
    auto oldMesh = ResourceManager::Get().AddResource<Mesh>(
        L"Test.OldMesh", Mesh::CreateEmpty());

    if (!oldMesh.IsValid())
        return false;

    const Handle oldRaw = oldMesh.GetRawHandle();

    // 기존 Handle을 유지한 채 Manager 삭제
    ResourceManager::Destroy();

    // R02 검증
    if (oldMesh.IsValid() || oldMesh.Get() != nullptr)
        return false;

    // 두 번째 Manager 생성
    if (!ResourceManager::Create())
        return false;

    auto newMesh = ResourceManager::Get().AddResource<Mesh>(
        L"Test.NewMesh", Mesh::CreateEmpty());

    if (!newMesh.IsValid())
        return false;

    const Handle newRaw = newMesh.GetRawHandle();

    // 동일한 슬롯이 재사용되는 상황 확인
    if (oldRaw.GetIndex() != newRaw.GetIndex())
        return false;

    // Generation은 반드시 달라야 한다.
    if (oldRaw.GetGeneration() == newRaw.GetGeneration())
        return false;

    // 기존 핸들이 다시 유효해져서는 안 된다.
    if (oldMesh.IsValid() || oldMesh.Get() != nullptr)
        return false;

    // 이전 Raw Handle을 다시 변환하는 경우도 거부
    ResourceHandle<Mesh> stale(oldRaw);

    if (stale.IsValid())
        return false;

    // 이전 Handle의 해제가 신규 리소스에 영향을 주면 안 된다.
    oldMesh.Reset();

    if (!newMesh.IsValid())
        return false;

    return true;
}

struct ShutdownCheckResult
{
    int FreeCount = 0;
    int DestructorCount = 0;

    bool DependencyInvalidated = false;
    bool LookupInvalidated = false;
    bool TypeIndexCleared = false;
    bool RegistrationRejected = false;
};

class ShutdownProbeResource final : public Resource
{
public:
    DECLARE_TYPE_ID()

    ShutdownProbeResource(
        const ResourceHandle<Mesh>& dependency,
        ShutdownCheckResult& result)
        : Resource(EResourceType::Unknown),
        m_Dependency(dependency),
        m_Result(result)
    {
    }

    ~ShutdownProbeResource() override
    {
        ++m_Result.DestructorCount;
    }

    void Free() override
    {
        ++m_Result.FreeCount;

        auto& manager = ResourceManager::Get();

        m_Result.DependencyInvalidated =
            !m_Dependency.IsValid() &&
            m_Dependency.Get() == nullptr;

        m_Result.LookupInvalidated =
            !manager.GetResourceHandle<Mesh>(
                L"ShutdownTest.Mesh").IsValid();

        m_Result.TypeIndexCleared =
            manager.GetResourceHandles<Mesh>().empty();

        // 내부 ResourceHandle 해제 시 Manager에 재진입
        m_Dependency.Reset();

        // 종료 중 신규 등록은 거부되어야 한다.
        auto rejected = manager.AddResource<Mesh>(
            L"ShutdownTest.Rejected",
            Mesh::CreateEmpty());

        m_Result.RegistrationRejected = !rejected.IsValid();
    }

private:
    ResourceHandle<Mesh> m_Dependency;
    ShutdownCheckResult& m_Result;
};

static bool CheckManagerShutdown()
{
    if (!ResourceManager::Create())
        return false;

    ShutdownCheckResult result;

    auto& manager = ResourceManager::Get();

    auto mesh = manager.AddResource<Mesh>(
        L"ShutdownTest.Mesh",
        Mesh::CreateEmpty());

    auto probe = manager.AddResource<ShutdownProbeResource>(
        L"ShutdownTest.Probe",
        new ShutdownProbeResource(mesh, result));

    if (!mesh.IsValid() || !probe.IsValid())
    {
        ResourceManager::Destroy();
        return false;
    }

    // 직접 Free를 호출하여 종료 중 상태와
    // 중복 Free 호출까지 검증한다.
    manager.Free();

    const bool handlesInvalid =
        !mesh.IsValid() &&
        !probe.IsValid() &&
        mesh.Get() == nullptr &&
        probe.Get() == nullptr;

    // 중복 종료 처리
    manager.Free();

    const bool passed =
        handlesInvalid &&
        result.FreeCount == 1 &&
        result.DestructorCount == 1 &&
        result.DependencyInvalidated &&
        result.LookupInvalidated &&
        result.TypeIndexCleared &&
        result.RegistrationRejected;

    ResourceManager::Destroy();

    return passed;
}

static bool CheckManagerClearResources()
{
    if (!ResourceManager::Create())
        return false;

    struct ResourceScope
    {
        ~ResourceScope()
        {
            ResourceManager::Destroy();
        }
    } scope;

    auto& manager = ResourceManager::Get();

    auto oldMesh = manager.AddResource<Mesh>(
        L"ClearTest.Mesh", Mesh::CreateEmpty());

    if (!oldMesh.IsValid())
        return false;

    const Handle oldRaw = oldMesh.GetRawHandle();

    manager.ClearResources();

    if (oldMesh.IsValid() || oldMesh.Get() != nullptr)
        return false;

    if (manager.FindHandleByKey(L"ClearTest.Mesh").IsValid())
        return false;

    if (!manager.GetResourceHandles<Mesh>().empty())
        return false;

    // 동일한 키로 신규 리소스 등록
    auto newMesh = manager.AddResource<Mesh>(
        L"ClearTest.Mesh", Mesh::CreateEmpty());

    if (!newMesh.IsValid())
        return false;

    if (newMesh.GetRawHandle() == oldRaw)
        return false;

    if (oldMesh.IsValid())
        return false;

    oldMesh.Reset();

    if (!newMesh.IsValid())
        return false;

    // 반복 초기화 안전성
    manager.ClearResources();
    manager.ClearResources();

    return !newMesh.IsValid();
}

static bool CheckReferenceCounting()
{
    if (!ResourceManager::Create())
        return false;

    struct ResourceScope
    {
        ~ResourceScope()
        {
            ResourceManager::Destroy();
        }
    } scope;

    auto& manager = ResourceManager::Get();

    // 1. 기본 참조를 가진 리소스 등록
    auto mesh = manager.AddResource<Mesh>(
        L"RefCountTest.Mesh", Mesh::CreateEmpty());

    if (!mesh.IsValid())
        return false;

    const Handle raw = mesh.GetRawHandle();

    // 2. 핸들 복사
    auto copied = mesh;

    if (!copied.IsValid())
        return false;

    copied.Reset();
    mesh.Reset();

    // 모든 외부 핸들이 제거되어도
    // 매니저 기본 참조가 남아 있어야 한다.
    if (!manager.IsValid(raw))
        return false;

    // 3. 외부 참조가 없는 상태에서 잘못된 Release
    manager.ReleaseResource(raw);
    manager.ReleaseResource(raw);

    // 기본 참조가 잘못 제거되어서는 안 된다.
    if (!manager.IsValid(raw))
        return false;

    // 4. 리소스 재획득
    ResourceHandle<Mesh> acquired(raw);

    if (!acquired.IsValid())
        return false;

    // 5. 매니저 기본 참조 제거
    if (!manager.ReleaseManagerReference(raw))
        return false;

    // 외부 핸들이 남아 있으므로 유효해야 한다.
    if (!acquired.IsValid())
        return false;

    // 6. 매니저 기본 참조 중복 제거 방지
    if (manager.ReleaseManagerReference(raw))
        return false;

    if (!acquired.IsValid())
        return false;

    // 7. 기본 참조 없이 핸들 복사 및 이동
    auto another = acquired;
    auto moved = std::move(another);

    if (!moved.IsValid() || another.IsValid())
        return false;

    acquired.Reset();

    if (!moved.IsValid())
        return false;

    // 8. 마지막 외부 핸들 제거
    moved.Reset();

    if (manager.IsValid(raw))
        return false;

    if (manager.FindHandleByKey(L"RefCountTest.Mesh").IsValid())
        return false;

    // 9. 이미 삭제된 핸들에 대한 반복 해제
    manager.ReleaseResource(raw);

    if (manager.ReleaseManagerReference(raw))
        return false;

    // 10. 이전 슬롯 재사용 확인
    auto replacement = manager.AddResource<Mesh>(
        L"RefCountTest.Replacement", Mesh::CreateEmpty());

    if (!replacement.IsValid())
        return false;

    // 이전 핸들 해제가 신규 객체에 영향을 주면 안 된다.
    manager.ReleaseResource(raw);

    if (!replacement.IsValid())
        return false;

    const Handle replacementRaw = replacement.GetRawHandle();

    // 11. 외부 참조가 없는 캐시 리소스 즉시 제거
    replacement.Reset();

    if (!manager.IsValid(replacementRaw))
        return false;

    if (!manager.ReleaseManagerReference(replacementRaw))
        return false;

    return !manager.IsValid(replacementRaw);
}

static bool CheckInvalidHandleCopy()
{
    if (!ResourceManager::Create())
        return false;

    struct ResourceScope
    {
        ~ResourceScope()
        {
            ResourceManager::Destroy();
        }
    } scope;

    auto& manager = ResourceManager::Get();

    auto mesh = manager.AddResource<Mesh>(
        L"RefCountTest.InvalidCopy", Mesh::CreateEmpty());

    if (!mesh.IsValid())
        return false;

    if (manager.DestroyResource(mesh.GetRawHandle()) != EResult::Success)
        return false;

    // 원본은 삭제된 Raw Handle을 보유하고 있다.
    if (mesh.IsValid())
        return false;

    // 복사 시 AddRef 실패를 감지해야 한다.
    auto copied = mesh;

    if (copied.IsValid() || copied.GetRawHandle().IsValid())
        return false;

    ResourceHandle<Mesh> assigned;
    assigned = mesh;

    if (assigned.IsValid() || assigned.GetRawHandle().IsValid())
        return false;

    return true;
}

static bool CheckDuplicateRegistration()
{
    if (!ResourceManager::Create())
        return false;

    struct ResourceScope
    {
        ~ResourceScope() { ResourceManager::Destroy(); }
    } scope;

    auto& manager = ResourceManager::Get();

    auto original = manager.AddResource<Mesh>(
        L"Registration.Mesh", Mesh::CreateEmpty());

    if (!original.IsValid())
        return false;

    auto* pointer = original.Get();
    const Handle raw = original.GetRawHandle();

    // 동일 포인터, 동일 키
    auto same = manager.AddResource<Mesh>(
        L"Registration.Mesh", pointer);

    if (same.GetRawHandle() != raw || same.Get() != pointer)
        return false;

    // 동일 포인터, 다른 키
    auto alias = manager.AddResource<Mesh>(
        L"Registration.Alias", pointer);

    if (alias.IsValid() || !original.IsValid())
        return false;

    if (pointer->GetKey() != L"Registration.Mesh")
        return false;

    if (manager.FindHandleByKey(L"Registration.Alias").IsValid())
        return false;

    // 다른 포인터, 동일 타입, 동일 키
    auto duplicate = manager.AddResource<Mesh>(
        L"Registration.Mesh", Mesh::CreateEmpty());

    if (duplicate.GetRawHandle() != raw)
        return false;

    // 다른 타입, 동일 키
    auto conflict = manager.AddResource<Texture>(
        L"Registration.Mesh", Texture::CreateEmpty());

    if (conflict.IsValid() || !original.IsValid())
        return false;

    // PreparedResource에서도 동일 포인터가 유지되어야 한다.
    if (manager.RegisterPreparedResource(pointer) != raw)
        return false;

    // 포인터 인덱스 검증
    if (manager.FindHandleByResource(pointer) != raw)
        return false;

    if (manager.GetResourceHandles<Mesh>().size() != 1)
        return false;

    // 포인터 기반 삭제 이후 모든 인덱스 제거
    if (manager.DestroyResource(pointer) != EResult::Success)
        return false;

    return !original.IsValid() &&
        !manager.FindHandleByKey(L"Registration.Mesh").IsValid() &&
        manager.GetResourceHandles<Mesh>().empty();
}

static bool CheckResourceKeyRename()
{
    if (!ResourceManager::Create())
        return false;

    struct ResourceScope
    {
        ~ResourceScope() { ResourceManager::Destroy(); }
    } scope;

    auto& manager = ResourceManager::Get();

    auto mesh = manager.AddResource<Mesh>(
        L"Rename.Old", Mesh::CreateEmpty());

    auto other = manager.AddResource<Mesh>(
        L"Rename.Occupied", Mesh::CreateEmpty());

    if (!mesh.IsValid() || !other.IsValid())
        return false;

    const Handle raw = mesh.GetRawHandle();
    const auto* pointer = mesh.Get();
    const auto path = mesh->GetPath();

    // 직접 Key 변경은 거부
    if (mesh->SetKey(L"Rename.Direct"))
        return false;

    if (mesh->GetKey() != L"Rename.Old")
        return false;

    // 기존 키와의 충돌 방지
    if (manager.RenameResourceKey(raw, L"Rename.Occupied"))
        return false;

    if (manager.RenameResourceKey(raw, L""))
        return false;

    // 정상 Key 변경
    if (!manager.RenameResourceKey(raw, L"Rename.New"))
        return false;

    if (mesh.GetRawHandle() != raw ||
        mesh.Get() != pointer ||
        mesh->GetPath() != path)
    {
        return false;
    }

    if (mesh->GetKey() != L"Rename.New")
        return false;

    if (manager.FindHandleByKey(L"Rename.Old").IsValid())
        return false;

    if (manager.FindHandleByKey(L"Rename.New") != raw)
        return false;

    // 이전 키 재사용
    auto replacement = manager.AddResource<Mesh>(
        L"Rename.Old", Mesh::CreateEmpty());

    if (!replacement.IsValid() ||
        replacement.GetRawHandle() == raw)
    {
        return false;
    }

    // 이름이 변경된 리소스 삭제 후 인덱스 확인
    if (manager.DestroyResource(raw) != EResult::Success)
        return false;

    return !manager.FindHandleByKey(L"Rename.New").IsValid() &&
        replacement.IsValid();
}

static bool CheckShaderKeyFlags()
{
    if (!ResourceManager::Create())
        return false;

    struct ResourceScope
    {
        ~ResourceScope() { ResourceManager::Destroy(); }
    } scope;

    auto& manager = ResourceManager::Get();

    auto shader = manager.AddResource<Shader>(
        L"Test.GBufferPS", Shader::CreateEmpty());

    if (!shader.IsValid() || !shader->IsGBufferPS())
        return false;

    if (!manager.RenameResourceKey(
        shader.GetRawHandle(), L"Test.DefaultPS"))
    {
        return false;
    }

    if (shader->IsGBufferPS() || !shader->IsDefaultPS())
        return false;

    // 부모 타입으로 접근하더라도 직접 변경은 거부
    ResourceHandle<Resource> base = shader;

    if (base->SetKey(L"Test.Skinning"))
        return false;

    if (shader->IsSkinningShader())
        return false;

    if (!manager.RenameResourceKey(
        shader.GetRawHandle(), L"Test.Skinning"))
    {
        return false;
    }

    return shader->IsSkinningShader() &&
        shader->IsGBufferVS() &&
        !shader->IsDefaultPS();
}

static bool CheckResourceOwnership()
{
    if (!ResourceManager::Create())
        return false;

    struct ResourceScope
    {
        ~ResourceScope() { ResourceManager::Destroy(); }
    } scope;

    auto& manager = ResourceManager::Get();

    // 미등록 객체
    auto* temporary = Mesh::CreateEmpty();

    if (!temporary)
        return false;

    // 등록된 객체 전용 삭제 API로 제거하면 안 된다.
    if (manager.DestroyResource(temporary) == EResult::Success)
        return false;

    if (temporary->IsManagerOwned())
        return false;

    if (manager.DestroyUnregisteredResource(temporary) != EResult::Success)
        return false;

    // 등록된 객체
    auto registered = manager.AddResource<Mesh>(
        L"Ownership.Registered", Mesh::CreateEmpty());

    if (!registered.IsValid() || !registered->IsManagerOwned())
        return false;

    // 미등록 객체 삭제 API로 등록된 객체를 제거하면 안 된다.
    if (manager.DestroyUnregisteredResource(registered.Get()) == EResult::Success)
        return false;

    if (!registered.IsValid())
        return false;

    if (manager.DestroyResource(registered.GetRawHandle()) != EResult::Success)
        return false;

    return !registered.IsValid();
}

static bool CheckWorkerThreadRelease()
{
    if (!ResourceManager::Create())
        return false;

    struct ResourceScope
    {
        ~ResourceScope() { ResourceManager::Destroy(); }
    } scope;

    auto& manager = ResourceManager::Get();

    auto mesh = manager.AddResource<Mesh>(
        L"ThreadTest.Mesh", Mesh::CreateEmpty());

    if (!mesh.IsValid())
        return false;

    const Handle raw = mesh.GetRawHandle();

    if (!manager.ReleaseManagerReference(raw))
        return false;

    bool accessBlocked = false;
    bool referenceValid = false;
    bool destroyBlocked = false;

    std::thread worker(
        [owned = std::move(mesh), &manager, raw,
        &accessBlocked, &referenceValid, &destroyBlocked]() mutable
        {
            referenceValid = owned.IsValid();
            accessBlocked = owned.Get() == nullptr;

            destroyBlocked =
                manager.DestroyResource(raw) != EResult::Success;

            auto copied = owned;

            owned.Reset();
            copied.Reset();
        });

    worker.join();

    if (!referenceValid || !accessBlocked || !destroyBlocked)
        return false;

    // 마지막 참조 해제 직후 Handle은 무효여야 한다.
    if (manager.IsValid(raw))
        return false;

    if (manager.FindHandleByKey(L"ThreadTest.Mesh").IsValid())
        return false;

    // 실제 삭제는 Owner Thread에서 수행한다.
    if (manager.FlushDeferredDeletes() != 1)
        return false;

    // 중복 삭제 방지
    return manager.FlushDeferredDeletes() == 0;
}

static bool CheckDeferredDeletionSlotReuse()
{
    if (!ResourceManager::Create())
        return false;

    struct ResourceScope
    {
        ~ResourceScope() { ResourceManager::Destroy(); }
    } scope;

    auto& manager = ResourceManager::Get();

    auto oldMesh = manager.AddResource<Mesh>(
        L"Deferred.Mesh", Mesh::CreateEmpty());

    if (!oldMesh.IsValid())
        return false;

    const Handle oldRaw = oldMesh.GetRawHandle();

    if (!manager.ReleaseManagerReference(oldRaw))
        return false;

    std::thread worker([owned = std::move(oldMesh)]() mutable
        {
            owned.Reset();
        });

    worker.join();

    auto replacement = manager.AddResource<Mesh>(
        L"Deferred.Mesh", Mesh::CreateEmpty());

    if (!replacement.IsValid())
        return false;

    if (replacement.GetRawHandle() == oldRaw)
        return false;

    if (manager.FlushDeferredDeletes() != 1)
        return false;

    return replacement.IsValid() && !manager.IsValid(oldRaw);
}

static bool CheckResourceCacheCollection()
{
    if (!ResourceManager::Create())
        return false;

    struct ResourceScope
    {
        ~ResourceScope() { ResourceManager::Destroy(); }
    } scope;

    auto& manager = ResourceManager::Get();

    auto pinned = manager.AddResource<Mesh>(
        L"Cache.Pinned", Mesh::CreateEmpty());

    auto unused = manager.AddResource<Mesh>(
        L"Cache.Unused", Mesh::CreateEmpty());

    auto referenced = manager.AddResource<Mesh>(
        L"Cache.Referenced", Mesh::CreateEmpty());

    if (!pinned.IsValid() || !unused.IsValid() || !referenced.IsValid())
        return false;

    const Handle pinnedRaw = pinned.GetRawHandle();
    const Handle unusedRaw = unused.GetRawHandle();
    const Handle referencedRaw = referenced.GetRawHandle();

    if (!manager.SetCacheEvictable(unusedRaw, true) ||
        !manager.SetCacheEvictable(referencedRaw, true))
    {
        return false;
    }

    pinned.Reset();
    unused.Reset();

    // 유휴 시간이 부족하면 제거하지 않는다.
    if (manager.CollectUnusedResources(
        64, std::chrono::hours(1)) != 0)
    {
        return false;
    }

    // 테스트에서는 실제 대기 없이 유휴 시간 제한을 제거한다.
    if (manager.CollectUnusedResources(
        64, std::chrono::milliseconds(0)) != 1)
    {
        return false;
    }

    if (manager.IsValid(unusedRaw))
        return false;

    if (!manager.IsValid(pinnedRaw) || !referenced.IsValid())
        return false;

    referenced.Reset();

    if (manager.CollectUnusedResources(
        64, std::chrono::milliseconds(0)) != 1)
    {
        return false;
    }

    return !manager.IsValid(referencedRaw) &&
        manager.IsValid(pinnedRaw);
}

static bool CheckResourceDependencies()
{
    if (!ResourceManager::Create())
        return false;

    struct ResourceScope
    {
        ~ResourceScope() { ResourceManager::Destroy(); }
    } scope;

    auto& manager = ResourceManager::Get();

    auto texture = manager.AddResource<Texture>(
        L"Dependency.Texture", Texture::CreateEmpty());

    auto material = manager.AddResource<Material>(
        L"Dependency.Material", Material::CreateEmpty());

    if (!texture.IsValid() || !material.IsValid())
        return false;

    const Handle textureRaw = texture.GetRawHandle();
    const Handle materialRaw = material.GetRawHandle();

    material->SetTextureBinding("Albedo", 0, texture);

    const auto snapshot = manager.GetDependencySnapshot();

    const auto dependents = snapshot.FindDependents(textureRaw);

    if (std::find(dependents.begin(), dependents.end(), materialRaw) ==
        dependents.end())
    {
        return false;
    }

    if (snapshot.HasKnownCycle())
        return false;

    if (!manager.SetCacheEvictable(textureRaw, true) ||
        !manager.SetCacheEvictable(materialRaw, true))
    {
        return false;
    }

    texture.Reset();

    // Material 내부의 참조가 Texture 수명을 유지해야 한다.
    if (manager.CollectUnusedResources(
        64, std::chrono::milliseconds(0)) != 0)
    {
        return false;
    }

    if (!manager.IsValid(textureRaw))
        return false;

    material.Reset();

    // 첫 번째 정리에서 Material이 삭제된다.
    if (manager.CollectUnusedResources(
        64, std::chrono::milliseconds(0)) != 1)
    {
        return false;
    }

    if (manager.IsValid(materialRaw))
        return false;

    // 다음 정리에서 의존성이 해제된 Texture가 삭제된다.
    if (manager.CollectUnusedResources(
        64, std::chrono::milliseconds(0)) != 1)
    {
        return false;
    }

    return !manager.IsValid(textureRaw);
}

static bool CheckResourceDependencyCycles()
{
    const Handle a(0, 1);
    const Handle b(1, 1);
    const Handle c(2, 1);

    ResourceDependencySnapshot snapshot;

    snapshot.Nodes = {
        { a, L"A", true, { b } },
        { b, L"B", true, { c } },
        { c, L"C", true, {} }
    };

    if (snapshot.HasKnownCycle())
        return false;

    snapshot.Nodes[2].Dependencies.push_back(a);

    if (!snapshot.HasKnownCycle())
        return false;

    snapshot.Nodes[2].Dependencies.clear();

    if (snapshot.HasKnownCycle())
        return false;

    // 자기 자신을 참조하는 경우도 순환 참조다.
    snapshot.Nodes[2].Dependencies.push_back(c);

    return snapshot.HasKnownCycle();
}

int main()
{
    if (!CheckManagerRecreation())
    {
        std::fprintf(stderr,
            "[FAIL] ResourceManager recreation safety\n");

        return 1;
    }

    if (!CheckManagerShutdown())
    {
        std::fprintf(stderr,
            "[FAIL] ResourceManager shutdown safety\n");

        return 1;
    }

    if (!CheckManagerClearResources())
    {
        std::fprintf(stderr, "[FAIL] ResourceManager clear safety\n");
        return 1;
    }

    if (!CheckReferenceCounting())
    {
        std::fprintf(stderr, "[FAIL] Resource reference counting\n");
        return 1;
    }

    if (!CheckInvalidHandleCopy())
    {
        std::fprintf(stderr, "[FAIL] Invalid handle copy\n");
        return 1;
    }

    if (!CheckDuplicateRegistration())
    {
        std::fprintf(stderr, "[FAIL] Duplicate registration\n");
        return 1;
    }

    if (!CheckResourceKeyRename())
    {
        std::fprintf(stderr, "[FAIL] Resource key rename\n");
        return 1;
    }

    if (!CheckShaderKeyFlags())
    {
        std::fprintf(stderr, "[FAIL] Shader key flags\n");
        return 1;
    }

    if (!CheckResourceOwnership())
    {
        std::fprintf(stderr, "[FAIL] Resource ownership\n");
        return 1;
    }

    if (!CheckWorkerThreadRelease())
    {
        std::fprintf(stderr, "[FAIL] Worker thread release\n");
        return 1;
    }

    if (!CheckDeferredDeletionSlotReuse())
    {
        std::fprintf(stderr, "[FAIL] Deferred deletion slot reuse\n");
        return 1;
    }

    if (!CheckResourceCacheCollection())
    {
        std::fprintf(stderr, "[FAIL] Resource cache collection\n");
        return 1;
    }

    if (!CheckResourceDependencies())
    {
        std::fprintf(stderr, "[FAIL] Resource dependencies\n");
        return 1;
    }

    if (!CheckResourceDependencyCycles())
    {
        std::fprintf(stderr, "[FAIL] Resource dependency cycles\n");
        return 1;
    }

    if (!ResourceManager::Create())
        return 1;

    struct ResourceScope
    {
        ~ResourceScope()
        {
            ResourceManager::Destroy();
        }
    } scope;

    auto& manager = ResourceManager::Get();

    int failures = 0;

    const auto expect = [&](bool condition, const char* message)
        {
            if (!condition)
            {
                std::fprintf(stderr, "[FAIL] %s\n", message);
                ++failures;
            }
        };

    // 1. 정상적인 타입 조회
    auto mesh = manager.AddResource<Mesh>(
        L"Test.Mesh", Mesh::CreateEmpty());

    expect(mesh.IsValid(), "Mesh registration failed");

    auto sameMesh = manager.GetResourceHandle<Mesh>(L"Test.Mesh");

    expect(sameMesh.IsValid(), "Correct type lookup failed");

    // 2. 잘못된 타입 조회
    auto wrongTexture =
        manager.GetResourceHandle<Texture>(L"Test.Mesh");

    expect(!wrongTexture.IsValid(), "Wrong type lookup accepted");
    expect(wrongTexture.Get() == nullptr, "Wrong type returned pointer");
    expect(!wrongTexture.GetRawHandle().IsValid(), "Wrong type retained raw handle");

    // 3. Raw Handle을 통한 잘못된 타입 생성
    ResourceHandle<Texture> wrongRaw(mesh.GetRawHandle());

    expect(!wrongRaw.IsValid(), "Wrong raw handle type accepted");

    // 4. 부모 클래스 타입 조회
    auto material = manager.AddResource<Material>(
        L"Test.Material", Material::CreateEmpty());

    auto materialBase =
        manager.GetResourceHandle<MaterialInterface>(L"Test.Material");

    expect(materialBase.IsValid(), "Material base lookup failed");

    // 5. 다른 파생 클래스에 대한 조회
    auto instance = manager.AddResource<MaterialInstance>(
        L"Test.Instance", MaterialInstance::CreateEmpty());

    auto instanceBase =
        manager.GetResourceHandle<MaterialInterface>(L"Test.Instance");

    expect(instanceBase.IsValid(), "MaterialInstance base lookup failed");

    auto wrongMaterial =
        manager.GetResourceHandle<Material>(L"Test.Instance");

    expect(!wrongMaterial.IsValid(), "Wrong derived type accepted");

    // 6. 기본 핸들 검사
    ResourceHandle<Mesh> empty;

    expect(!empty.IsValid(), "Empty handle accepted");
    expect(empty.Get() == nullptr, "Empty handle returned pointer");

    // 7. 삭제된 핸들 검사
    const Handle staleHandle = mesh.GetRawHandle();

    expect(manager.DestroyResource(staleHandle) == EResult::Success,
        "Resource destruction failed");

    ResourceHandle<Mesh> stale(staleHandle);

    expect(!stale.IsValid(), "Stale handle accepted");
    expect(!mesh.IsValid(), "Destroyed resource remained valid");

    // 8. 슬롯 재사용 이후 Generation 확인
    auto newMesh = manager.AddResource<Mesh>(
        L"Test.NewMesh", Mesh::CreateEmpty());

    expect(newMesh.IsValid(), "Replacement resource failed");
    expect(!stale.IsValid(), "Stale handle became valid again");

    std::printf("ResourceHandleCheck: %s\n",
        failures == 0 ? "PASS" : "FAIL");

    return failures == 0 ? 0 : 1;
}