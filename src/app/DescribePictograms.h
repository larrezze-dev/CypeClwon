#pragma once
#include <QPixmap>
#include <QString>

#include "core/bar_element_data.h"

namespace cc::app {

/// Pictograms of the Pilar and Viga pages of "Describir" (T-P5-07, TK-076). The original shows rendered 3D bitmaps (PE resources of
/// seccviga.dll / pilargeo.dll, not extracted); these are the clone's own vector drawings (SVG rendered with QSvgRenderer): grey
/// concrete, orange slab, red reinforcement dots, blue-grey steel, tan timber. Square tiles of `size` pixels on the #F7F7F7 tile colour.
QPixmap pillarMaterialPictogram(cc::core::PillarMaterial material, int size);
QPixmap pillarConcretePictogram(cc::core::PillarConcreteKind kind, int size);
QPixmap mixedPillarPictogram(cc::core::MixedPillarKind kind, int size);
QPixmap beamFamilyPictogram(cc::core::BeamFamily family, int size);
/// `type` indexes the "Tipo" icons of the family (cc::core::beamTypeCount).
QPixmap beamTypePictogram(cc::core::BeamFamily family, int type, int size);

/// The SVG sources (exposed for the tests: every pictogram must be a valid document).
QString pillarMaterialSvg(cc::core::PillarMaterial material);
QString pillarConcreteSvg(cc::core::PillarConcreteKind kind);
QString mixedPillarSvg(cc::core::MixedPillarKind kind);
QString beamFamilySvg(cc::core::BeamFamily family);
QString beamTypeSvg(cc::core::BeamFamily family, int type);

}  // namespace cc::app
