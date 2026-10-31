#include "core/project_archive.h"

#include <miniz.h>

#include <system_error>

namespace cc::core {

namespace fs = std::filesystem;

std::vector<fs::path> backupFilesOf(const fs::path& project) {
    std::vector<fs::path> found;
    const fs::path dir = project.parent_path();
    for (const fs::path& candidate : {dir / (project.stem().string() + ".bak"), dir / (project.filename().string() + ".bak")}) {
        std::error_code ec;
        if (fs::is_regular_file(candidate, ec)) found.push_back(candidate);
    }
    return found;
}

std::vector<fs::path> archiveMembers(const fs::path& project, bool includeBackups) {
    std::vector<fs::path> members{project};
    if (includeBackups) {
        for (const fs::path& b : backupFilesOf(project)) members.push_back(b);
    }
    return members;
}

std::uint64_t archiveSourceBytes(const fs::path& project, bool includeBackups) {
    std::uint64_t total = 0;
    for (const fs::path& m : archiveMembers(project, includeBackups)) {
        std::error_code ec;
        const auto size = fs::file_size(m, ec);
        if (!ec) total += size;
    }
    return total;
}

ArchiveResult compressProject(const fs::path& project, const fs::path& archive, bool includeBackups) {
    ArchiveResult result;
    std::error_code ec;
    if (!fs::is_regular_file(project, ec)) {
        result.error = "No se encuentra la obra «" + project.filename().string() + "».";
        return result;
    }
    // Written to a temporary file first so a failure never destroys an existing archive.
    const fs::path temp = archive.string() + ".tmp";
    fs::remove(temp, ec);
    mz_zip_archive zip{};
    if (!mz_zip_writer_init_file(&zip, temp.string().c_str(), 0)) {
        result.error = "No se puede crear el fichero comprimido.";
        return result;
    }
    bool good = true;
    for (const fs::path& member : archiveMembers(project, includeBackups)) {
        good = mz_zip_writer_add_file(&zip, member.filename().string().c_str(), member.string().c_str(), nullptr, 0,
                                      MZ_DEFAULT_LEVEL) != 0;
        if (!good) break;
        result.files.push_back(member);
    }
    good = good && mz_zip_writer_finalize_archive(&zip) != 0;
    mz_zip_writer_end(&zip);
    if (!good) {
        fs::remove(temp, ec);
        result.files.clear();
        result.error = "No se ha podido comprimir la obra.";
        return result;
    }
    fs::remove(archive, ec);
    fs::rename(temp, archive, ec);
    if (ec) {
        fs::remove(temp, ec);
        result.files.clear();
        result.error = "No se puede escribir «" + archive.string() + "».";
        return result;
    }
    result.ok = true;
    return result;
}

ArchiveResult extractProject(const fs::path& archive, const fs::path& folder, bool overwrite) {
    ArchiveResult result;
    mz_zip_archive zip{};
    if (!mz_zip_reader_init_file(&zip, archive.string().c_str(), 0)) {
        result.error = "El fichero «" + archive.filename().string() + "» no es una obra comprimida válida.";
        return result;
    }
    std::error_code ec;
    const mz_uint count = mz_zip_reader_get_num_files(&zip);
    // First pass: validate every member so a bad archive writes nothing.
    std::vector<fs::path> targets(count);
    for (mz_uint i = 0; i < count; ++i) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st)) {
            result.error = "Obra comprimida dañada.";
            mz_zip_reader_end(&zip);
            return result;
        }
        if (mz_zip_reader_is_file_a_directory(&zip, i)) continue;
        const fs::path name = fs::path(st.m_filename);
        if (name.is_absolute() || name.has_root_name() || name != name.filename()) {
            result.error = "La obra comprimida contiene rutas no permitidas.";
            mz_zip_reader_end(&zip);
            return result;
        }
        targets[i] = folder / name;
        if (!overwrite && fs::exists(targets[i], ec)) {
            result.error = "Ya existe «" + name.string() + "» en la carpeta de destino.";
            mz_zip_reader_end(&zip);
            return result;
        }
    }
    fs::create_directories(folder, ec);
    for (mz_uint i = 0; i < count; ++i) {
        if (targets[i].empty()) continue;
        if (!mz_zip_reader_extract_to_file(&zip, i, targets[i].string().c_str(), 0)) {
            result.error = "No se ha podido descomprimir la obra.";
            mz_zip_reader_end(&zip);
            return result;
        }
        result.files.push_back(targets[i]);
    }
    mz_zip_reader_end(&zip);
    result.ok = true;
    return result;
}

}  // namespace cc::core
