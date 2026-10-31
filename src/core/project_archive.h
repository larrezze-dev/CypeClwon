#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cc::core {

/// Compressed projects (.cyp) behind the "Comprimir" / "Descomprimir" buttons of "Gestión archivos". The original's
/// .cyp layout is not documented, so ours is a plain ZIP holding the project file (and, optionally, its backup
/// copies); only the user-visible behaviour (dialogs, extension, size message) was captured. Ticket TK-018.
struct ArchiveResult {
    bool ok = false;
    std::string error;                         ///< Spanish, ready to show to the user
    std::vector<std::filesystem::path> files;  ///< files stored (compress) or written (extract)
};

/// Backup copies of a project: "<stem>.bak" and "<name>.bak" in the same folder (provisional naming).
std::vector<std::filesystem::path> backupFilesOf(const std::filesystem::path& project);

/// Files that Comprimir stores: the project plus, when requested, its existing backups.
std::vector<std::filesystem::path> archiveMembers(const std::filesystem::path& project, bool includeBackups);

/// "Total bytes a enviar": sum of the sizes of archiveMembers().
std::uint64_t archiveSourceBytes(const std::filesystem::path& project, bool includeBackups);

ArchiveResult compressProject(const std::filesystem::path& project, const std::filesystem::path& archive, bool includeBackups);

/// Extracts every member of `archive` into `folder`. Members that would escape the folder are refused (zip-slip),
/// the archive is validated before anything is written, and existing files are kept unless `overwrite` is true.
ArchiveResult extractProject(const std::filesystem::path& archive, const std::filesystem::path& folder, bool overwrite = false);

}  // namespace cc::core
