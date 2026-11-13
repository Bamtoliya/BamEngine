#pragma once

#include <filesystem>
#include <cstdint>
#include <future>
#include <optional>
#include <string>

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
            std::string Output;
            std::filesystem::path ExportedDirectory;
        };

        static Result Build(const std::filesystem::path& projectRoot, const std::optional<ClientPackageRequest>& package);

        Status m_Status = Status::Idle;
        bool m_IsExport = false;
        std::string m_Output;
        std::filesystem::path m_ExportedDirectory;
        std::future<Result> m_Future;
    };
}
