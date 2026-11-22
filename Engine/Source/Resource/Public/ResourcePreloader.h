#pragma once

#include "Engine_API.h"
#include "ResourceReference.h"
#include "Result.h"
#include "Types.h"

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <memory>
#include <span>

namespace Engine
{
    class Resource;

    // 메시와 텍스처를 작업 스레드에서 읽고 해석한 뒤 메인 스레드에서 GPU에 올려 등록한다.
    // 재질은 파일을 훑어 참조하는 텍스처만 미리 읽는다. 재질·모델 등 나머지 리소스는 호출자가 기존 방식으로 로드하며,
    // 미리 읽지 못한 리소스도 기존 방식에서 다시 읽고 오류를 알린다.
    // ResourceStreamer가 있으면 더 단순한 LOD가 함께 있는 메시와 재질이 쓰는 이미지 텍스처는 자리만 등록하고 내용은 스트리머에 맡긴다.
    // Start·Update·소멸자는 메인 스레드에서만 호출한다.
    class ENGINE_API ResourcePreloader final
    {
    public:
        struct Stats
        {
            uint32 WorkerCount = 0;
            size_t ScheduledCount = 0;
            size_t FinishedCount = 0;
            size_t PlaceholderCount = 0;    // 자리만 먼저 등록한 리소스
            size_t CachedPlaceholderCount = 0; // 단색이 아닌 임시 텍스처
            size_t MeshCount = 0;
            size_t TextureCount = 0;
            size_t UploadBytes = 0;
            f64 WorkerMilliseconds = 0.0;   // 작업 스레드 시간의 합
            f64 MainMilliseconds = 0.0;     // GPU 생성·업로드와 등록
            f64 MeshMainMilliseconds = 0.0;
            f64 TextureMainMilliseconds = 0.0;
            f64 SlowestWorkerMilliseconds = 0.0;
        };

        // 등록을 마친 리소스의 키와 메인 스레드 시간(ms)
        using LoadedCallback = std::function<void(const std::wstring& key, f64 milliseconds)>;

        // 한 번의 Update에서 GPU로 복사할 CPU 데이터의 상한. 한 프레임의 업로드 메모리가 커지는 것을 막는다.
        static constexpr size_t MaxUploadBytesPerUpdate = 64u * 1024u * 1024u;

        ResourcePreloader();
        ~ResourcePreloader();
        ResourcePreloader(const ResourcePreloader&) = delete;
        ResourcePreloader& operator=(const ResourcePreloader&) = delete;

        void Start(std::span<const ResourceReference> references);
        // 준비된 리소스를 기한까지 등록한다. 예약한 작업을 모두 마치면 true를 반환한다.
        bool Update(std::chrono::steady_clock::time_point deadline, const LoadedCallback& onLoaded = {});
        [[nodiscard]] Stats GetStats() const;

        // 모두 마칠 때까지 기다린다. 이미 로드한 리소스가 대부분이면 스레드를 만들지 않는다.
        static void LoadBlocking(std::span<const ResourceReference> references);
        // 자리만 등록한 리소스의 내용을 바로 읽는다. 메인 스레드 전용. Resource::EnsureResident가 쓴다.
        static EResult LoadDetail(Resource& resource);
        // 파일의 CPU 내용만 읽는다. GPU에 올리거나 등록하지 않으며 작업 스레드에서 호출해도 된다. 읽을 수 없으면 nullptr.
        static Resource* PrepareFile(const std::wstring& key, const std::filesystem::path& path);
        // 작업 스레드가 리플렉션 등록 정보를 읽고 있는지. 모듈을 다시 불러오기 전에 확인한다.
        static bool IsAnyActive();

    private:
        struct State;
        std::unique_ptr<State> m_State;
    };
}
