#pragma once
#include <string>
#include <vector>

#include "core/import_geometry.h"

namespace cc::core {

/// Parser of the text file of "Geometria > Importar > Fichero de texto" (T-P5-11, core only). Behaviour source:
/// docs/research/ui/importar.md section 1 ([REA] help text of ed3ayuda.c, [CAD] messages of imporstr.dll #106854-#106890,
/// #399535-#399541).
///
/// Format: sections introduced by a line with the keyword NUDOS / JOINTS, BARRAS / FRAMES, LAMINAS / SHELLS or PAÑOS / PANELS.
///   NUDOS:  `<n> X=<v> Y=<v> Z=<v>`      BARRAS: `<n> <start> <end>`      LAMINAS / PAÑOS: `<n> <node> <node> ...`
/// Coordinates are metres. [REA, importar.md 3.5] keywords are case-insensitive; everything from the first ':' of a line is a comment;
/// blank lines are ignored; fewer than 3 lines is "Fichero vacío"; the first sector must be NUDOS (and it cannot come back); after
/// it BARRAS / LAMINAS / PAÑOS follow in any order; without any of them "No se encontraron todos los sectores de datos".
/// PROVISIONAL (not read in the original): keywords are also accepted without accent (LÁMINAS, PANOS) and in Latin-1; the decimal
/// separator may be '.' or ','; a shell has at least 3 nodes. Every problem is collected with its line (the original stops at the
/// first; the first message is the same).
struct TextImportResult {
    ImportedGeometry geometry;
    std::vector<ImportMessage> errors;
    long long linesRead = 0;  ///< lines of the file, blank and comment ones included (#106876 "Leídas %lu líneas.")
    bool ok() const { return errors.empty(); }
};

TextImportResult parseGeometryText(const std::string& text);

/// Original string of the importer by id (es-ES, with %lu / %s already filled with the given numbers where the original has them).
/// Known ids: 106854-106857, 106859-106870, 106873, 106877, 106884-106890, 399536-399541. Empty for any other id.
std::string importTextMessage(int id, long long a = 0, long long b = 0);

/// "Total: N nudos, M barras" (#106879).
std::string importTotalsMessage(long long nodes, long long bars);

}  // namespace cc::core
