#pragma once

#include "Base.h"
#include "Handle.h"
#include "TextureMipCache.h"
#include "Types.h"

#include <filesystem>
#include <memory>
#include <string>

namespace Engine
{
    class MaterialInterface;
    class Mesh;
    class Texture;

    // 자리만 등록한 메시와 텍스처를 화면에서 필요한 만큼 작업 스레드에서 읽고, 한도를 넘으면 덜 필요한 것을 내린다.
    // 프레임마다 Update 다음에 요청을 모으고, 다음 Update가 그 요청으로 계획을 세운다. 메인 스레드 전용.
    class ENGINE_API ResourceStreamer final : public Base
    {
        DECLARE_SINGLETON(ResourceStreamer)
    private:
        ResourceStreamer();
        virtual ~ResourceStreamer();
        EResult Initialize(void* arg = nullptr);
    public:
        virtual void Free() override;

        static constexpr uint32 DefaultBudgetMB = 512;
        static constexpr uint32 MinBudgetMB = 16;
        static constexpr uint32 MaxBudgetMB = 65536;

        struct Stats
        {
            uint64 BudgetBytes = 0;
            uint64 UsedBytes = 0;
            uint32 MeshCount = 0;
            uint32 LoadedMeshCount = 0;
            uint32 TextureCount = 0;
            uint32 FullTextureCount = 0;    // 가장 자세한 밉까지 올린 텍스처
            uint32 InFlightCount = 0;
            uint32 UnmetCount = 0;          // 한도 때문에 원하는 만큼 올리지 못한 수
            uint64 LoadCount = 0;
            uint64 ReduceCount = 0;
            uint64 FailedCount = 0;
        };

        struct TextureInfo
        {
            TextureMipLayout Layout;
            uint32 FirstMip = 0;        // 지금 올라가 있는 밉. 단색이면 Layout.MipLevels
        };

        void SetBudgetMB(uint32 megabytes);
        [[nodiscard]] Stats GetStats() const;
        // 작업 스레드가 파일을 읽고 있는지. 모듈을 다시 불러오기 전에 확인한다.
        [[nodiscard]] bool IsBusy() const;
        // 맡은 리소스가 있는지. 없으면 요청을 계산하지 않아도 된다.
        [[nodiscard]] bool HasEntries() const;

        // 자리만 등록한 리소스를 맡는다. 내용을 바로 쓰는 쪽이 있으면(EnsureResident) 그 리소스는 관리에서 빠져 내리지 않는다.
        void AddMesh(const Handle& handle, const std::filesystem::path& path, uint64 estimatedBytes);
        void AddTexture(const Handle& handle, const std::filesystem::path& path, const TextureInfo& info);

        // 이번 프레임의 요청. 여러 번 부르면 더 자세하고 더 중요한 쪽을 쓴다.
        void RequestMesh(const Mesh& mesh, f32 priority);
        // pixelsPerUV는 UV 1만큼이 화면에서 차지하는 픽셀 수. 0이면 가장 자세한 밉을 요청한다.
        void RequestMaterial(const MaterialInterface& material, f32 pixelsPerUV, f32 priority);
        // 재질 바인딩에 쓴 텍스처. 같은 프레임에 요청이 없으면 얼마나 자세해야 하는지 모르므로 가장 자세한 밉을 쓴다.
        static void NoteBound(const Texture& texture);
        // 재질의 텍스처를 모두 바로 읽는다. 한 번만 그려 저장하는 썸네일처럼 기다릴 수 없는 곳에서 쓴다.
        static void EnsureMaterialTextures(const MaterialInterface& material);

        // 준비된 내용을 올리고 새 작업을 시작한다.
        void Update(f64 budgetMilliseconds = 3.0, uint64 maxUploadBytes = 16ull * 1024 * 1024);

    private:
        struct State;
        std::unique_ptr<State> m_State;
    };
}
