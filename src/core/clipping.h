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
/// Every plane has its own turn (panelarea.dll): X and Y planes turn about the vertical axis, the Z plane tilts about Y,
/// both through the point of the plane at the centre of the envelope. The turn is limited to +-80 degrees.
struct SectionPlane {
    static constexpr double kMaxAngleRad = 1.3962634015954636;  ///< 80 degrees
    bool active = false;
    double position = 0.0;  ///< coordinate along its (world) axis
    bool flipped = false;
    Vec3 centre{};          ///< centre of the envelope when the plane was shown; its `axis` coordinate is replaced by `position`
    double angleRad = 0.0;  ///< own turn, within +-kMaxAngleRad
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
    /// Global turn of the clipping VOLUME only (the sections have their own angle): centre of the turn (the original stores
    /// the centre of the volume) and counter-clockwise turn about Z.
    Vec3 centre{};
    double angleRad = 0.0;

    bool anyActive() const;
    /// Half-spaces that the geometry must satisfy to remain visible.
    std::vector<ClipHalfSpace> halfSpaces() const;

    /// Local frame <-> world of the VOLUME: the world turned `angleRad` about Z through `centre`.
    Vec3 toWorld(const Vec3& local) const;
    Vec3 toLocal(const Vec3& world) const;
    /// Unit vector of local axis `axis` of the volume in world coordinates.
    Vec3 axisDirection(int axis) const;
    /// Adds `deltaRad` to the global turn (kept in (-pi, pi]); only while the volume is active.
    void rotate(double deltaRad);

    /// Pivot of section `axis`: its centre with the `axis` coordinate replaced by the position.
    Vec3 sectionPoint(int axis) const;
    /// Unit normal of section `axis` after its own turn (X, Y: about Z; Z: about Y).
    Vec3 sectionNormal(int axis) const;
    /// Turns a point of the unturned plane about the pivot of the section by its angle.
    Vec3 sectionToWorld(int axis, const Vec3& unturned) const;
    /// Adds `deltaRad` to the angle of the section. The whole step is refused (false) when the section is inactive, the
    /// delta is not finite or the result leaves +-80 degrees.
    bool rotateSection(int axis, double deltaRad);

    /// `centre` is the point the section turns about (centre of the envelope, `axis` coordinate replaced by the position).
    void setSectionActive(int axis, bool on, double defaultPosition, const Vec3& centre = {});
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
