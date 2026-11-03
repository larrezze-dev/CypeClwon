#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/section_properties.h"

namespace cc::core {

/// Profile library of a project ("Biblioteca de perfiles", T-P5-05, core only). Behaviour source:
/// docs/research/ui/perfiles_biblioteca.md. The library belongs to the project (like the options): an ordered list of
/// series, each with an ordered list of profiles of one single shape. Series and profiles are addressed by their
/// reference (unique, compared exactly: the original's comparison rule is not known, TK-053).

struct ProfileSeries {
    std::string reference;    ///< "IPE"
    std::string description;  ///< "Perfiles IPE (EN 10365)"
    SectionFamily family = SectionFamily::HotRolledSteel;
    ShapeKind kind = ShapeKind::IParallel;  ///< every profile of the series has this shape
    std::string source;       ///< where a predefined series comes from (empty for the user's own series)
    std::vector<Section> profiles;  ///< in the order chosen by the user ("subir" / "bajar")
    bool operator==(const ProfileSeries&) const = default;

    const Section* findProfile(const std::string& reference) const;
};

struct ProfileLibrary {
    std::vector<ProfileSeries> series;  ///< in the order chosen by the user
    bool operator==(const ProfileLibrary&) const = default;

    const ProfileSeries* find(const std::string& reference) const;
    /// Index of a series, or -1.
    int indexOf(const std::string& reference) const;
    /// First free name of the form "<base> (copia)", "<base> (copia 2)"... for a duplicate.
    std::string suggestedSeriesReference(const std::string& base) const;
};

// --- validation (messages are the original strings where the REA pass found them) --------------------------------------

/// Empty when the series is valid, otherwise the first problem: missing reference (#131965), empty series (#131925),
/// profile without reference (#131966), repeated profile (#131926), shape different from the series, invalid dimensions.
std::string validateSeries(const ProfileSeries& series);
/// Empty when every series is valid and no reference is repeated (#400894).
std::string validateLibrary(const ProfileLibrary& library);

// --- JSON of the project ------------------------------------------------------------------------------------------------------

std::string toJson(const ProfileLibrary& library);
/// nullopt when the text is not a valid library (bad JSON, unknown shape, invalid series).
std::optional<ProfileLibrary> profileLibraryFromJson(const std::string& jsonText);

// --- export / import of series (own JSON format; the original's file format is not known, TK-054) -------------------------

inline constexpr const char* kSeriesFileFormat = "cype-clone-profile-series";
inline constexpr int kSeriesFileVersion = 1;

/// Text of an export file holding the given series.
std::string exportSeriesToJson(const std::vector<ProfileSeries>& series);
struct SeriesImport {
    std::vector<ProfileSeries> series;
    std::string error;  ///< empty on success
};
SeriesImport importSeriesFromJson(const std::string& jsonText);
/// Writes `series` to `path`; returns an error message or empty. Refuses to overwrite unless `overwrite` (#138928).
std::string exportSeriesToFile(const std::string& path, const std::vector<ProfileSeries>& series, bool overwrite = false);
SeriesImport importSeriesFromFile(const std::string& path);

}  // namespace cc::core
