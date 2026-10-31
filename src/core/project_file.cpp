#include "core/project_file.h"

#include <miniz.h>
#include <nlohmann/json.hpp>

#include <chrono>
#include <cstdio>
#include <format>
#include <fstream>
#include <memory>
#include <system_error>

#include "core/project_json.h"
#include "core/version.h"

namespace cc::core {

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace {

constexpr const char* kManifestEntry = "manifest.json";
constexpr const char* kSectionPrefix = "sections/";
constexpr const char* kSectionSuffix = ".json";
constexpr mz_uint kMaxEntries = 4096;
constexpr mz_uint64 kMaxEntryBytes = 512ull * 1024 * 1024;  // refuse "zip bombs"

std::string quoted(const fs::path& path) { return "«" + path.filename().string() + "»"; }

/// Read-only ZIP over a FILE*; closes everything on destruction.
class ZipReader {
public:
    explicit ZipReader(const fs::path& path) {
#ifdef _WIN32
        if (_wfopen_s(&m_file, path.c_str(), L"rb") != 0) m_file = nullptr;
#else
        m_file = std::fopen(path.c_str(), "rb");
#endif
        if (!m_file) {
            m_openError = true;
            return;
        }
        if (!mz_zip_reader_init_cfile(&m_zip, m_file, 0, 0)) {
            m_zipError = true;
            return;
        }
        m_ready = true;
    }
    ~ZipReader() {
        if (m_ready) mz_zip_reader_end(&m_zip);
        if (m_file) std::fclose(m_file);
    }
    ZipReader(const ZipReader&) = delete;
    ZipReader& operator=(const ZipReader&) = delete;

    bool ready() const { return m_ready; }
    bool cannotOpen() const { return m_openError; }
    mz_uint count() { return mz_zip_reader_get_num_files(&m_zip); }

    bool name(mz_uint index, std::string& out) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&m_zip, index, &st)) return false;
        if (mz_zip_reader_is_file_a_directory(&m_zip, index)) {
            out.clear();
            return true;
        }
        out = st.m_filename;
        return true;
    }

    /// False when the entry is missing, too big or fails its CRC.
    bool read(const std::string& entry, std::string& out, bool& missing) {
        missing = false;
        const int index = mz_zip_reader_locate_file(&m_zip, entry.c_str(), nullptr, 0);
        if (index < 0) {
            missing = true;
            return false;
        }
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&m_zip, static_cast<mz_uint>(index), &st) || st.m_uncomp_size > kMaxEntryBytes) return false;
        std::size_t size = 0;
        void* data = mz_zip_reader_extract_to_heap(&m_zip, static_cast<mz_uint>(index), &size, 0);
        if (!data) return false;
        out.assign(static_cast<const char*>(data), size);
        mz_free(data);
        return true;
    }

private:
    std::FILE* m_file = nullptr;
    mz_zip_archive m_zip{};
    bool m_ready = false, m_openError = false, m_zipError = false;
};

std::string notAProject(const fs::path& path) {
    return "El fichero " + quoted(path) + " no es una obra de CypeClwon válida o está dañado.";
}

/// Parses and checks the manifest text. Returns an empty string on success.
std::string parseManifest(const std::string& text, const fs::path& path, ProjectManifest& out) {
    try {
        const json j = json::parse(text);
        if (!j.is_object() || j.value("format", std::string()) != kProjectFormatName) return notAProject(path);
        out.formatVersion = j.at("formatVersion").get<int>();
        out.appVersion = j.value("appVersion", std::string());
        out.description = j.value("description", std::string());
        out.createdAt = j.value("createdAt", std::string());
        out.modifiedAt = j.value("modifiedAt", std::string());
    } catch (const std::exception&) {
        return notAProject(path);
    }
    if (out.formatVersion < 1) return notAProject(path);
    return {};
}

std::string manifestText(const ProjectManifest& m) {
    json j = {{"format", kProjectFormatName},   {"formatVersion", m.formatVersion}, {"appVersion", m.appVersion},
              {"description", m.description},   {"createdAt", m.createdAt},         {"modifiedAt", m.modifiedAt}};
    return j.dump(2);
}

/// The section name of a "sections/<name>.json" entry, empty when the entry is something else.
std::string sectionOfEntry(const std::string& entry) {
    const std::string prefix = kSectionPrefix, suffix = kSectionSuffix;
    if (entry.size() <= prefix.size() + suffix.size() || entry.compare(0, prefix.size(), prefix) != 0 ||
        entry.compare(entry.size() - suffix.size(), suffix.size(), suffix) != 0)
        return {};
    const std::string name = entry.substr(prefix.size(), entry.size() - prefix.size() - suffix.size());
    if (name.find_first_of("/\\") != std::string::npos) return {};
    return name;
}

}  // namespace

std::string currentTimestamp() {
    return std::format("{:%Y-%m-%dT%H:%M:%SZ}", std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now()));
}

ProjectDocument makeDocument(const Project& project, const std::string& description) {
    ProjectDocument doc;
    doc.project = project;
    doc.manifest.formatVersion = kProjectFormatVersion;
    doc.manifest.appVersion = std::string(versionString());
    doc.manifest.description = description;
    doc.manifest.createdAt = doc.manifest.modifiedAt = currentTimestamp();
    return doc;
}

SaveResult saveProjectFile(const fs::path& path, ProjectDocument& document) {
    SaveResult result;
    ProjectManifest manifest = document.manifest;
    manifest.formatVersion = kProjectFormatVersion;
    manifest.appVersion = std::string(versionString());
    manifest.modifiedAt = currentTimestamp();
    if (manifest.createdAt.empty()) manifest.createdAt = manifest.modifiedAt;

    const std::string invalid = project_json::validate(document.project);
    if (!invalid.empty()) {
        result.error = "No se puede guardar la obra: " + invalid;
        return result;
    }

    // Build the archive in memory: nothing touches the disk until it is complete.
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_heap(&zip, 0, 64 * 1024)) {
        result.error = "No se puede preparar el fichero de la obra.";
        return result;
    }
    bool good = true;
    const auto add = [&](const std::string& entry, const std::string& text) {
        good = good && mz_zip_writer_add_mem(&zip, entry.c_str(), text.data(), text.size(), MZ_DEFAULT_LEVEL) != 0;
    };
    try {
        add(kManifestEntry, manifestText(manifest));
        for (const std::string& name : project_json::sectionNames())
            add(kSectionPrefix + name + kSectionSuffix, project_json::dumpSection(document.project, name));
        for (const auto& [name, text] : document.unknownSections) {
            if (name.empty() || name.find_first_of("/\\") != std::string::npos || project_json::isKnownSection(name)) continue;
            add(kSectionPrefix + name + kSectionSuffix, text);
        }
    } catch (const std::exception& e) {
        mz_zip_writer_end(&zip);
        result.error = std::string("No se puede serializar la obra: ") + e.what();
        return result;
    }
    void* buffer = nullptr;
    std::size_t size = 0;
    good = good && mz_zip_writer_finalize_heap_archive(&zip, &buffer, &size) != 0;
    if (!good) {
        mz_zip_writer_end(&zip);
        result.error = "No se ha podido crear el contenido de la obra.";
        return result;
    }
    std::unique_ptr<void, decltype(&mz_free)> owned(buffer, &mz_free);
    mz_zip_writer_end(&zip);

    std::error_code ec;
    if (path.has_parent_path()) fs::create_directories(path.parent_path(), ec);
    const fs::path temp = path.string() + ".tmp";
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(static_cast<const char*>(buffer), static_cast<std::streamsize>(size));
        out.flush();
        if (!out) {
            out.close();
            fs::remove(temp, ec);
            result.error = "No se puede escribir en «" + path.string() + "». Compruebe que la carpeta existe y que hay espacio disponible.";
            return result;
        }
    }
    fs::rename(temp, path, ec);  // replaces an existing file
    if (ec) {
        fs::remove(temp, ec);
        result.error = "No se puede guardar «" + path.string() + "»: el fichero está en uso o es de solo lectura.";
        return result;
    }
    document.manifest = manifest;
    result.ok = true;
    return result;
}

ManifestResult readProjectManifest(const fs::path& path) {
    ManifestResult result;
    ZipReader zip(path);
    if (zip.cannotOpen()) {
        result.error = "No se puede abrir el fichero " + quoted(path) + ".";
        return result;
    }
    std::string text;
    bool missing = false;
    if (!zip.ready() || !zip.read(kManifestEntry, text, missing)) {
        result.error = notAProject(path);
        return result;
    }
    result.error = parseManifest(text, path, result.manifest);
    result.ok = result.error.empty();
    return result;
}

LoadResult loadProjectFile(const fs::path& path, const LoadOptions& options) {
    LoadResult result;
    ZipReader zip(path);
    if (zip.cannotOpen()) {
        result.error = "No se puede abrir el fichero " + quoted(path) + ".";
        return result;
    }
    std::string text;
    bool missing = false;
    if (!zip.ready() || !zip.read(kManifestEntry, text, missing)) {
        result.error = notAProject(path);
        return result;
    }
    ProjectDocument doc;
    result.error = parseManifest(text, path, doc.manifest);
    if (!result.error.empty()) return result;
    if (doc.manifest.formatVersion > options.currentVersion) {
        result.error = "La obra " + quoted(path) + " se guardó con una versión más reciente de la aplicación (formato " +
                       std::to_string(doc.manifest.formatVersion) + "; esta versión admite hasta el " +
                       std::to_string(options.currentVersion) + "). Actualice CypeClwon para abrirla.";
        return result;
    }

    if (zip.count() > kMaxEntries) {
        result.error = notAProject(path);
        return result;
    }
    SectionTexts sections;
    for (mz_uint i = 0; i < zip.count(); ++i) {
        std::string entry;
        if (!zip.name(i, entry)) {
            result.error = notAProject(path);
            return result;
        }
        const std::string name = sectionOfEntry(entry);
        if (name.empty()) continue;
        std::string content;
        if (!zip.read(entry, content, missing)) {
            result.error = "La sección «" + project_json::sectionTitle(name) + "» de la obra " + quoted(path) +
                           " está dañada (no se puede leer o su suma de comprobación no coincide).";
            return result;
        }
        sections[name] = std::move(content);
    }

    // Upgrade older files one version at a time.
    for (int version = doc.manifest.formatVersion; version < options.currentVersion; ++version) {
        const auto index = static_cast<std::size_t>(version - 1);
        if (index >= options.migrations.size() || !options.migrations[index]) {
            result.error = "La obra " + quoted(path) + " usa el formato " + std::to_string(version) +
                           ", que esta versión ya no sabe convertir.";
            return result;
        }
        try {
            options.migrations[index](sections);
        } catch (const std::exception& e) {
            result.error = "No se ha podido convertir la obra " + quoted(path) + " desde el formato " +
                           std::to_string(version) + ": " + e.what();
            return result;
        }
    }

    try {
        for (const auto& [name, content] : sections) {
            if (project_json::isKnownSection(name)) project_json::loadSection(doc.project, name, content);
            else doc.unknownSections[name] = content;
        }
    } catch (const std::exception& e) {
        result.error = e.what();
        return result;
    }
    const std::string invalid = project_json::validate(doc.project);
    if (!invalid.empty()) {
        result.error = "La obra " + quoted(path) + " no es válida. " + invalid;
        return result;
    }
    doc.manifest.formatVersion = options.currentVersion;
    result.document = std::move(doc);
    result.ok = true;
    return result;
}

}  // namespace cc::core
