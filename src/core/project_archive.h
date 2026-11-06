#pragma once
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace cc::core {

/// Compressed projects (.cyp) behind the "Comprimir" / "Descomprimir" buttons of "Gestión archivos".
/// Layout (measured in the original with REA, docs/research/ui/dialogs_gestion_archivos.md section 4): a single gzip
/// stream whose uncompressed content is little-endian:
///   u32 container version (3 = UTF-16LE strings, 1 = ANSI strings, read only), u32 1,
///   string user name (u32 byte length + bytes), u32 user number,
///   then for each file: u64 size, string path without drive letter, the raw bytes.
/// The clone writes version 3 with the fixed user "CypeClwon" / 0. The one extra write of the original after the user
/// number is unresolved and omitted (TODO(medir)). Ticket TK-018.
struct ArchiveResult {
    bool ok = false;
    std::string error;                         ///< Spanish, ready to show to the user
    std::vector<std::filesystem::path> files;  ///< files stored (compress) or written (extract)
};

/// Backup copy of a project: the sibling whose extension ends in '$' ("obra.ed3" -> "obra.ed$"), the previous
/// version of the project, created on every save.
std::filesystem::path backupPathOf(const std::filesystem::path& project);

/// The backup of `project` when it exists (zero or one path).
std::vector<std::filesystem::path> backupFilesOf(const std::filesystem::path& project);

/// Copies the current `project` file over its backup (call before overwriting it). True when there was nothing to
/// preserve or the copy succeeded.
bool createBackup(const std::filesystem::path& project);

/// Files that Comprimir stores: the project plus, when requested, its existing backups.
std::vector<std::filesystem::path> archiveMembers(const std::filesystem::path& project, bool includeBackups);

/// "Total bytes a enviar": sum of the sizes of archiveMembers().
std::uint64_t archiveSourceBytes(const std::filesystem::path& project, bool includeBackups);

ArchiveResult compressProject(const std::filesystem::path& project, const std::filesystem::path& archive, bool includeBackups);

/// Extracts every member of `archive` into `folder` using only the file-name part of the stored paths (so a hostile
/// archive cannot escape the folder). The archive is validated before anything is written, and existing files are kept
/// unless `overwrite` is true.
ArchiveResult extractProject(const std::filesystem::path& archive, const std::filesystem::path& folder, bool overwrite = false);

}  // namespace cc::core
