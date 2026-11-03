#include "core/profile_library.h"

#include <memory>
#include <utility>

namespace cc::core {

ReplaceLibraryCommand::ReplaceLibraryCommand(Project& project, ProfileLibrary after, std::string text)
    : m_project(project), m_before(project.profileLibrary), m_after(std::move(after)), m_text(std::move(text)) {}

void ReplaceLibraryCommand::redo() { m_project.profileLibrary = m_after; }
void ReplaceLibraryCommand::undo() { m_project.profileLibrary = m_before; }

namespace {

const char* kUnknownSeries = "La serie de perfiles no existe en la biblioteca.";
const char* kUnknownProfile = "La referencia del perfil no existe.";  // #65417

/// Validates `edited` and pushes the replacement; empty string on success.
std::string commit(Project& project, UndoStack& stack, ProfileLibrary edited, const char* text) {
    const std::string problem = validateLibrary(edited);
    if (!problem.empty()) return problem;
    if (edited == project.profileLibrary) return "La operación no cambia la biblioteca de perfiles.";
    stack.push(std::make_unique<ReplaceLibraryCommand>(project, std::move(edited), text));
    return {};
}

/// Index of the profile in the series or -1.
int profileIndex(const ProfileSeries& s, const std::string& reference) {
    for (std::size_t i = 0; i < s.profiles.size(); ++i)
        if (s.profiles[i].reference == reference) return static_cast<int>(i);
    return -1;
}

template <class T>
bool moveInList(std::vector<T>& list, std::size_t index, int direction) {
    const long target = static_cast<long>(index) + (direction < 0 ? -1 : 1);
    if (direction == 0 || target < 0 || target >= static_cast<long>(list.size())) return false;
    std::swap(list[index], list[static_cast<std::size_t>(target)]);
    return true;
}

std::string suggestedProfileReference(const ProfileSeries& s, const std::string& base) {
    std::string name = base + " (copia)";
    for (int n = 2; profileIndex(s, name) >= 0; ++n) name = base + " (copia " + std::to_string(n) + ")";
    return name;
}

}  // namespace

std::string addSeries(Project& project, UndoStack& stack, ProfileSeries series) {
    ProfileLibrary edited = project.profileLibrary;
    edited.series.push_back(std::move(series));
    return commit(project, stack, std::move(edited), "Nueva serie de perfiles");
}

std::string editSeries(Project& project, UndoStack& stack, const std::string& reference, ProfileSeries series) {
    ProfileLibrary edited = project.profileLibrary;
    const int i = edited.indexOf(reference);
    if (i < 0) return kUnknownSeries;
    edited.series[static_cast<std::size_t>(i)] = std::move(series);
    return commit(project, stack, std::move(edited), "Editar serie de perfiles");
}

std::string duplicateSeries(Project& project, UndoStack& stack, const std::string& reference, std::string newReference) {
    ProfileLibrary edited = project.profileLibrary;
    const ProfileSeries* original = edited.find(reference);
    if (!original) return kUnknownSeries;
    ProfileSeries copy = *original;
    copy.reference = newReference.empty() ? edited.suggestedSeriesReference(reference) : std::move(newReference);
    copy.source.clear();
    edited.series.push_back(std::move(copy));
    return commit(project, stack, std::move(edited), "Duplicar serie de perfiles");
}

std::string removeSeries(Project& project, UndoStack& stack, const std::string& reference) {
    ProfileLibrary edited = project.profileLibrary;
    const int i = edited.indexOf(reference);
    if (i < 0) return kUnknownSeries;
    edited.series.erase(edited.series.begin() + i);
    return commit(project, stack, std::move(edited), "Borrar serie de perfiles");
}

std::string moveSeries(Project& project, UndoStack& stack, const std::string& reference, int direction) {
    ProfileLibrary edited = project.profileLibrary;
    const int i = edited.indexOf(reference);
    if (i < 0) return kUnknownSeries;
    if (!moveInList(edited.series, static_cast<std::size_t>(i), direction)) return "La serie ya está en el extremo de la lista.";
    return commit(project, stack, std::move(edited), direction < 0 ? "Subir serie de perfiles" : "Bajar serie de perfiles");
}

std::string addProfile(Project& project, UndoStack& stack, const std::string& seriesReference, Section profile, int position) {
    ProfileLibrary edited = project.profileLibrary;
    const int i = edited.indexOf(seriesReference);
    if (i < 0) return kUnknownSeries;
    auto& list = edited.series[static_cast<std::size_t>(i)].profiles;
    const std::size_t at = position < 0 || position > static_cast<int>(list.size()) ? list.size() : static_cast<std::size_t>(position);
    list.insert(list.begin() + static_cast<long>(at), std::move(profile));
    return commit(project, stack, std::move(edited), "Nuevo perfil");
}

std::string editProfile(Project& project, UndoStack& stack, const std::string& seriesReference, const std::string& profileReference,
                        Section profile) {
    ProfileLibrary edited = project.profileLibrary;
    const int i = edited.indexOf(seriesReference);
    if (i < 0) return kUnknownSeries;
    ProfileSeries& series = edited.series[static_cast<std::size_t>(i)];
    const int j = profileIndex(series, profileReference);
    if (j < 0) return kUnknownProfile;
    series.profiles[static_cast<std::size_t>(j)] = std::move(profile);
    return commit(project, stack, std::move(edited), "Editar perfil");
}

std::string duplicateProfile(Project& project, UndoStack& stack, const std::string& seriesReference,
                             const std::string& profileReference, std::string newReference) {
    ProfileLibrary edited = project.profileLibrary;
    const int i = edited.indexOf(seriesReference);
    if (i < 0) return kUnknownSeries;
    ProfileSeries& series = edited.series[static_cast<std::size_t>(i)];
    const int j = profileIndex(series, profileReference);
    if (j < 0) return kUnknownProfile;
    Section copy = series.profiles[static_cast<std::size_t>(j)];
    copy.reference = newReference.empty() ? suggestedProfileReference(series, profileReference) : std::move(newReference);
    series.profiles.insert(series.profiles.begin() + j + 1, std::move(copy));  // right after the original
    return commit(project, stack, std::move(edited), "Duplicar perfil");
}

std::string removeProfile(Project& project, UndoStack& stack, const std::string& seriesReference, const std::string& profileReference) {
    ProfileLibrary edited = project.profileLibrary;
    const int i = edited.indexOf(seriesReference);
    if (i < 0) return kUnknownSeries;
    ProfileSeries& series = edited.series[static_cast<std::size_t>(i)];
    const int j = profileIndex(series, profileReference);
    if (j < 0) return kUnknownProfile;
    series.profiles.erase(series.profiles.begin() + j);  // an empty series is refused by validateSeries (#131925)
    return commit(project, stack, std::move(edited), "Borrar perfil");
}

std::string moveProfile(Project& project, UndoStack& stack, const std::string& seriesReference, const std::string& profileReference,
                        int direction) {
    ProfileLibrary edited = project.profileLibrary;
    const int i = edited.indexOf(seriesReference);
    if (i < 0) return kUnknownSeries;
    ProfileSeries& series = edited.series[static_cast<std::size_t>(i)];
    const int j = profileIndex(series, profileReference);
    if (j < 0) return kUnknownProfile;
    if (!moveInList(series.profiles, static_cast<std::size_t>(j), direction)) return "El perfil ya está en el extremo de la lista.";
    return commit(project, stack, std::move(edited), direction < 0 ? "Subir perfil" : "Bajar perfil");
}

std::string importSeries(Project& project, UndoStack& stack, const std::vector<ProfileSeries>& series, ImportPolicy policy,
                         int* imported) {
    if (imported) *imported = 0;
    if (series.empty()) return "Debe seleccionar, al menos, una serie de perfiles.";  // #33818
    ProfileLibrary edited = project.profileLibrary;
    int count = 0;
    for (const ProfileSeries& incoming : series) {
        const std::string problem = validateSeries(incoming);
        if (!problem.empty()) return problem;
        const int at = edited.indexOf(incoming.reference);
        if (at < 0) {
            edited.series.push_back(incoming);
            ++count;
        } else if (policy == ImportPolicy::ReplaceExisting) {
            edited.series[static_cast<std::size_t>(at)] = incoming;
            ++count;
        }
    }
    if (count == 0) return "Todas las series ya existen en la biblioteca de perfiles de la obra.";
    const std::string result = commit(project, stack, std::move(edited), "Importar series de perfiles");
    if (result.empty() && imported) *imported = count;
    return result;
}

}  // namespace cc::core
