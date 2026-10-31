#pragma once
#include <cstddef>
#include <string>

#include "core/combination_report.h"

namespace cc::core {

std::size_t utf8Length(const std::string& s);  ///< Code points in a UTF-8 string.

/// Text export of the report: 120-character pages, CRLF, ASCII tables, γ written "g" and ψ written "y"; the formulae
/// are images in the original and are left blank. Returns UTF-8 with CRLF line ends.
std::string exportText(const CombinationReport& report, int pageWidth = 120);
/// Same text as UTF-16 LE with byte-order mark, the encoding the original writes.
std::string exportTextUtf16(const CombinationReport& report, int pageWidth = 120);

std::string utf8ToUtf16Le(const std::string& utf8, bool withBom);

/// Rich formats; formulae are written in Unicode text instead of images.
std::string exportHtml(const CombinationReport& report);
std::string exportRtf(const CombinationReport& report);

}  // namespace cc::core
