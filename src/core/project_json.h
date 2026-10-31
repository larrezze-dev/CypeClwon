#pragma once
#include <string>
#include <vector>

#include "core/project.h"

namespace cc::core {

/// JSON mapping of the project model (one section per Project member). nlohmann-json stays an implementation detail
/// of project_json.cpp: callers exchange JSON text.
///
/// Reading is tolerant: fields missing from a section keep their default value (so files written before a field
/// was added still open) and unknown fields are ignored. A wrong type or an unknown enumeration value is an error.
namespace project_json {

/// Section names in the order they are written: "standards", "profiles", ... "units", "nodes", ... "shells".
const std::vector<std::string>& sectionNames();

/// True when `name` is one of sectionNames().
bool isKnownSection(const std::string& name);

/// JSON text of one section of `project`. `name` must be a known section.
std::string dumpSection(const Project& project, const std::string& name);

/// Replaces one section of `project` with the content of `jsonText`.
/// Throws std::runtime_error with a Spanish message when the text is malformed or has wrong types.
void loadSection(Project& project, const std::string& name, const std::string& jsonText);

/// Empty when every section holds valid values, otherwise a Spanish message naming the first invalid section.
std::string validate(const Project& project);

/// Human name of a section for messages ("Normas", "Estados límite"...).
std::string sectionTitle(const std::string& name);

}  // namespace project_json
}  // namespace cc::core
