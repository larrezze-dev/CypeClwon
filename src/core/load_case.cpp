#include "core/load_case.h"

#include <algorithm>
#include <set>

namespace cc::core {

LoadCaseSet::LoadCaseSet() {
    LoadCase selfWeight;
    selfWeight.id = 0;
    selfWeight.nature = LoadNature::SelfWeight;
    selfWeight.reference = loadNatureName(LoadNature::SelfWeight);
    cases.push_back(std::move(selfWeight));
}

std::string loadNatureName(LoadNature nature) {
    switch (nature) {
        case LoadNature::SelfWeight: return "Peso propio";
        case LoadNature::DeadLoad: return "Cargas muertas";
        case LoadNature::LiveLoad: return "Sobrecarga de uso";
        case LoadNature::Temperature: return "Temperatura";
        case LoadNature::Shrinkage: return "Retracción";
        case LoadNature::Wind: return "Viento";
        case LoadNature::Seismic: return "Sismo";
        case LoadNature::Snow: return "Nieve";
        case LoadNature::EarthPressure: return "Empujes del terreno";
        case LoadNature::Accidental: return "Accidental";
    }
    return {};
}

std::string loadNaturePrefix(LoadNature nature) {
    switch (nature) {
        case LoadNature::SelfWeight: return {};
        case LoadNature::DeadLoad: return "CM";
        case LoadNature::LiveLoad: return "Q";
        case LoadNature::Temperature: return "T";
        case LoadNature::Shrinkage: return "R";
        case LoadNature::Wind: return "V";
        case LoadNature::Seismic: return "S";
        case LoadNature::Snow: return "N";
        case LoadNature::EarthPressure: return "H";
        case LoadNature::Accidental: return "A";
    }
    return {};
}

bool loadNatureAcceptsAdditional(LoadNature nature) { return nature != LoadNature::SelfWeight; }

std::string proposedLoadReference(const LoadCaseSet& set, LoadNature nature) {
    const int index = static_cast<int>(nature);
    const int counter = (index >= 0 && index < kLoadNatureCount) ? set.created[static_cast<std::size_t>(index)] : 0;
    return loadNaturePrefix(nature) + " " + std::to_string(counter + 1);
}

const LoadCase* findLoadCase(const LoadCaseSet& set, int id) {
    for (const LoadCase& c : set.cases)
        if (c.id == id) return &c;
    return nullptr;
}

std::vector<const LoadCase*> loadCasesOfNature(const LoadCaseSet& set, LoadNature nature) {
    std::vector<const LoadCase*> out;
    for (const LoadCase& c : set.cases)
        if (c.nature == nature && c.id != 0) out.push_back(&c);
    return out;
}

std::string loadCaseLabel(const LoadCase& c) { return c.reference; }

std::string nextUseCategoryRef(const LoadCaseSet& set) {
    auto used = [&](const std::string& ref) {
        return std::any_of(set.useCategories.begin(), set.useCategories.end(), [&](const UseCategory& u) { return u.ref == ref; });
    };
    for (char ch = 'A'; ch <= 'Z'; ++ch)
        if (const std::string ref(1, ch); !used(ref)) return ref;
    for (char first = 'A'; first <= 'Z'; ++first)
        for (char second = 'A'; second <= 'Z'; ++second)
            if (const std::string ref{first, second}; !used(ref)) return ref;
    return {};
}

std::string validateLoadCases(const LoadCaseSet& set) {
    if (set.cases.empty() || set.cases.front().id != 0 || set.cases.front().nature != LoadNature::SelfWeight)
        return "El primer caso debe ser el peso propio.";
    std::set<int> ids;
    std::array<int, kLoadNatureCount> count{};
    for (std::size_t i = 0; i < set.cases.size(); ++i) {
        const LoadCase& c = set.cases[i];
        if (!ids.insert(c.id).second) return "Identificador de hipótesis repetido.";
        if (i == 0) continue;
        if (c.id < 1 || c.id >= set.nextId) return "Identificador de hipótesis fuera de rango.";
        if (!loadNatureAcceptsAdditional(c.nature)) return "Naturaleza sin hipótesis adicionales.";
        if (c.reference.empty()) return "Hipótesis sin referencia.";
        if (c.nature == LoadNature::Seismic && c.hasArrangements()) return "Una hipótesis de sismo no tiene disposiciones.";
        ++count[static_cast<std::size_t>(c.nature)];
    }
    for (int n = 0; n < kLoadNatureCount; ++n)
        if (set.created[static_cast<std::size_t>(n)] < count[static_cast<std::size_t>(n)]) return "Contador de hipótesis inferior al número de hipótesis.";
    for (const UseCategory& u : set.useCategories)
        if (u.ref.empty()) return "Categoría de uso sin referencia.";
    return {};
}

}  // namespace cc::core
