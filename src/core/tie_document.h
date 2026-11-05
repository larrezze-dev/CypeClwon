#pragma once
#include <string>
#include <vector>

namespace cc::core {

/// Static help document of the Tirante page of "Describir": «CÁLCULO DE TIRANTES EN CYPE 3D» (ed3dlib.dll, ed3ayuda.c FUN_180077490;
/// describir_barras.md section 19.4). The original generates it without reading the work. The texts are the Spanish strings #64092-
/// #64120 of the original; the order and the numbering of the lists follow the REA. The four figures of the original are bitmap
/// resources that were not extracted: FigurePlaceholder marks where they go (ticket TK-081).
enum class TieBlockKind { Title, Heading, Paragraph, ListItem, Caption, FigurePlaceholder };

struct TieBlock {
    TieBlockKind kind;
    int id;            ///< string id of the original (64121 for the title; 0 for a figure)
    std::string text;  ///< Spanish text with the %s of the original already filled (see tieDocument)
    int number = 0;    ///< ListItem: 1-based position in its list
};

/// Blocks in the order of the original. The %s arguments of #64096 (20 %) come from the REA; those of #64110 / #64113 / #64114 are
/// names of the tie bars of the original's figures (not extracted): the clone fills them with «T1» / «T2» / «ΔT» (PROVISIONAL).
const std::vector<TieBlock>& tieDocument();

}  // namespace cc::core
