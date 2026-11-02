#pragma once
#include <array>

namespace cc::core {

struct Vec3 {
    double x = 0.0, y = 0.0, z = 0.0;
    bool operator==(const Vec3&) const = default;
};
Vec3 operator+(const Vec3& a, const Vec3& b);
Vec3 operator-(const Vec3& a, const Vec3& b);
Vec3 operator*(const Vec3& a, double s);
double dot(const Vec3& a, const Vec3& b);
Vec3 cross(const Vec3& a, const Vec3& b);
double length(const Vec3& a);
Vec3 normalised(const Vec3& a);  ///< the zero vector stays zero

/// 4x4 matrix, column-major (OpenGL layout): element (row r, column c) is m[c * 4 + r].
struct Mat4 {
    std::array<double, 16> m{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
    double& at(int row, int col) { return m[static_cast<std::size_t>(col * 4 + row)]; }
    double at(int row, int col) const { return m[static_cast<std::size_t>(col * 4 + row)]; }
};
Mat4 operator*(const Mat4& a, const Mat4& b);

struct Ray {
    Vec3 origin;
    Vec3 direction;  ///< unit length
};

enum class StandardView { Top, Bottom, Front, Back, Left, Right, Isometric };

/// One cell of the "Proyección" palette of the vertical view bar (docs/research/ui/barra_vertical_vistas.md).
struct ProjectionPreset {
    double yawDeg = 0.0;
    double pitchDeg = 0.0;
    double rollDeg = 0.0;   ///< rotation about the view axis (180 = upside down)
    bool available = true;  ///< false: the original's angles of this cell are not measured yet, see TK-030
};

/// The 22 cells of the palette in reading order (3 columns, the last one centred). Indices are 0-based.
inline constexpr int kProjectionPresetCount = 22;
const std::array<ProjectionPreset, kProjectionPresetCount>& projectionPresets();

/// Where the camera is: what "Ventana anterior" stores and restores (the projection type is not part of it).
struct CameraView {
    Vec3 target;
    double distance = 20.0;
    double yawDeg = 45.0;
    double pitchDeg = 30.0;
    double rollDeg = 0.0;
    bool operator==(const CameraView&) const = default;
};

/// Orbital camera of the 3D viewport. World axes as in the original: Z up, X and Y in the horizontal plane.
///
/// The camera looks at `target` from `distance` away. `yawDeg` is the azimuth about Z and `pitchDeg` the elevation:
///   direction target -> camera = (sin(yaw) cos(pitch), -cos(yaw) cos(pitch), sin(pitch))
/// so yaw 0 / pitch 0 is the front view (looking along +Y, X to the right, Z up) and yaw 45 / pitch 30 shows X pointing
/// down-right and Y up-right, like the axes triad of the original. Pitch is limited to [-90, 90]; the right vector
/// depends on yaw only, so the top and bottom views have a well-defined orientation and orbiting never flips.
///
/// Both projections show the same height at the target distance (2 * distance * tan(fov / 2)), so switching between
/// perspective and orthographic keeps the apparent size of the model.
///
/// `rollDeg` turns the image about the view axis (positive: anticlockwise on screen): right and up are the yaw/pitch
/// ones rotated about `forward`. Orbiting keeps it (measured in the original); the standard views reset it and `setView` restores it.
class Camera {
public:
    enum class Projection { Perspective, Orthographic };

    Vec3 target;
    double distance = 20.0;
    double yawDeg = 45.0;
    double pitchDeg = 30.0;
    double rollDeg = 0.0;
    double fovYDeg = 45.0;
    Projection projection = Projection::Perspective;

    static constexpr double kMinDistance = 1e-3;
    static constexpr double kMaxDistance = 1e6;

    Vec3 position() const;
    Vec3 forward() const;  ///< unit, from the camera towards the target
    Vec3 right() const;    ///< unit, screen x axis
    Vec3 up() const;       ///< unit, screen y axis

    /// Rotates about the target: dragging `dxDeg` to the right turns the model with the mouse (the camera yaws the
    /// opposite way), dragging `dyDeg` down raises the camera.
    void orbit(double dxDeg, double dyDeg);

    /// Moves the view so the scene follows a mouse drag of (dxPx, dyPx) screen pixels (y grows downwards).
    void pan(double dxPx, double dyPx, double viewportHeightPx);

    /// Dolly by `notches` wheel notches (positive = closer, 6.5 % per notch). With `cursorNdc` (x, y in [-1, 1]) the
    /// point of the target plane under the cursor stays under the cursor.
    void zoom(double notches);
    void zoomAt(double notches, double ndcX, double ndcY, double aspect);

    void setStandardView(StandardView view);
    void setProjection(Projection p) { projection = p; }
    /// Applies the palette cell `index` (0-based): changes yaw, pitch and roll. False (nothing changes) when the index is
    /// out of range or the cell is not available.
    bool setPreset(int index);

    CameraView view() const { return {target, distance, yawDeg, pitchDeg, rollDeg}; }
    void setView(const CameraView& v) { target = v.target; distance = v.distance; yawDeg = v.yawDeg; pitchDeg = v.pitchDeg; rollDeg = v.rollDeg; }

    /// Centres on the box and moves back until it fits the viewport. A degenerate box (a point) uses `distance` 1.
    void frameBox(const Vec3& min, const Vec3& max, double aspect);

    Mat4 viewMatrix() const;
    Mat4 projectionMatrix(double aspect) const;
    Mat4 viewProjection(double aspect) const { return projectionMatrix(aspect) * viewMatrix(); }
    double nearPlane() const;
    double farPlane() const;

    /// Normalised device coordinates of a world point (x, y in [-1, 1] inside the viewport, z the depth); false when
    /// the point is behind a perspective camera.
    bool project(const Vec3& world, double aspect, Vec3& ndc) const;
    /// Ray through the viewport position (ndcX, ndcY).
    Ray ray(double ndcX, double ndcY, double aspect) const;
    /// World point of the plane through the target (perpendicular to the view direction) seen at (ndcX, ndcY).
    Vec3 pointOnTargetPlane(double ndcX, double ndcY, double aspect) const;
    /// World size of one screen pixel at the target distance, for a viewport `viewportHeightPx` tall.
    double worldPerPixel(double viewportHeightPx) const;

private:
    double halfHeight() const;  ///< half of the visible height at the target distance
};

}  // namespace cc::core
