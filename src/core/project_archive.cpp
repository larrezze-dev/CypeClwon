#include "core/project_archive.h"

#include <miniz.h>

#include <algorithm>
#include <cstring>
#include <fstream>
#include <memory>
#include <system_error>

namespace cc::core {

namespace fs = std::filesystem;

namespace {

constexpr std::uint32_t kContainerVersionCurrent = 3;  // UTF-16LE strings
constexpr std::uint32_t kContainerVersionLegacy = 1;   // ANSI strings
constexpr std::uint64_t kMaxContainerBytes = 1ull << 30;  // refuse "gzip bombs"
constexpr const char* kClonUserName = "CypeClwon";
constexpr std::uint32_t kClonUserNumber = 0;

using Bytes = std::vector<unsigned char>;

void putU32(Bytes& out, std::uint32_t v) {
    for (int i = 0; i < 4; ++i) out.push_back(static_cast<unsigned char>(v >> (8 * i)));
}
void putU64(Bytes& out, std::uint64_t v) {
    for (int i = 0; i < 8; ++i) out.push_back(static_cast<unsigned char>(v >> (8 * i)));
}
void putString(Bytes& out, const std::u16string& s) {
    putU32(out, static_cast<std::uint32_t>(s.size() * 2));
    for (char16_t c : s) {
        out.push_back(static_cast<unsigned char>(c & 0xff));
        out.push_back(static_cast<unsigned char>(c >> 8));
    }
}

/// Little-endian cursor over a decompressed container; every read is bounds-checked.
struct Reader {
    const unsigned char* data;
    std::size_t size;
    std::size_t pos = 0;
    bool u32(std::uint32_t& v) {
        if (size - pos < 4) return false;
        v = 0;
        for (int i = 0; i < 4; ++i) v |= static_cast<std::uint32_t>(data[pos + i]) << (8 * i);
        pos += 4;
        return true;
    }
    bool u64(std::uint64_t& v) {
        if (size - pos < 8) return false;
        v = 0;
        for (int i = 0; i < 8; ++i) v |= static_cast<std::uint64_t>(data[pos + i]) << (8 * i);
        pos += 8;
        return true;
    }
    /// Length-prefixed string; `wide` selects UTF-16LE (version 3) or ANSI (version 1, read as Latin-1).
    bool str(bool wide, std::u16string& out) {
        std::uint32_t bytes = 0;
        if (!u32(bytes) || bytes > size - pos) return false;
        out.clear();
        if (wide) {
            if (bytes % 2) return false;
            for (std::uint32_t i = 0; i < bytes; i += 2)
                out.push_back(static_cast<char16_t>(data[pos + i] | (data[pos + i + 1] << 8)));
        } else {
            for (std::uint32_t i = 0; i < bytes; ++i) out.push_back(static_cast<char16_t>(data[pos + i]));
        }
        pos += bytes;
        return true;
    }
};

/// Path as stored by the original: no drive letter ("\CYPE Ingenieros\Proyectos\CYPE 3D\nuevo2.ed3").
std::u16string pathWithoutDrive(const fs::path& p) {
    std::error_code ec;
    fs::path absolute = fs::absolute(p, ec);
    if (ec) absolute = p;
    fs::path stored = absolute.root_directory() / absolute.relative_path();
    stored.make_preferred();
    return stored.u16string();
}

/// Last component of a stored path (either separator); empty when it is not a plain file name.
std::u16string leafName(const std::u16string& stored) {
    const std::size_t cut = stored.find_last_of(u"\\/");
    std::u16string name = cut == std::u16string::npos ? stored : stored.substr(cut + 1);
    if (name.empty() || name == u"." || name == u".." || name.find(u':') != std::u16string::npos) return {};
    return name;
}

bool readFile(const fs::path& path, Bytes& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    if (size < 0) return false;
    in.seekg(0);
    const std::size_t old = out.size();
    out.resize(old + static_cast<std::size_t>(size));
    in.read(reinterpret_cast<char*>(out.data() + old), size);
    return static_cast<bool>(in) || size == 0;
}

/// gzip member: header (as written by zlib's "wb": no name, OS = 0x0b), raw deflate, CRC32 + ISIZE.
bool gzipEncode(const Bytes& raw, Bytes& out) {
    std::size_t deflatedSize = 0;
    std::unique_ptr<void, decltype(&mz_free)> deflated(
        tdefl_compress_mem_to_heap(raw.data(), raw.size(), &deflatedSize, TDEFL_DEFAULT_MAX_PROBES), &mz_free);
    if (!deflated) return false;
    const unsigned char header[10] = {0x1f, 0x8b, 0x08, 0x00, 0, 0, 0, 0, 0x00, 0x0b};
    out.assign(header, header + 10);
    const auto* body = static_cast<const unsigned char*>(deflated.get());
    out.insert(out.end(), body, body + deflatedSize);
    putU32(out, static_cast<std::uint32_t>(mz_crc32(MZ_CRC32_INIT, raw.data(), raw.size())));
    putU32(out, static_cast<std::uint32_t>(raw.size() & 0xffffffffu));
    return true;
}

bool gzipDecode(const Bytes& in, Bytes& raw) {
    if (in.size() < 18 || in[0] != 0x1f || in[1] != 0x8b || in[2] != 8) return false;
    const unsigned flags = in[3];
    std::size_t pos = 10;
    if (flags & 4) {  // FEXTRA
        if (in.size() < pos + 2) return false;
        pos += 2 + (in[pos] | (in[pos + 1] << 8));
    }
    for (unsigned bit : {8u, 16u}) {  // FNAME, FCOMMENT: zero-terminated
        if (!(flags & bit)) continue;
        while (pos < in.size() && in[pos] != 0) ++pos;
        ++pos;
    }
    if (flags & 2) pos += 2;  // FHCRC
    if (pos + 8 > in.size()) return false;
    const std::size_t trailer = in.size() - 8;
    const auto le32 = [&](std::size_t at) {
        return static_cast<std::uint32_t>(in[at]) | (static_cast<std::uint32_t>(in[at + 1]) << 8) |
               (static_cast<std::uint32_t>(in[at + 2]) << 16) | (static_cast<std::uint32_t>(in[at + 3]) << 24);
    };
    const std::uint32_t crc = le32(trailer), isize = le32(trailer + 4);
    if (isize > kMaxContainerBytes) return false;
    raw.assign(isize, 0);
    // Output buffer is exactly ISIZE long: a stream that inflates to more fails instead of growing.
    const std::size_t produced = tinfl_decompress_mem_to_mem(raw.data(), raw.size(), in.data() + pos, trailer - pos, 0);
    if (produced != raw.size()) return false;
    return static_cast<std::uint32_t>(mz_crc32(MZ_CRC32_INIT, raw.data(), raw.size())) == crc;
}

struct Entry {
    std::u16string name;  // leaf only
    std::size_t offset = 0;
    std::size_t size = 0;
};

/// Parses the container; fills `entries` (offsets into `raw`). Error text in Spanish.
bool parseContainer(const Bytes& raw, std::vector<Entry>& entries, std::string& error) {
    Reader r{raw.data(), raw.size()};
    std::uint32_t version = 0, one = 0, userNumber = 0;
    std::u16string userName;
    if (!r.u32(version) || (version != kContainerVersionCurrent && version != kContainerVersionLegacy)) {
        error = "Formato de obra comprimida no reconocido.";
        return false;
    }
    const bool wide = version == kContainerVersionCurrent;
    if (!r.u32(one) || !r.str(wide, userName) || !r.u32(userNumber)) {
        error = "Obra comprimida dañada.";
        return false;
    }
    while (r.pos < raw.size()) {
        std::uint64_t size = 0;
        std::u16string stored;
        if (!r.u64(size) || !r.str(wide, stored) || size > raw.size() - r.pos) {
            error = "Obra comprimida dañada.";
            return false;
        }
        Entry e;
        e.name = leafName(stored);
        if (e.name.empty()) {
            error = "La obra comprimida contiene rutas no permitidas.";
            return false;
        }
        e.offset = r.pos;
        e.size = static_cast<std::size_t>(size);
        r.pos += e.size;
        entries.push_back(std::move(e));
    }
    if (entries.empty()) {
        error = "La obra comprimida no contiene ficheros.";
        return false;
    }
    return true;
}

}  // namespace

fs::path backupPathOf(const fs::path& project) {
    fs::path backup = project;
    std::string ext = project.extension().string();
    if (ext.size() > 1) {
        ext.back() = '$';  // ".ed3" -> ".ed$": the original's convention (extension ending in '$')
        backup.replace_extension(ext);
    } else {
        backup += ".$";
    }
    return backup;
}

std::vector<fs::path> backupFilesOf(const fs::path& project) {
    std::vector<fs::path> found;
    std::error_code ec;
    const fs::path candidate = backupPathOf(project);
    if (fs::is_regular_file(candidate, ec)) found.push_back(candidate);
    return found;
}

bool createBackup(const fs::path& project) {
    std::error_code ec;
    if (!fs::is_regular_file(project, ec)) return true;  // nothing to preserve
    return fs::copy_file(project, backupPathOf(project), fs::copy_options::overwrite_existing, ec) && !ec;
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
    // Container (uncompressed): version 3, u32 1, user name, user number, then per file u64 size + path + bytes.
    // The original performs one more write after the user number whose content could not be recovered (docs/research/
    // ui/dialogs_gestion_archivos.md section 4); it is omitted here, as in the older embedded examples.
    Bytes raw;
    putU32(raw, kContainerVersionCurrent);
    putU32(raw, 1);
    putString(raw, std::u16string(kClonUserName, kClonUserName + std::strlen(kClonUserName)));
    putU32(raw, kClonUserNumber);
    for (const fs::path& member : archiveMembers(project, includeBackups)) {
        Bytes content;
        if (!readFile(member, content)) {
            result.error = "No se ha podido leer «" + member.filename().string() + "».";
            return result;
        }
        putU64(raw, content.size());
        putString(raw, pathWithoutDrive(member));
        raw.insert(raw.end(), content.begin(), content.end());
        result.files.push_back(member);
    }
    Bytes gz;
    if (!gzipEncode(raw, gz)) {
        result.files.clear();
        result.error = "No se ha podido comprimir la obra.";
        return result;
    }
    // Written to a temporary file first so a failure never destroys an existing archive.
    const fs::path temp = archive.string() + ".tmp";
    fs::remove(temp, ec);
    {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(gz.data()), static_cast<std::streamsize>(gz.size()));
        out.flush();
        if (!out) {
            out.close();
            fs::remove(temp, ec);
            result.files.clear();
            result.error = "No se puede escribir «" + archive.string() + "».";
            return result;
        }
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
    Bytes gz, raw;
    std::vector<Entry> entries;
    if (!readFile(archive, gz) || !gzipDecode(gz, raw)) {
        result.error = "El fichero «" + archive.filename().string() + "» no es una obra comprimida válida.";
        return result;
    }
    if (!parseContainer(raw, entries, result.error)) return result;
    // Validate everything before writing anything.
    std::error_code ec;
    std::vector<fs::path> targets;
    for (const Entry& e : entries) {
        targets.push_back(folder / fs::path(e.name));
        if (!overwrite && fs::exists(targets.back(), ec)) {
            result.error = "Ya existe «" + targets.back().filename().string() + "» en la carpeta de destino.";
            return result;
        }
    }
    fs::create_directories(folder, ec);
    for (std::size_t i = 0; i < entries.size(); ++i) {
        std::ofstream out(targets[i], std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(raw.data() + entries[i].offset), static_cast<std::streamsize>(entries[i].size));
        out.flush();
        if (!out) {
            result.error = "No se ha podido descomprimir la obra.";
            return result;
        }
        result.files.push_back(targets[i]);
    }
    result.ok = true;
    return result;
}

}  // namespace cc::core
