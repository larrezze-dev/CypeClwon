#include "core/standards.h"

#include <algorithm>

namespace cc::core {

namespace {

using List = std::vector<std::string>;

// Appends "<base> (<scope>)" for each scope.
void expand(List& out, const std::string& base, const List& scopes) {
    for (const auto& scope : scopes) out.push_back(base + " (" + scope + ")");
}

List concrete() {
    List l = {"Código Estructural (España)", "EHE-08", "EHE-98-CTE", "EHE-98", "EH-91", "REBAP (Portugal)"};
    expand(l, "Eurocódigo 2", {"UE", "España", "Portugal", "Francia", "Bulgaria", "Rumanía", "Polonia", "Eslovenia", "Malasia", "Singapur"});
    for (const char* s : {"SABS 0100", "BAEL-91 (R-99)", "NTC 14-01-2008", "BS 8110-1:1997", "SP 63.13330.2012"}) l.push_back(s);
    for (const char* y : {"25", "19", "14", "11", "08", "99"}) l.push_back(std::string("ACI 318M-") + y + " (USA)");
    for (const char* s : {"NTC:2023", "NTCRC:2017", "NTCRC:2004", "NTCRC (México)"}) l.push_back(s);
    for (const char* y : {"2026", "2023", "2014", "2007", "2003"}) l.push_back(std::string("ABNT NBR 6118:") + y);
    for (const char* s : {"NB-1 (Brasil)", "CIRSOC 201-2005", "CIRSOC 201-1982", "NCh430.Of2008 D°60:2011", "ACI 318-99 (Chile)",
                          "NTE E.060:2009", "NSR-10", "NB 1225001:2020", "NB 1225001:2017", "CBH 87", "IS 456:2000"}) {
        l.push_back(s);
    }
    return l;
}

List coldFormed() {
    List l = {"CTE DB SE-A (España)", "EAE 2011", "EA-95 (MV110)", "MV110 (Portugal)"};
    expand(l, "Eurocódigos 3 y 4", {"UE", "España", "Portugal", "Francia", "Italia", "Bulgaria", "Eslovenia", "Rumanía", "Reino Unido", "Malasia", "Singapur"});
    for (const char* s : {"NTC 14-01-2008", "BS 5950-1:1998", "IS 801:1975", "DIN 18800:2008-11", "AISI S100-2016 (LRFD) (USA y México)",
                          "AISI S100-2007 (LRFD) (USA y México)", "AISI (USA)", "CAN/CSA S136-16", "CAN/CSA S136-07", "ABNT NBR 14762:2010",
                          "ABNT NBR 14762:2001", "AISI (Brasil)", "NCh427", "SANS 10162-2:2011", "AS 4600:2005", "NZS 4600:2005"}) {
        l.push_back(s);
    }
    return l;
}

List hotRolled() {
    List l = {"Código Estructural (España)", "EAE 2011", "CTE DB SE-A (España)", "EA-95 (MV103)", "REAE"};
    expand(l, "Eurocódigos 3 y 4", {"UE", "España", "Portugal", "Francia", "Italia", "Reino Unido", "Malasia", "Singapur", "Eslovenia", "Rumanía"});
    for (const char* s : {"NTC 14-01-2008", "BS 5950-1:2000", "DIN 18800:2008-11"}) l.push_back(s);
    for (const char* y : {"22", "16", "10", "05"}) l.push_back(std::string("ANSI/AISC 360-") + y + " (LRFD)");
    for (const char* s : {"AISC LRFD 86", "AISC ASD 89", "CAN/CSA S16-01", "NTC-DCEA 2020 (México)", "NTCRC Estruct.Metal.", "NBR8800",
                          "ABNT NBR 8800:2008", "ABNT NBR 8800:2024", "NCh427", "IS 800:2007", "SANS 10162-1:2011", "NTC-DCEA 2023",
                          "NTC-2018 (Italia)"}) {
        l.push_back(s);
    }
    return l;
}

List timber() {
    List l = {"CTE DB SE-M (España)", "NBR 7190"};
    expand(l, "Eurocódigo 5", {"UE", "Francia", "Bélgica"});
    l.push_back("ANSI/AWC NDS-2015 (ASD)");
    l.push_back("CIRSOC 601");
    return l;
}

}  // namespace

const std::vector<std::string>& availableStandards(StandardFamily family) {
    static const List kConcrete = concrete(), kCold = coldFormed(), kHot = hotRolled(), kTimber = timber(), kAlu = {"Eurocódigo 9"};
    switch (family) {
        case StandardFamily::Concrete: return kConcrete;
        case StandardFamily::ColdFormedSteel: return kCold;
        case StandardFamily::HotRolledSteel: return kHot;
        case StandardFamily::Timber: return kTimber;
        case StandardFamily::Aluminium: return kAlu;
    }
    return kAlu;
}

const std::string& StandardsSelection::get(StandardFamily family) const {
    switch (family) {
        case StandardFamily::Concrete: return concrete;
        case StandardFamily::ColdFormedSteel: return coldFormedSteel;
        case StandardFamily::HotRolledSteel: return hotRolledSteel;
        case StandardFamily::Timber: return timber;
        case StandardFamily::Aluminium: return aluminium;
    }
    return aluminium;
}

void StandardsSelection::set(StandardFamily family, std::string name) {
    switch (family) {
        case StandardFamily::Concrete: concrete = std::move(name); break;
        case StandardFamily::ColdFormedSteel: coldFormedSteel = std::move(name); break;
        case StandardFamily::HotRolledSteel: hotRolledSteel = std::move(name); break;
        case StandardFamily::Timber: timber = std::move(name); break;
        case StandardFamily::Aluminium: aluminium = std::move(name); break;
    }
}

bool StandardsSelection::isValid() const {
    for (auto f : {StandardFamily::Concrete, StandardFamily::ColdFormedSteel, StandardFamily::HotRolledSteel,
                   StandardFamily::Timber, StandardFamily::Aluminium}) {
        const auto& names = availableStandards(f);
        if (std::find(names.begin(), names.end(), get(f)) == names.end()) return false;
    }
    return true;
}

std::string shortStandardName(std::string s) {
    const auto open = s.rfind(" (");
    if (open != std::string::npos && !s.empty() && s.back() == ')') s.erase(open);
    return s;
}

std::string StandardsSelection::summary() const {
    const auto strip = [](const std::string& s) { return shortStandardName(s); };
    const std::vector<std::string> names = {strip(concrete), strip(coldFormedSteel), strip(hotRolledSteel), strip(timber),
                                            strip(aluminium)};
    std::string out;
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (i > 0) out += (i + 1 == names.size()) ? " y " : ", ";
        out += names[i];
    }
    return out;
}

}  // namespace cc::core
