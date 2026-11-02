#pragma once
#include <array>
#include <optional>
#include <utility>
#include <vector>

#include "camera.h"

namespace cc::core {

/// Section planes and clipping volume of the 3D window (docs/research/ui/barra_vertical_vistas.md, sessions 21-22).
/// A section is a plane perpendicular to a global axis; the geometry on the side of the red arrow is removed.
/// The clipping volume is a box of six planes that keeps what lies inside. As in the original (panelarea.dll,
/// "angulo_giro_antihorario_rad_xy_global", "Xo/Yo/Zo_centro_volumen"), the whole set can be turned about the
/// vertical axis through one centre by dragging an arc, so the planes need not stay axis-aligned.

/// Half-space kept by the clipper: dot(normal, p) <= offset.
struct ClipHalfSpace {
    Vec3 normal{1, 0, 0};  ///< unit
    double offset = 0.0;
    bool contains(const Vec3& p, double tolerance = 1e-9) const;
    /// Plane perpendicular to `axis` (0 = X, 1 = Y, 2 = Z) of the local frame (turned `angleRad` about Z through `centre`) at
    /// coordinate `value`; keeps the lower side when `keepBelow`.
    static ClipHalfSpace local(int axis, double value, bool keepBelow, const Vec3& centre = {}, double angleRad = 0.0);
    /// Same without any turn.
    static ClipHalfSpace axisAligned(int axis, double value, bool keepBelow) { return local(axis, value, keepBelow); }
};

/// One section plane. `flipped` reverses the arrow (double click on the arrow in the original).
struct SectionPlane {
    bool active = false;
    double position = 0.0;  ///< coordinate along its axis in the local frame
    bool flipped = false;
    /// Direction of the arrow (the side that is removed): +1 along the axis, or -1 when flipped.
    int arrowSign() const { return flipped ? -1 : 1; }
};

/// Box of the clipping volume; each face can be moved on its own.
struct ClipVolume {
    bool active = false;
    Vec3 min{};
    Vec3 max{};
};

struct ClipSettings {
    std::array<SectionPlane, 3> sections{};
    ClipVolume volume{};
    Vec3 centre{};          ///< centre of the turn (the original stores the centre of the volume)
    double angleRad = 0.0;  ///< counter-clockwise turn about Z (global XY plane)

    bool anyActive() const;
    /// Half-spaces that the geometry must satisfy to remain visible.
    std::vector<ClipHalfSpace> halfSpaces() const;

    /// Local frame <-> world: the local frame is the world turned `angleRad` about Z through `centre`.
    Vec3 toWorld(const Vec3& local) const;
    Vec3 toLocal(const Vec3& world) const;
    /// Unit vector of local axis `axis` in world coordinates.
    Vec3 axisDirection(int axis) const;
    /// Adds `deltaRad` to the turn (kept in (-pi, pi]).
    void rotate(double deltaRad);

    /// `centreIfFirst` becomes the centre of the turn when nothing was active before.
    void setSectionActive(int axis, bool on, double defaultPosition, const Vec3& centreIfFirst = {});
    /// Reverses the direction of the removed side; false for an inactive section.
    bool flipSection(int axis);
    void setVolumeActive(bool on, const Vec3& min, const Vec3& max);
    /// Moves one face of the volume (`faceMax` = the face with the larger coordinate), never past the opposite face.
    void moveVolumeFace(int axis, bool faceMax, double value);
};

/// Portion of segment a-b inside every half-space, if any.
std::optional<std::pair<Vec3, Vec3>> clipSegment(const Vec3& a, const Vec3& b, const std::vector<ClipHalfSpace>& spaces);

/// Sutherland-Hodgman clip of a polygon; empty when nothing remains.
std::vector<Vec3> clipPolygon(const std::vector<Vec3>& polygon, const std::vector<ClipHalfSpace>& spaces);

/// Axis-aligned bounds of a set of points, with `margin` added on every side; a default 10 m cube around the origin
/// when the set is empty.
std::pair<Vec3, Vec3> envelope(const std::vector<Vec3>& points, double margin = 0.0);

}  // namespace cc::core
