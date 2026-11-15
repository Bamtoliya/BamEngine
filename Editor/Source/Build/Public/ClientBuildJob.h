#pragma once

#include <filesystem>
#include <cstdint>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <optional>
#include <string>
#include <string_view>

namespace Editor
{
    struct ClientPackageIdentity
    {
        std::uint32_t Volume = 0;
        std::uint64_t FileId = 0;
        bool operator==(const ClientPackageIdentity&) const = default;
    };

    struct ClientPackageDestination
    {
        std::filesystem::path Directory;
        std::optional<ClientPackageIdentity> ExistingPackage;
    };

    struct ClientPackageRequest
    {
        static constexpr auto DefaultDirectoryName = "BamEngine-Windows-x64";
        static constexpr auto DefaultProgramName = "Client";

        std::filesystem::path StartupScene;
        std::filesystem::path OutputParent;
        std::string DirectoryName = DefaultDirectoryName;
        std::string ProgramName = DefaultProgramName;
        ClientPackageDestination Destination;
    };

    ClientPackageDestination InspectClientPackageDestination(const std::filesystem::path& projectRoot,
        const ClientPackageRequest& request);

    class ClientBuildJob final
    {
    public:
        enum class Status { Idle, Running, Succeeded, Failed };
        using OutputCallback = std::function<void(std::string_view)>;

        ClientBuildJob() = default;
        ~ClientBuildJob();

        ClientBuildJob(const ClientBuildJob&) = delete;
        ClientBuildJob& operator=(const ClientBuildJob&) = delete;

        void Start(const std::filesystem::path& projectRoot, std::optional<ClientPackageRequest> package = std::nullopt);
        void SetExportPreparationError(std::string output);
        void Update();

        [[nodiscard]] Status GetStatus() const { return m_Status; }
        [[nodiscard]] const std::string& GetOutput() const { return m_Output; }
        [[nodiscard]] bool IsExport() const { return m_IsExport; }
        [[nodiscard]] const std::filesystem::path& GetExportedDirectory() const { return m_ExportedDirectory; }

    private:
        struct Result
        {
            bool Success = false;
            std::filesystem::path ExportedDirectory;
            std::string ErrorMessage;
        };

        static Result Build(const std::filesystem::path& projectRoot,
            const std::optional<ClientPackageRequest>& package, OutputCallback onOutput);
        void AppendOutput(std::string_view text);

        Status m_Status = Status::Idle;
        bool m_IsExport = false;
        std::string m_Output;
        std::filesystem::path m_ExportedDirectory;

        std::thread m_WorkerThread;
        std::atomic<bool> m_IsFinished{ false };
        mutable std::mutex m_ResultMutex;
        Result m_Result;

        mutable std::mutex m_OutputMutex;
        std::string m_LiveOutput;
    };
}