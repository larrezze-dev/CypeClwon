#pragma once
#include <optional>
#include <string>
#include <vector>

#include "core/profile_series.h"
#include "core/project.h"
#include "core/undo_stack.h"

namespace cc::core {

/// Undoable editing of the profile library of a project (T-P5-05, core only); see profile_series.h for the model.

// --- undoable editing ---------------------------------------------------------------------------------------------------

/// Replaces the library of the project by `after` (kept exactly as given); undo restores the previous one. All the editing
/// functions below build the edited copy, validate it, and push one of these on the stack.
class ReplaceLibraryCommand : public UndoCommand {
public:
    ReplaceLibraryCommand(Project& project, ProfileLibrary after, std::string text);
    void redo() override;
    void undo() override;
    std::string text() const override { return m_text; }

private:
    Project& m_project;
    ProfileLibrary m_before, m_after;
    std::string m_text;
};

/// Every function returns an empty string on success, otherwise the Spanish message of the refusal; on refusal nothing is
/// pushed and the project is untouched.
std::string addSeries(Project& project, UndoStack& stack, ProfileSeries series);
/// Replaces the series called `reference` (position kept); the new reference may differ but must stay unique.
std::string editSeries(Project& project, UndoStack& stack, const std::string& reference, ProfileSeries edited);
/// Appends a copy of the series with `newReference` (empty = suggested one); the copy has no `source`.
std::string duplicateSeries(Project& project, UndoStack& stack, const std::string& reference, std::string newReference = {});
std::string removeSeries(Project& project, UndoStack& stack, const std::string& reference);
/// direction -1 = up, +1 = down; refused at the ends.
std::string moveSeries(Project& project, UndoStack& stack, const std::string& reference, int direction);

/// `position` < 0 appends; otherwise the profile is inserted before that index.
std::string addProfile(Project& project, UndoStack& stack, const std::string& seriesReference, Section profile, int position = -1);
std::string editProfile(Project& project, UndoStack& stack, const std::string& seriesReference, const std::string& profileReference,
                        Section edited);
std::string duplicateProfile(Project& project, UndoStack& stack, const std::string& seriesReference, const std::string& profileReference,
                             std::string newReference = {});
/// Refused when it would leave the series empty (#131925).
std::string removeProfile(Project& project, UndoStack& stack, const std::string& seriesReference, const std::string& profileReference);
std::string moveProfile(Project& project, UndoStack& stack, const std::string& seriesReference, const std::string& profileReference,
                        int direction);

/// What to do with an imported series whose reference already exists (original dialog #33822..#33824).
enum class ImportPolicy { KeepExisting, ReplaceExisting };
/// Imports series (predefined or read from a file) as ONE undoable step. Existing references are kept or replaced in place
/// according to `policy`; new ones are appended. Every series is validated first; nothing is imported on error.
std::string importSeries(Project& project, UndoStack& stack, const std::vector<ProfileSeries>& series, ImportPolicy policy,
                         int* imported = nullptr);

}  // namespace cc::core
