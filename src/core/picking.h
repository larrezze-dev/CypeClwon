#pragma once
#include <optional>
#include <vector>

#include "camera.h"
#include "selection.h"
#include "structure_model.h"

namespace cc::core {

/// Maps world points to widget pixels (x right, y down) for a camera and a viewport size.
struct ScreenMapper {
    const Camera* camera = nullptr;
    double width = 0.0;
    double height = 0.0;
    /// False when the point cannot be seen (behind a perspective camera) or the viewport is empty.
    bool toScreen(const Vec3& world, double& x, double& y) const;
    bool toScreen(const Point3& world, double& x, double& y) const { return toScreen(Vec3{world.x, world.y, world.z}, x, y); }
};

struct PickOptions {
    double nodeRadiusPx = 7.0;  ///< half of the on-screen node square
    double barTolerancePx = 5.0;
};

/// Direct click: the nearest node within the node radius, else the nearest bar within the tolerance, else the shell
/// under the cursor. Only kinds allowed by the filter are considered. Elements with a point behind the camera are skipped.
std::optional<ElementRef> pickAt(const StructuralModel& model, const ScreenMapper& map, const SelectionFilter& filter,
                                 double x, double y, const PickOptions& options = {});

/// Window (corner 2 to the right of corner 1): only what lies entirely inside the rectangle.
/// Crossing (corner 2 to the left): everything the rectangle touches.
enum class RectMode { Window, Crossing };
RectMode rectModeFor(double x1, double x2);

std::vector<ElementRef> pickRect(const StructuralModel& model, const ScreenMapper& map, const SelectionFilter& filter,
                                 double x1, double y1, double x2, double y2);

}  // namespace cc::core
