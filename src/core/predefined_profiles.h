#pragma once
#include <vector>

#include "core/profile_series.h"

namespace cc::core {

/// Predefined series of the clone ("Importación de series de perfiles predefinidas", T-P5-05). Minimal set built from
/// public tables (NOT from the original's data, TK-003): the nominal dimensions come from the ArcelorMittal catalogue
/// "Sections and Merchant Bars" (IPE per EU 19-57, HE per EU 53-62 -taken over by EN 10365-, IPN per DIN 1025-1, UPN per
/// DIN 1026-1, equal angles per EN 10056-1); the properties are computed, never copied. Details and accuracy:
/// docs/research/ui/perfiles_datos.md. References: IPE, HEA, HEB, HEM, IPN, UPN, L.
const std::vector<ProfileSeries>& predefinedSeries();

}  // namespace cc::core
