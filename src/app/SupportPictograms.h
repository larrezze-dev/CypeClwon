#pragma once
#include <QColor>
#include <QPixmap>
#include <QPointF>
#include <array>

#include "core/node_support.h"

class QPainter;

namespace cc::app {

/// Drawings of the node supports (T-P6-01): the pictograms of the buttons of "Vinculación exterior" / "Vinculación interior" and the
/// symbols of the 3D window. Sources: docs/research/ui/vinculaciones.md section 5 (captures orig_vinc_ext_dialogo_inicial.png,
/// orig_vinc_ext_simbolos_1/2.png, orig_vinc_int_simbolos_3d.png). The pictograms are our own drawings (the original uses bitmaps).

/// Screen directions (unit vectors, y down) of the global axes X, Y and Z at a node: the symbols follow the view.
using SymbolAxes = std::array<QPointF, 3>;

/// Button image of an external restraint type (the 20 buttons of the grid), `size` logical pixels.
QPixmap externalSupportPictogram(cc::core::ExternalSupportType type, int size, qreal devicePixelRatio = 1.0);
/// Button image of an internal link type (empotrado, parcial, articulado, genérico).
QPixmap internalLinkPictogram(cc::core::InternalLinkType type, int size, qreal devicePixelRatio = 1.0);

/// Width in screen pixels of the symbol of an external restraint: constant with the zoom (~90 px measured in the original).
inline constexpr double kSupportSymbolWidth = 90.0;

/// Draws the 3D symbol of an external restraint at `node` (screen position), hanging towards the bottom of the screen like the original:
/// nothing for a free node, a hatched line for the fixed support, a hatched triangle for the pinned one (and the generic one with its three
/// displacements blocked), guide lines with ellipses ("wheels") for the line and plane types. No text. Returns false when nothing is drawn.
bool drawExternalSupportSymbol(QPainter& painter, const cc::core::ExternalSupport& support, const QPointF& node, const SymbolAxes& axes,
                               const QColor& colour);
/// Draws the 3D symbol of an internal link: a circle for the pinned link, two concentric circles for the partial fixity, one circle for a
/// generic link that is not the default; nothing for the fixed (default) link. Returns false when nothing is drawn.
bool drawInternalLinkSymbol(QPainter& painter, const cc::core::InternalLink& link, const QPointF& node, const QColor& colour);
/// True for a generic link that differs from the default one (a single row with every element and no partial fixity).
bool genericLinkIsNonTrivial(const cc::core::InternalLink& link);

}  // namespace cc::app
