#pragma once
#include <optional>

#include "camera.h"
#include "picking.h"
#include "selection.h"
#include "structure_model.h"

namespace cc::core {

/// Kinds of point the cursor can snap to. Names follow the "Capturas a plantillas" dialog of the original.
enum class SnapKind {
    None,       ///< no snap: free cursor
    Endpoint,   ///< "Extremo": a node
    Midpoint,   ///< "Punto medio": middle of a bar
    Nearest,    ///< "Más cercano": the point of a bar closest to the cursor
    Extension   ///< "Extensión": the point of a guide line (X, Y or Z axis line through a node) closest to the cursor
};

/// Object-snap settings, mirroring the "Capturas a plantillas" dialog of the original (docs/research/ui/viewport_3d.md).
/// Defaults are those of the original. Only some modes can be evaluated on a bar model: the others are stored so the
/// dialog can round-trip them (point, insertion point, centre, perpendicular, parallel, intersection, quadrant: TK-026).
struct SnapOptions {
    bool enabled = false;   ///< "Activar capturas (F3)"
    bool tracking = false;  ///< "Activar puntos de rastreo (F11)"
    bool point = false;
    bool endpoint = true;
    bool insertion = false;
    bool centre = false;
    bool perpendicular = false;
    bool parallel = false;
    bool midpoint = true;
    bool intersection = true;
    bool quadrant = false;
    bool nearest = true;
    bool extension = true;

    /// Whether `kind` is evaluated. Observed in the original: with the master switch off, endpoints, nearest points and
    /// guide-line extensions still snap but midpoints do not, so those three are an always-on baseline.
    bool active(SnapKind kind) const;
    void clearAll();  ///< "Quitar todas"
};

struct SnapParams {
    double nodeRadiusPx = 12.0;  ///< original: between 10 and 15 px
    double lineRadiusPx = 22.0;  ///< original: between 20 and 25 px
};

struct SnapResult {
    SnapKind kind = SnapKind::None;
    Vec3 point;                           ///< snapped world position (meaningless for None)
    std::optional<ElementRef> element;    ///< the node or bar snapped to
    std::optional<int> referenceNode;     ///< node the dimension label is measured from (Nearest and Extension)
    double referenceDistance = 0.0;       ///< metres from referenceNode to point
    double pixelDistance = 0.0;
    /// Axis (0 = X, 1 = Y, 2 = Z) of the guide line for Extension.
    int axis = -1;
    bool snapped() const { return kind != SnapKind::None; }
};

/// Snaps the cursor at widget pixel (x, y). Priority: endpoint (node radius), midpoint, then the closest of the
/// nearest-on-bar and guide-line candidates within the line radius.
SnapResult snapCursor(const StructuralModel& model, const ScreenMapper& map, double x, double y,
                      const SnapOptions& options, const SnapParams& params = {});

}  // namespace cc::core
