#include "core/panel_distribution.h"

#include <algorithm>
#include <cmath>

#include "core/geom3.h"

namespace cc::core {

namespace {

using geom::add;
using geom::dot;
using geom::length;
using geom::scale;
using geom::sub;

constexpr double kEps = 1e-9;
constexpr double kPrecision = 1e-4;     // [REA] i_PRECISION4: also the dedup of cut points and the null limit of q and of the segment loads
constexpr double kMinForce = 1e-6;      // [REA] minimum area of the force polygon

Point2 sub2(const Point2& a, const Point2& b) { return {a.x - b.x, a.y - b.y}; }
Point2 add2(const Point2& a, const Point2& b) { return {a.x + b.x, a.y + b.y}; }
Point2 mul2(const Point2& a, double s) { return {a.x * s, a.y * s}; }
double dot2(const Point2& a, const Point2& b) { return a.x * b.x + a.y * b.y; }
double cross2(const Point2& a, const Point2& b) { return a.x * b.y - a.y * b.x; }
double len2(const Point2& a) { return std::hypot(a.x, a.y); }

using Interval = std::pair<double, double>;

/// Parameters in [0, 1] of the part of the segment a-b that lies inside (or on the boundary of) the polygon.
std::vector<Interval> clipSegment(const Point2& a, const Point2& b, const std::vector<Point2>& poly) {
    std::vector<Interval> out;
    const Point2 r = sub2(b, a);
    const double rl = len2(r);
    if (rl < kEps || poly.size() < 3) return out;
    std::vector<double> params{0.0, 1.0};
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const Point2 &c = poly[i], &e = poly[(i + 1) % poly.size()];
        const Point2 s = sub2(e, c);
        const double sl = len2(s);
        if (sl < kEps) continue;
        const double denom = cross2(r, s);
        if (std::fabs(denom) < 1e-12 * rl * sl) {
            // Parallel: only a collinear edge contributes (its ends).
            if (std::fabs(cross2(sub2(c, a), r)) / rl > 1e-6) continue;
            for (const Point2& p : {c, e}) {
                const double t = dot2(sub2(p, a), r) / (rl * rl);
                if (t > 0.0 && t < 1.0) params.push_back(t);
            }
            continue;
        }
        const double t = cross2(sub2(c, a), s) / denom;
        const double u = cross2(sub2(c, a), r) / denom;
        const double eu = 1e-9 / sl;
        if (t > 0.0 && t < 1.0 && u > -eu && u < 1.0 + eu) params.push_back(t);
    }
    std::sort(params.begin(), params.end());
    for (std::size_t i = 0; i + 1 < params.size(); ++i) {
        if ((params[i + 1] - params[i]) * rl < kEps) continue;
        const Point2 mid = add2(a, mul2(r, 0.5 * (params[i] + params[i + 1])));
        if (!pointInPolygon2D(mid, poly, kPrecision)) continue;
        if (!out.empty() && params[i] - out.back().second < 1e-12) out.back().second = params[i + 1];
        else out.emplace_back(params[i], params[i + 1]);
    }
    return out;
}

struct LocalBar {
    int id = 0;
    Point2 a, b;
    double length = 0.0;
    std::vector<Interval> support;  ///< parameters of the part inside the panel
};

}  // namespace

std::vector<DistributionBar> barsOfPanel(const std::vector<Point3>& polygon, const std::vector<DistributionBar>& bars) {
    std::vector<DistributionBar> out;
    const auto f = panelFrame(polygon);
    if (!f) return out;
    const std::vector<Point2> poly = polygonLocal(*f, polygon);
    for (const DistributionBar& bar : bars) {
        if (std::fabs(f->heightOf(bar.a)) > kPanelPlaneToleranceM || std::fabs(f->heightOf(bar.b)) > kPanelPlaneToleranceM) continue;
        const Point2 a = f->toLocal(bar.a), b = f->toLocal(bar.b);
        const auto parts = clipSegment(a, b, poly);
        const double rl = len2(sub2(b, a));
        if (std::any_of(parts.begin(), parts.end(), [&](const Interval& i) { return (i.second - i.first) * rl > 1e-6; })) out.push_back(bar);
    }
    return out;
}

DistributionResult distributeSurfaceLoad(const DistributionInput& in) {
    DistributionResult result;
    const std::vector<Point3>& supportPoly = in.support.empty() ? in.polygon : in.support;
    const auto frame = panelFrame(supportPoly);
    if (!frame || in.polygon.size() < 3) return result;
    const std::vector<Point2> sup = polygonLocal(*frame, supportPoly);
    const std::vector<Point2> poly = polygonLocal(*frame, in.polygon);
    Point2 d{dot(in.direction, frame->u), dot(in.direction, frame->v)};
    if (len2(d) < 1e-9) return result;
    d = mul2(d, 1.0 / len2(d));
    const Point2 t{-d.y, d.x};
    result.valid = true;

    // Centroid of the loaded polygon (area centroid; the mean of the vertices when degenerate).
    Point2 c0{0, 0};
    {
        double a = 0.0, cx = 0.0, cy = 0.0;
        for (std::size_t i = 0; i < poly.size(); ++i) {
            const Point2 &p = poly[i], &q = poly[(i + 1) % poly.size()];
            const double c = cross2(p, q);
            a += c;
            cx += (p.x + q.x) * c;
            cy += (p.y + q.y) * c;
        }
        if (std::fabs(a) > 1e-12) c0 = {cx / (3.0 * a), cy / (3.0 * a)};
        else
            for (const Point2& p : poly) c0 = add2(c0, mul2(p, 1.0 / static_cast<double>(poly.size())));
    }
    const auto tOf = [&](const Point2& p) { return dot2(sub2(p, c0), t); };
    const auto sOf = [&](const Point2& p) { return dot2(sub2(p, c0), d); };

    // Bars in the plane of the panel.
    std::vector<LocalBar> bars;          // associated bars that can carry load (not parallel to d)
    std::vector<double> cutPoints;
    for (const Point2& p : poly) cutPoints.push_back(tOf(p));
    for (const DistributionBar& bar : in.bars) {
        if (std::fabs(frame->heightOf(bar.a)) > kPanelPlaneToleranceM || std::fabs(frame->heightOf(bar.b)) > kPanelPlaneToleranceM) continue;
        LocalBar lb;
        lb.id = bar.id;
        lb.a = frame->toLocal(bar.a);
        lb.b = frame->toLocal(bar.b);
        lb.length = length(sub(bar.b, bar.a));
        if (lb.length < 1e-6) continue;
        const Point2 r = sub2(lb.b, lb.a);
        lb.support = clipSegment(lb.a, lb.b, sup);
        const double rl = len2(r);
        if (std::none_of(lb.support.begin(), lb.support.end(), [&](const Interval& i) { return (i.second - i.first) * rl > 1e-6; })) continue;
        // (b) intersections of the bar with the loaded polygon; (c) ends of the bars that are not parallel to d.
        for (const Interval& i : clipSegment(lb.a, lb.b, poly)) {
            cutPoints.push_back(tOf(add2(lb.a, mul2(r, i.first))));
            cutPoints.push_back(tOf(add2(lb.a, mul2(r, i.second))));
        }
        const bool parallelToD = std::fabs(cross2(mul2(r, 1.0 / rl), d)) < 1e-6;
        if (parallelToD) continue;
        cutPoints.push_back(tOf(lb.a));
        cutPoints.push_back(tOf(lb.b));
        bars.push_back(std::move(lb));
    }
    std::sort(cutPoints.begin(), cutPoints.end());
    std::vector<double> cuts;
    for (double v : cutPoints)
        if (cuts.empty() || v - cuts.back() > kPrecision) cuts.push_back(v);

    double extent = 1.0;
    for (const Point2& p : sup) extent = std::max(extent, std::hypot(p.x - c0.x, p.y - c0.y));
    for (const Point2& p : poly) extent = std::max(extent, std::hypot(p.x - c0.x, p.y - c0.y));
    const double reach = 2.0 * extent + 1.0;

    std::vector<PanelBarLoad> loads;
    for (std::size_t k = 0; k + 1 < cuts.size(); ++k) {
        const double t1 = cuts[k], t2 = cuts[k + 1], width = t2 - t1, mid = 0.5 * (t1 + t2);
        const Point2 base = add2(c0, mul2(t, mid));
        const Point2 from = sub2(base, mul2(d, reach)), to = add2(base, mul2(d, reach));
        for (const Interval& iv : clipSegment(from, to, poly)) {
            const double sa = -reach + iv.first * 2.0 * reach, sb = -reach + iv.second * 2.0 * reach;
            const double segLength = sb - sa;
            if (segLength < kEps) continue;
            const auto world = [&](double s) { return frame->toWorld(add2(base, mul2(d, s))); };
            const double va = surfaceLoadValueAt(in.load, in.polygon, world(sa)), vb = surfaceLoadValueAt(in.load, in.polygon, world(sb));
            // Pieces of constant sign (a variable load of mixed sign is split where it crosses zero).
            struct Piece {
                double s1, s2, v1, v2;
            };
            std::vector<Piece> pieces;
            if (va * vb < 0.0) {
                const double sz = sa + segLength * va / (va - vb);
                pieces.push_back({sa, sz, va, 0.0});
                pieces.push_back({sz, sb, 0.0, vb});
            } else {
                pieces.push_back({sa, sb, va, vb});
            }
            for (const Piece& pc : pieces) {
                const double w1 = pc.v1 * width, w2 = pc.v2 * width;
                if (std::fabs(w1) <= kPrecision && std::fabs(w2) <= kPrecision) continue;
                const double pl = pc.s2 - pc.s1;
                const double area = 0.5 * (w1 + w2) * pl;
                if (std::fabs(area) <= kMinForce) continue;
                const double xc = (std::fabs(w1) + 2.0 * std::fabs(w2)) / (3.0 * (std::fabs(w1) + std::fabs(w2))) * pl;
                const double sP = pc.s1 + xc;
                const double sign = (w1 + w2) < 0.0 ? -1.0 : 1.0;

                // Nearest bar behind and ahead of the point of application along the centre line.
                struct Candidate {
                    const LocalBar* bar = nullptr;
                    double s = 0.0, u = 0.0;
                    Interval span;
                };
                std::optional<Candidate> behind, ahead;
                for (const LocalBar& bar : bars) {
                    const double ta = tOf(bar.a), tb = tOf(bar.b);
                    if (std::fabs(tb - ta) < kEps) continue;
                    const double u = (mid - ta) / (tb - ta);
                    const double eu = 1e-7 / bar.length;
                    const Interval* span = nullptr;
                    for (const Interval& i : bar.support)
                        if (u >= i.first - eu && u <= i.second + eu) {
                            span = &i;
                            break;
                        }
                    if (!span) continue;
                    const double s = sOf(add2(bar.a, mul2(sub2(bar.b, bar.a), u)));
                    Candidate c{&bar, s, u, *span};
                    if (s <= sP + kEps) {
                        if (!behind || s > behind->s) behind = c;
                    } else if (!ahead || s < ahead->s) {
                        ahead = c;
                    }
                }
                if (!behind && !ahead) {
                    result.unassignedPoints.push_back(world(sP));
                    continue;
                }
                const double dBehind = behind ? std::max(0.0, sP - behind->s) : 0.0;
                const double dAhead = ahead ? std::max(0.0, ahead->s - sP) : 0.0;
                const auto fractionOf = [&](bool isBehind) {
                    if (!behind || !ahead) return 1.0;
                    const double sum = dBehind + dAhead;
                    if (sum < 1e-12) return 0.5;
                    return (isBehind ? dAhead : dBehind) / sum;
                };
                for (const bool isBehind : {true, false}) {
                    const std::optional<Candidate>& cand = isBehind ? behind : ahead;
                    if (!cand) continue;
                    const LocalBar& bar = *cand->bar;
                    const double ta = tOf(bar.a), tb = tOf(bar.b);
                    const double u1 = (t1 - ta) / (tb - ta), u2 = (t2 - ta) / (tb - ta);
                    const double lo = std::max(std::min(u1, u2), cand->span.first), hi = std::min(std::max(u1, u2), cand->span.second);
                    const double s1 = lo * bar.length, s2 = hi * bar.length;
                    if (s2 <= s1) continue;
                    const double q = std::fabs(area) * fractionOf(isBehind) / (s2 - s1);
                    if (q <= kPrecision) continue;
                    PanelBarLoad out;
                    out.panelId = in.panelId;
                    out.surfaceLoadId = in.surfaceLoadId;
                    out.barId = bar.id;
                    out.loadCaseId = in.load.loadCaseId;
                    out.s1 = s1;
                    out.s2 = s2;
                    out.q = q;
                    out.direction = scale(geom::unit(in.positiveDirection), sign);
                    loads.push_back(out);
                }
            }
        }
    }
    if (in.compact) {
        std::sort(loads.begin(), loads.end(), [](const PanelBarLoad& x, const PanelBarLoad& y) {
            if (x.barId != y.barId) return x.barId < y.barId;
            return x.s1 < y.s1;
        });
        std::vector<PanelBarLoad> merged;
        for (const PanelBarLoad& l : loads) {
            if (!merged.empty()) {
                PanelBarLoad& last = merged.back();
                if (last.barId == l.barId && std::fabs(last.s2 - l.s1) <= kPrecision && std::fabs(last.q - l.q) <= kPrecision &&
                    length(sub(last.direction, l.direction)) < 1e-9) {
                    last.s2 = std::max(last.s2, l.s2);
                    continue;
                }
            }
            merged.push_back(l);
        }
        loads = std::move(merged);
    }
    result.loads = std::move(loads);
    return result;
}

// --- project level ---

namespace {

bool insideStrict(const Point2& p, const std::vector<Point2>& poly) {
    if (!pointInPolygon2D(p, poly, 0.0)) return false;
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const Point2 &a = poly[i], &b = poly[(i + 1) % poly.size()];
        const Point2 ab = sub2(b, a);
        const double l2 = dot2(ab, ab);
        const double t = l2 < 1e-24 ? 0.0 : std::clamp(dot2(sub2(p, a), ab) / l2, 0.0, 1.0);
        if (len2(sub2(p, add2(a, mul2(ab, t)))) <= 1e-6) return false;
    }
    return true;
}

bool edgesCrossProperly(const std::vector<Point2>& a, const std::vector<Point2>& b) {
    for (std::size_t i = 0; i < a.size(); ++i)
        for (std::size_t j = 0; j < b.size(); ++j) {
            const Point2 &p = a[i], &q = a[(i + 1) % a.size()], &r = b[j], &s = b[(j + 1) % b.size()];
            const double d1 = cross2(sub2(q, p), sub2(r, p)), d2 = cross2(sub2(q, p), sub2(s, p));
            const double d3 = cross2(sub2(s, r), sub2(p, r)), d4 = cross2(sub2(s, r), sub2(q, r));
            const double tol = 1e-9;
            if (((d1 > tol && d2 < -tol) || (d1 < -tol && d2 > tol)) && ((d3 > tol && d4 < -tol) || (d3 < -tol && d4 > tol))) return true;
        }
    return false;
}

Point2 centroid2(const std::vector<Point2>& poly) {
    double a = 0.0, cx = 0.0, cy = 0.0;
    for (std::size_t i = 0; i < poly.size(); ++i) {
        const Point2 &p = poly[i], &q = poly[(i + 1) % poly.size()];
        const double c = cross2(p, q);
        a += c;
        cx += (p.x + q.x) * c;
        cy += (p.y + q.y) * c;
    }
    if (std::fabs(a) < 1e-12) return poly.empty() ? Point2{} : poly.front();
    return {cx / (3.0 * a), cy / (3.0 * a)};
}

}  // namespace

bool panelsOverlap(const std::vector<Point3>& a, const std::vector<Point3>& b) {
    const auto fa = panelFrame(a);
    if (!fa || !panelFrame(b)) return false;
    for (const Point3& p : b)
        if (std::fabs(fa->heightOf(p)) > kPanelPlaneToleranceM) return false;
    const std::vector<Point2> pa = polygonLocal(*fa, a), pb = polygonLocal(*fa, b);
    for (const Point2& p : pb)
        if (insideStrict(p, pa)) return true;
    for (const Point2& p : pa)
        if (insideStrict(p, pb)) return true;
    if (insideStrict(centroid2(pa), pb) || insideStrict(centroid2(pb), pa)) return true;
    return edgesCrossProperly(pa, pb);
}

bool barIsTie(const Project& project, int barId) {
    const Bar* bar = project.structure.findBar(barId);
    if (!bar || bar->sectionId == 0) return false;
    const BarDescription* d = project.barDescriptions.findBySection(bar->sectionId);
    return d && d->tieSelected;
}

namespace {

/// Fraction of a panel covered by the shells that lie in its plane (grid of sample points inside the panel). -1 when the panel has no sample point.
double shellCoverage(const Project& project, const Panel& panel) {
    const auto f = panelFrame(panel.vertices);
    if (!f) return -1.0;
    struct Cover {
        ShellFrame frame;
        std::vector<Point2> contour;
    };
    std::vector<Cover> covers;
    for (const Shell& s : project.structure.shells) {
        const auto sf = shellFrame(project.structure, s);
        if (!sf) continue;
        covers.push_back({*sf, shellContour2D(project.structure, s)});
    }
    const std::vector<Point2> poly = polygonLocal(*f, panel.vertices);
    double x0 = poly[0].x, x1 = x0, y0 = poly[0].y, y1 = y0;
    for (const Point2& p : poly) {
        x0 = std::min(x0, p.x);
        x1 = std::max(x1, p.x);
        y0 = std::min(y0, p.y);
        y1 = std::max(y1, p.y);
    }
    constexpr int kGrid = 24;
    int inside = 0, covered = 0;
    for (int i = 0; i < kGrid; ++i)
        for (int j = 0; j < kGrid; ++j) {
            const Point2 p{x0 + (x1 - x0) * (i + 0.5) / kGrid, y0 + (y1 - y0) * (j + 0.5) / kGrid};
            if (!pointInPolygon2D(p, poly, 0.0)) continue;
            ++inside;
            const Point3 world = f->toWorld(p);
            for (const Cover& c : covers)
                if (std::fabs(c.frame.heightOf(world)) <= kPanelPlaneToleranceM && pointInPolygon2D(c.frame.toLocal(world), c.contour, kPanelGeomToleranceM)) {
                    ++covered;
                    break;
                }
        }
    return inside == 0 ? -1.0 : static_cast<double>(covered) / inside;
}

}  // namespace

PanelDistribution distributePanels(const Project& project) {
    PanelDistribution out;
    const Panels& panels = project.panels;
    std::vector<DistributionBar> allBars;
    for (const Bar& bar : project.structure.bars) {
        if (!surfaceBarAssigned(project.surfaceLoads, bar.id) || barIsTie(project, bar.id)) continue;
        const Node *a = project.structure.findNode(bar.startNode), *b = project.structure.findNode(bar.endNode);
        if (a && b) allBars.push_back({bar.id, a->position, b->position});
    }
    for (std::size_t i = 0; i < panels.items.size(); ++i)
        for (std::size_t j = i + 1; j < panels.items.size(); ++j)
            if (panelsOverlap(panels.items[i].vertices, panels.items[j].vertices)) {
                PanelWarning w;
                w.kind = PanelWarning::Kind::Overlap;
                w.panelId = panels.items[i].id;
                w.otherPanelId = panels.items[j].id;
                w.message = panelWarningOverlapMessage();
                out.warnings.push_back(w);
            }
    const bool withShells = !project.structure.shells.empty();
    for (const Panel& panel : panels.items) {
        const std::vector<DistributionBar> bars = barsOfPanel(panel.vertices, allBars);
        if (bars.empty()) {
            PanelWarning w;
            w.panelId = panel.id;
            w.kind = PanelWarning::Kind::NoBars;
            w.message = panelWarningNoBarsMessage(withShells);
            if (withShells) {
                const double cover = shellCoverage(project, panel);
                if (cover >= 1.0 - 1e-9) continue;  // fully covered by shells: the transfer to shells is not implemented here
                if (cover > 0.0) {
                    w.kind = PanelWarning::Kind::NotCoveredByShells;
                    w.message = panelWarningNotCoveredByShellsMessage();
                }
            }
            out.warnings.push_back(w);
            continue;
        }
        const auto run = [&](const std::vector<Point3>& polygon, const SurfaceLoadData& data, int surfaceLoadId) {
            DistributionInput in;
            in.polygon = polygon;
            in.support = panel.vertices;
            in.direction = panel.direction;
            in.bars = bars;
            in.load = data;
            in.positiveDirection = panelPositiveDirection(panel.vertices, isWindCase(project.loadCases, data.loadCaseId));
            in.panelId = panel.id;
            in.surfaceLoadId = surfaceLoadId;
            const DistributionResult r = distributeSurfaceLoad(in);
            out.loads.insert(out.loads.end(), r.loads.begin(), r.loads.end());
            for (const Point3& p : r.unassignedPoints) {
                PanelWarning w;
                w.kind = PanelWarning::Kind::NotAssigned;
                w.panelId = panel.id;
                w.where = p;
                w.message = panelWarningNotAssignedMessage();
                out.warnings.push_back(w);
            }
        };
        for (const SurfaceLoadData& row : panel.loads) run(panel.vertices, row, 0);
        for (const SurfaceLoad& load : project.surfaceLoads.items) {
            const std::vector<int> owners = panelsContaining(panels, load.contour);
            if (!owners.empty() && owners.front() == panel.id) run(load.contour, load.data, load.id);
        }
    }
    return out;
}

}  // namespace cc::core
