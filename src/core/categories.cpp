#include "core/categories.h"

#include <nlohmann/json.hpp>

namespace cc::core {

using json = nlohmann::json;

namespace {
const char* analysisName(AnalysisElements a) {
    switch (a) {
        case AnalysisElements::Buckling: return "Buckling";
        case AnalysisElements::Modal: return "Modal";
        default: return "StressDeformation";
    }
}
}  // namespace

std::string toJson(const CategorySettings& s) {
    json j;
    j["visible"] = s.visible;
    j["locked"] = s.locked;
    j["useLayerColours"] = s.useLayerColours;
    j["analysis"] = analysisName(s.analysis);
    return j.dump();
}

std::optional<CategorySettings> categorySettingsFromJson(const std::string& jsonText) {
    try {
        const json j = json::parse(jsonText);
        if (!j.is_object()) return std::nullopt;
        CategorySettings s;
        auto flags = [&](const char* key, std::array<bool, kCategoryCount>& out) {
            if (!j.contains(key)) return true;
            const json& a = j.at(key);
            if (!a.is_array() || a.size() != static_cast<std::size_t>(kCategoryCount)) return false;
            for (int i = 0; i < kCategoryCount; ++i) out[i] = a.at(i).get<bool>();
            return true;
        };
        if (!flags("visible", s.visible) || !flags("locked", s.locked)) return std::nullopt;
        if (j.contains("useLayerColours")) s.useLayerColours = j.at("useLayerColours").get<bool>();
        if (j.contains("analysis")) {
            const std::string name = j.at("analysis").get<std::string>();
            if (name == "StressDeformation") s.analysis = AnalysisElements::StressDeformation;
            else if (name == "Buckling") s.analysis = AnalysisElements::Buckling;
            else if (name == "Modal") s.analysis = AnalysisElements::Modal;
            else return std::nullopt;
        }
        return s;
    } catch (const json::exception&) {
        return std::nullopt;
    }
}

}  // namespace cc::core
