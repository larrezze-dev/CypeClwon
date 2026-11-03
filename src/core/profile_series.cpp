#include "core/profile_series.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>

namespace cc::core {

using json = nlohmann::json;

namespace {

std::string quoted(const std::string& pattern, const std::string& arg) {
    std::string out = pattern;
    const std::size_t at = out.find("%s");
    if (at != std::string::npos) out.replace(at, 2, arg);
    return out;
}

}  // namespace

const Section* ProfileSeries::findProfile(const std::string& ref) const {
    for (const Section& s : profiles)
        if (s.reference == ref) return &s;
    return nullptr;
}

const ProfileSeries* ProfileLibrary::find(const std::string& reference) const {
    const int i = indexOf(reference);
    return i < 0 ? nullptr : &series[static_cast<std::size_t>(i)];
}

int ProfileLibrary::indexOf(const std::string& reference) const {
    for (std::size_t i = 0; i < series.size(); ++i)
        if (series[i].reference == reference) return static_cast<int>(i);
    return -1;
}

std::string ProfileLibrary::suggestedSeriesReference(const std::string& base) const {
    std::string name = base + " (copia)";
    for (int n = 2; indexOf(name) >= 0; ++n) name = base + " (copia " + std::to_string(n) + ")";
    return name;
}

std::string validateSeries(const ProfileSeries& s) {
    if (s.reference.empty()) return "Debe introducir una referencia para la serie de perfiles.";  // #131965
    if (s.profiles.empty()) return "Debe introducir, al menos, un perfil en la serie.";            // #131925
    std::set<std::string> seen;
    for (const Section& p : s.profiles) {
        if (p.reference.empty()) return "Debe introducir una referencia para el perfil.";  // #131966
        if (!seen.insert(p.reference).second) return quoted("El perfil con referencia '%s' está repetido.", p.reference);  // #131926
        if (p.kind != s.kind)
            return "El perfil '" + p.reference + "' no tiene la forma de la serie (" + shapeNameEs(s.kind) + ").";
        const std::string problem = validateSection(p);
        if (!problem.empty()) return "Perfil '" + p.reference + "': " + problem;
    }
    return {};
}

std::string validateLibrary(const ProfileLibrary& library) {
    std::set<std::string> seen;
    for (const ProfileSeries& s : library.series) {
        if (!s.reference.empty() && !seen.insert(s.reference).second)
            return quoted("La serie de perfiles '%s' está repetida.", s.reference);  // #400894
        const std::string problem = validateSeries(s);
        if (!problem.empty()) return problem;
    }
    return {};
}

// --- JSON ----------------------------------------------------------------------------------------------------------------------

namespace {

json overridesToJson(const SectionOverrides& t) {
    json j = json::object();
    const auto put = [&j](const char* key, const std::optional<double>& v) {
        if (v) j[key] = *v;
    };
    put("area", t.area); put("iy", t.iy); put("iz", t.iz); put("it", t.it); put("iw", t.iw);
    put("wplY", t.wplY); put("wplZ", t.wplZ); put("avy", t.avy); put("avz", t.avz);
    return j;
}

SectionOverrides overridesFromJson(const json& j) {
    SectionOverrides t;
    const auto get = [&j](const char* key, std::optional<double>& v) {
        if (j.contains(key)) v = j.at(key).get<double>();
    };
    get("area", t.area); get("iy", t.iy); get("iz", t.iz); get("it", t.it); get("iw", t.iw);
    get("wplY", t.wplY); get("wplZ", t.wplZ); get("avy", t.avy); get("avz", t.avz);
    return t;
}

json seriesToJson(const ProfileSeries& s) {
    json profiles = json::array();
    for (const Section& p : s.profiles) {
        json item = {{"reference", p.reference}, {"dims", p.dims}};
        if (p.tabulated.any()) item["tabulated"] = overridesToJson(p.tabulated);
        profiles.push_back(item);
    }
    return json{{"reference", s.reference}, {"description", s.description}, {"family", familyKey(s.family)},
                {"kind", shapeKey(s.kind)}, {"source", s.source}, {"profiles", profiles}};
}

ProfileSeries seriesFromJson(const json& j) {
    ProfileSeries s;
    s.reference = j.at("reference").get<std::string>();
    s.description = j.value("description", std::string());
    s.source = j.value("source", std::string());
    const auto family = familyFromKey(j.at("family").get<std::string>());
    const auto kind = shapeFromKey(j.at("kind").get<std::string>());
    if (!family || !kind) throw std::invalid_argument("familia o forma desconocida");
    s.family = *family;
    s.kind = *kind;
    for (const json& item : j.at("profiles")) {
        Section p;
        p.reference = item.at("reference").get<std::string>();
        p.kind = s.kind;
        p.dims = item.at("dims").get<std::vector<double>>();
        if (item.contains("tabulated")) p.tabulated = overridesFromJson(item.at("tabulated"));
        s.profiles.push_back(std::move(p));
    }
    return s;
}

}  // namespace

std::string toJson(const ProfileLibrary& library) {
    json list = json::array();
    for (const ProfileSeries& s : library.series) list.push_back(seriesToJson(s));
    return json{{"series", list}}.dump(2);
}

std::optional<ProfileLibrary> profileLibraryFromJson(const std::string& text) {
    try {
        const json j = json::parse(text);
        ProfileLibrary library;
        for (const json& item : j.at("series")) library.series.push_back(seriesFromJson(item));
        if (!validateLibrary(library).empty()) return std::nullopt;
        return library;
    } catch (const std::exception&) {
        return std::nullopt;
    }
}

// --- export / import -------------------------------------------------------------------------------------------------------------

std::string exportSeriesToJson(const std::vector<ProfileSeries>& series) {
    json list = json::array();
    for (const ProfileSeries& s : series) list.push_back(seriesToJson(s));
    return json{{"format", kSeriesFileFormat}, {"version", kSeriesFileVersion}, {"series", list}}.dump(2);
}

SeriesImport importSeriesFromJson(const std::string& text) {
    SeriesImport result;
    try {
        const json j = json::parse(text);
        if (j.value("format", std::string()) != kSeriesFileFormat)
            throw std::invalid_argument("el fichero no es un fichero de series de perfiles");
        if (j.value("version", 0) > kSeriesFileVersion) throw std::invalid_argument("el fichero lo creó una versión más reciente");
        ProfileLibrary check;
        for (const json& item : j.at("series")) check.series.push_back(seriesFromJson(item));
        const std::string problem = validateLibrary(check);
        if (!problem.empty()) throw std::invalid_argument(problem);
        if (check.series.empty()) throw std::invalid_argument("no contiene ninguna serie");
        result.series = std::move(check.series);
    } catch (const std::exception& e) {
        result.error = std::string("No se puede importar el fichero de series de perfiles: ") + e.what();
    }
    return result;
}

std::string exportSeriesToFile(const std::string& path, const std::vector<ProfileSeries>& series, bool overwrite) {
    namespace fs = std::filesystem;
    if (series.empty()) return "Debe seleccionar, al menos, una serie de perfiles.";  // #33818
    std::error_code ec;
    if (!overwrite && fs::exists(fs::u8path(path), ec)) return "El fichero " + path + " ya existe.";
    std::ofstream out(fs::u8path(path), std::ios::binary | std::ios::trunc);
    if (!out) return "No se puede escribir el fichero " + path + ".";
    out << exportSeriesToJson(series);
    out.close();
    return out ? std::string() : "No se ha podido escribir el fichero " + path + ".";
}

SeriesImport importSeriesFromFile(const std::string& path) {
    std::ifstream in(std::filesystem::u8path(path), std::ios::binary);
    if (!in) {
        SeriesImport r;
        r.error = "El fichero a importar '" + path + "' no existe o no se puede leer.";  // #75062
        return r;
    }
    std::stringstream buffer;
    buffer << in.rdbuf();
    return importSeriesFromJson(buffer.str());
}

}  // namespace cc::core
