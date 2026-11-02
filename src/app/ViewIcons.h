#pragma once
#include <QIcon>

#include "core/camera.h"

namespace cc::app {

/// Icons of the vertical view bar, drawn in code (original artwork, not copied from CYPE 3D).
enum class ViewBarIcon {
    Projection,
    ProjectionType,
    RotateAroundPoint,
    RotateAroundCamera,
    SectionX,
    SectionY,
    SectionZ,
    ClippingVolume,
    Unidentified1,
    Unidentified2
};

/// Icons of the zoom / framing group of the top bar.
enum class ZoomToolIcon { PreviousWindow, FullWindow, DoubleWindow, Redraw, ZoomMark, MoveImage, Orbit3D };

QIcon viewBarIcon(ViewBarIcon which, int size = 20);
QIcon zoomToolIcon(ZoomToolIcon which, int size = 20);

/// Axes triad of a palette cell: X red, Y green, Z blue, seen from the camera orientation `yawDeg` / `pitchDeg`.
/// `rolled180` turns the picture upside down (cells 7-9); `faded` draws it grey (unavailable cells).
QIcon triadIcon(double yawDeg, double pitchDeg, double rollDeg, bool faded, int size = 30);

}  // namespace cc::app
