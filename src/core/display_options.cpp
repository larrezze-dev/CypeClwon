#include "core/display_options.h"

#include <nlohmann/json.hpp>

#include <cmath>

namespace cc::core {

using json = nlohmann::json;

bool isValid(const DisplayOptions& options) {
    return std::isfinite(options.nodeSizeCm) && options.nodeSizeCm > 0 && std::isfinite(options.textSizePercent) &&
           options.textSizePercent > 0;
}

std::string toJson(const DisplayOptions& o) {
    json j;
    j["showElementInfo"] = o.showElementInfo;
    j["showTexts"] = o.showTexts;
    j["showLocalAxes"] = o.showLocalAxes;
    j["fillShells"] = o.fillShells;
    j["fillLoads"] = o.fillLoads;
    j["nodeSizeCm"] = o.nodeSizeCm;
    j["textSizePercent"] = o.textSizePercent;
    j["tooltipPlacement"] = o.tooltipPlacement == TooltipPlacement::OnElement ? "OnElement" : "TopRightCorner";
    return j.dump();
}

std::optional<DisplayOptions> displayOptionsFromJson(const std::string& jsonText) {
    try {
        const json j = json::parse(jsonText);
        if (!j.is_object()) return std::nullopt;
        DisplayOptions o;
        auto boolean = [&](const char* key, bool& out) { if (j.contains(key)) out = j.at(key).get<bool>(); };
        auto number = [&](const char* key, double& out) { if (j.contains(key)) out = j.at(key).get<double>(); };
        boolean("showElementInfo", o.showElementInfo);
        boolean("showTexts", o.showTexts);
        boolean("showLocalAxes", o.showLocalAxes);
        boolean("fillShells", o.fillShells);
        boolean("fillLoads", o.fillLoads);
        number("nodeSizeCm", o.nodeSizeCm);
        number("textSizePercent", o.textSizePercent);
        if (j.contains("tooltipPlacement")) {
            const std::string name = j.at("tooltipPlacement").get<std::string>();
            if (name == "TopRightCorner") o.tooltipPlacement = TooltipPlacement::TopRightCorner;
            else if (name == "OnElement") o.tooltipPlacement = TooltipPlacement::OnElement;
            else return std::nullopt;
        }
        if (!isValid(o)) return std::nullopt;
        return o;
    } catch (const json::exception&) {
        return std::nullopt;
    }
}

}  // namespace cc::core
