#pragma once
#include <filesystem>
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/project.h"

namespace cc::core {

/// Project file (.ed3) of CypeClwon: an open, documented container (our own format; the original's is not public).
///
///   obra.ed3  = ZIP archive
///     manifest.json              {"format":"cypeclwon.project","formatVersion":1,"appVersion":...,"description":...,
///                                 "createdAt":...,"modifiedAt":...}
///     sections/<name>.json       one JSON document per module (standards, profiles, ..., units; later nodes, bars...)
///
/// - The manifest can be read on its own (readProjectManifest) without decompressing the sections.
/// - formatVersion is bumped whenever the layout of a section changes incompatibly; older files are upgraded on load
///   by the migration chain, files from a newer version are refused with a clear message.
/// - Sections that this build does not know are preserved verbatim (ProjectDocument::unknownSections).
/// - Sections that are missing take their default values (e.g. a file without "units" opens in cm / cm / m).
/// - Writing is atomic: a temporary file in the same folder, then a rename over the target.
inline constexpr int kProjectFormatVersion = 1;
inline constexpr const char* kProjectFormatName = "cypeclwon.project";
inline constexpr const char* kProjectExtension = ".ed3";

struct SaveResult {
    bool ok = false;
    std::string error;  ///< Spanish, ready to show
};

struct LoadResult {
    bool ok = false;
    std::string error;  ///< Spanish, ready to show
    ProjectDocument document;
};

struct ManifestResult {
    bool ok = false;
    std::string error;
    ProjectManifest manifest;
};

/// Section texts by name, as stored in the file; what migrations transform.
using SectionTexts = std::map<std::string, std::string>;
/// Upgrades the sections of one format version to the next one.
using Migration = std::function<void(SectionTexts&)>;

struct LoadOptions {
    int currentVersion = kProjectFormatVersion;  ///< highest version understood (tests lower or raise it)
    /// migrations[i] upgrades format version i + 1 to i + 2. Empty for the current default table (version 1 is the
    /// first one); tests inject their own.
    std::vector<Migration> migrations;
};

/// Current UTC time as "2026-10-31T12:00:00Z".
std::string currentTimestamp();

/// A new document for `project`: created and modified now, formatVersion and appVersion filled in.
ProjectDocument makeDocument(const Project& project, const std::string& description);

/// Writes `document` to `path` atomically. Updates manifest.formatVersion, appVersion and modifiedAt (and
/// createdAt when empty) of `document` on success.
SaveResult saveProjectFile(const std::filesystem::path& path, ProjectDocument& document);

/// Reads and validates a whole project.
LoadResult loadProjectFile(const std::filesystem::path& path, const LoadOptions& options = {});

/// Reads only the manifest (cheap: "Gestión archivos" lists many files).
ManifestResult readProjectManifest(const std::filesystem::path& path);

}  // namespace cc::core
