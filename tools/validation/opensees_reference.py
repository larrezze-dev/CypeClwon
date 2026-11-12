"""Reference values for the linear frame solver of the clone (src/core/fem) computed with the PUBLIC OpenSees (openseespy).

This is a support route only (T-P8-05): the original CYPE 3D embeds its own element (ElasticBeam3dCype) and CYPETransformation, so
agreement with public OpenSees is evidence of a correct linear frame theory, NOT of fidelity to the original.

Usage (from the project root, inside the project's own virtual environment):
    .toolchain/venv-opensees/Scripts/python.exe tools/validation/opensees_reference.py
Outputs tools/validation/opensees_reference.json and tests/opensees_reference_data.inc (the constants used by
tests/fem_opensees_ref_tests.cpp; ctest never runs Python).

Units: kN, m, rad, degC. Clone convention for the bar forces written to the outputs (12 numbers: origin 6, end 6):
    origin = -localForce(nodeI), end = +localForce(nodeJ) of OpenSees (N, Vy, Vz, T, My, Mz in local axes).
Standard elements only: elasticBeamColumn (Euler-Bernoulli) and ElasticTimoshenkoBeam (shear deformation), geomTransf Linear.
"""
import json
import math
import os
import sys

import numpy as np
import openseespy.opensees as ops

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(os.path.dirname(HERE))

E = 210e6
G = E / 2.6
ALPHA = 1.2e-5
RIGID = 1e11  # translational stiffness of the zeroLength links that emulate bar end releases
FREE = 1e-6   # rotational stiffness of a "free" release (OpenSees has no exactly-zero link)


def unit(v):
    v = np.asarray(v, float)
    return v / np.linalg.norm(v)


def axes_from_vecxz(p_i, p_j, vecxz):
    """Local axes like OpenSees geomTransf Linear: x along the bar, z in the plane (x, vecxz), y = z x x."""
    ex = unit(np.asarray(p_j, float) - np.asarray(p_i, float))
    z0 = np.asarray(vecxz, float)
    ey = unit(np.cross(z0, ex))
    ez = np.cross(ex, ey)
    return ex, ey, ez


class Sec:
    def __init__(self, a=0.01, iy=1e-4, iz=5e-5, j=8e-5, avy=0.004, avz=0.004, dy=0.1, dz=0.2):
        self.a, self.iy, self.iz, self.j, self.avy, self.avz, self.dy, self.dz = a, iy, iz, j, avy, avz, dy, dz


def hermite(s, h):
    return (1 - 3 * s * s + 2 * s ** 3, h * (s - 2 * s * s + s ** 3), 3 * s * s - 2 * s ** 3, h * (-s * s + s ** 3))


def element_load_vector(f, h):
    """Consistent end loads of one element for local components f(x) -> (qx, qy, qz) (x in [0, h]); returns two 6-vectors in local axes."""
    gx, gw = np.polynomial.legendre.leggauss(6)
    li = np.zeros(6)
    lj = np.zeros(6)
    for g, w in zip(gx, gw):
        x = 0.5 * h * (g + 1)
        wt = 0.5 * h * w
        qx, qy, qz = f(x)
        h1, h2, h3, h4 = hermite(x / h, h)
        li[0] += qx * (1 - x / h) * wt
        lj[0] += qx * (x / h) * wt
        li[1] += qy * h1 * wt  # Fy (v, theta_z = dv/dx)
        li[5] += qy * h2 * wt
        lj[1] += qy * h3 * wt
        lj[5] += qy * h4 * wt
        li[2] += qz * h1 * wt  # Fz (w, theta_y = -dw/dx)
        li[4] += -qz * h2 * wt
        lj[2] += qz * h3 * wt
        lj[4] += -qz * h4 * wt
    return li, lj


def add_consistent_loads(case, bar, ld, R, L, n, nodal_extra, ex, ey, ez, corr):
    h = L / n
    base = bar['i']
    ids = [bar['i']] + [50000 + bar['id'] * 1000 + k for k in range(1, n)] + [bar['j']]
    glob = np.array([ex, ey, ez])  # rows = local axes in global components

    def to_global(v6):
        return list(glob.T @ v6[:3]) + list(glob.T @ v6[3:])

    if ld['kind'] == 'point':
        p = np.array(ld['P'], float)
        pl = R @ p if ld.get('global') else p
        k = min(int(ld['x'] / h), n - 1)
        s = (ld['x'] - k * h) / h
        h1, h2, h3, h4 = hermite(s, h)
        li = np.array([pl[0] * (1 - s), pl[1] * h1, pl[2] * h1, 0, -pl[2] * h2, pl[1] * h2])
        lj = np.array([pl[0] * s, pl[1] * h3, pl[2] * h3, 0, -pl[2] * h4, pl[1] * h4])
        nodal_extra.append((ids[k], to_global(li)))
        nodal_extra.append((ids[k + 1], to_global(lj)))
        if k == 0:
            corr[0] += li
        if k == n - 1:
            corr[1] += lj
        return
    for k in range(n):
        xa = k * h

        def f(x, xa=xa):
            xx = xa + x
            if ld['kind'] == 'uniform':
                q = np.array(ld['q'], float)
                return R @ q if ld.get('global') else q
            if xx < ld['x1'] - 1e-12 or xx > ld['x2'] + 1e-12:
                return np.zeros(3)
            t = (xx - ld['x1']) / (ld['x2'] - ld['x1'])
            return np.array(ld['w1'], float) * (1 - t) + np.array(ld['w2'], float) * t

        li, lj = element_load_vector(f, h)
        nodal_extra.append((ids[k], to_global(li)))
        nodal_extra.append((ids[k + 1], to_global(lj)))
        if k == 0:
            corr[0] += li
        if k == n - 1:
            corr[1] += lj


def run(case, beam):
    """case: dict with nodes {id: (x, y, z)}, fixed {id: [6 bool]}, bars [...], loads. beam: 'eb' or 'ti'."""
    ops.wipe()
    ops.model('basic', '-ndm', 3, '-ndf', 6)
    for nid, p in case['nodes'].items():
        ops.node(nid, *p)
    for nid, mask in case['fixed'].items():
        pres = case.get('prescribed', {}).get(nid, {})  # prescribed DOFs get a non-homogeneous sp, not a fix
        ops.fix(nid, *[1 if (m and (d + 1) not in pres) else 0 for d, m in enumerate(mask)])
    ops.uniaxialMaterial('Elastic', 1, RIGID)
    ops.uniaxialMaterial('Elastic', 2, FREE)
    next_mat = [10]
    next_node = [10000]
    next_ele = [20000]
    next_tr = [1]
    ele_of_bar = {}  # bar id -> list of element tags
    axes_of_bar = {}
    nodal_extra = []  # equivalent nodal loads emulating temperature and the consistent loads of the meshed bars
    thermal_local = {}  # bar id -> [equivalent load at I, at J] (local axes) of the first / last element; subtracted from the element forces

    for bar in case['bars']:
        pi, pj = np.array(case['nodes'][bar['i']], float), np.array(case['nodes'][bar['j']], float)
        ex, ey, ez = axes_from_vecxz(pi, pj, bar['vecxz'])
        axes_of_bar[bar['id']] = (ex, ey, ez)
        sec = bar.get('sec', Sec())
        loaded = any(ld['bar'] == bar['id'] and ld['kind'] in ('uniform', 'point', 'trap') for ld in case.get('barloads', []))
        # ElasticTimoshenkoBeam accepts no element loads: its loaded bars are meshed and loaded with consistent nodal loads
        n = bar.get('mesh', 240 if (beam == 'ti' and loaded) else 1)
        if beam == 'ti' and n == 1:
            pass
        bar['_n'] = n
        pts = [bar['i']]
        for k in range(1, n):
            nid = 50000 + bar['id'] * 1000 + k
            ops.node(nid, *(pi + (pj - pi) * k / n))
            pts.append(nid)
        pts.append(bar['j'])
        ele_tags = []
        for k in range(n):
            a, b = pts[k], pts[k + 1]
            if k == 0 and bar.get('rel_i'):
                a = release_link(bar, ex, ey, 0, case, next_node, next_ele, next_mat)
            if k == n - 1 and bar.get('rel_j'):
                b = release_link(bar, ex, ey, 1, case, next_node, next_ele, next_mat)
            ops.geomTransf('Linear', next_tr[0], *ez.tolist())
            tag = next_ele[0]
            next_ele[0] += 1
            if beam == 'eb':
                ops.element('elasticBeamColumn', tag, a, b, sec.a, E, G, sec.j, sec.iy, sec.iz, next_tr[0])
            else:
                ops.element('ElasticTimoshenkoBeam', tag, a, b, E, G, sec.a, sec.j, sec.iy, sec.iz, sec.avy, sec.avz, next_tr[0])
            next_tr[0] += 1
            ele_tags.append(tag)
        ele_of_bar[bar['id']] = ele_tags
    ops.timeSeries('Constant', 1)
    ops.pattern('Plain', 1, 1)
    for nid, f in case.get('nodal', {}).items():
        ops.load(nid, *f)
    bars = {b['id']: b for b in case['bars']}
    for ld in case.get('barloads', []):
        bar = bars[ld['bar']]
        ex, ey, ez = axes_of_bar[bar['id']]
        R = np.array([ex, ey, ez])  # global -> local
        sec = bar.get('sec', Sec())
        L = np.linalg.norm(np.array(case['nodes'][bar['j']], float) - np.array(case['nodes'][bar['i']], float))
        n = bar['_n']
        tags = ele_of_bar[bar['id']]
        nodal_route = (beam == 'ti') or ld['kind'] == 'trap'
        if ld['kind'] in ('uniform', 'point') and not nodal_route:
            if ld['kind'] == 'uniform':
                q = np.array(ld['q'], float)
                ql = R @ q if ld.get('global') else q
                for t in tags:
                    ops.eleLoad('-ele', t, '-type', '-beamUniform', ql[1], ql[2], ql[0])
            else:
                p = np.array(ld['P'], float)
                pl = R @ p if ld.get('global') else p
                ops.eleLoad('-ele', tags[0], '-type', '-beamPoint', pl[1], pl[2], ld['x'] / L, pl[0])
        elif ld['kind'] in ('uniform', 'point', 'trap'):
            corr = thermal_local.setdefault(bar['id'], [np.zeros(6), np.zeros(6)])
            add_consistent_loads(case, bar, ld, R, L, n, nodal_extra, ex, ey, ez, corr)
        elif ld['kind'] == 'temp':
            # equivalent nodal loads of a straight prismatic bar (exact): N = E A alpha T, My = E Iy alpha dTz / depthZ,
            # Mz = -E Iz alpha dTy / depthY (sign: hotter +y face bends towards -y, hotter +z face towards -z)
            nforce = E * sec.a * ALPHA * ld.get('uniform', 0.0)
            my = E * sec.iy * ALPHA * ld.get('gz', 0.0) / sec.dz if sec.dz else 0.0
            mz = -E * sec.iz * ALPHA * ld.get('gy', 0.0) / sec.dy if sec.dy else 0.0
            mi = -my * ey - mz * ez  # moment vectors: My about local y, Mz about local z
            mj = my * ey + mz * ez
            fi = -nforce * ex
            fj = nforce * ex
            nodal_extra.append((bar['i'], list(fi) + list(mi)))
            nodal_extra.append((bar['j'], list(fj) + list(mj)))
            corr = thermal_local.setdefault(bar['id'], [np.zeros(6), np.zeros(6)])
            corr[0] += np.array([-nforce, 0, 0, 0, -my, -mz])
            corr[1] += np.array([nforce, 0, 0, 0, my, mz])
    for nid, f in nodal_extra:
        ops.load(nid, *f)
    for nid, vals in case.get('prescribed', {}).items():
        for dof, v in vals.items():
            ops.sp(nid, dof, v)
    if len(case['fixed']) == len(case['nodes']) and all(all(m) for m in case['fixed'].values()):
        # no free DOF: OpenSees needs at least one equation, add an isolated dummy node tied to the ground by a spring
        ops.node(99998, 0, 0, 0)
        ops.node(99999, 0, 0, 0)
        ops.fix(99998, 1, 1, 1, 1, 1, 1)
        ops.fix(99999, 0, 1, 1, 1, 1, 1)
        ops.element('zeroLength', 99997, 99998, 99999, '-mat', 1, '-dir', 1)
    ops.constraints('Transformation')  # Plain ignores the non-zero values of sp
    ops.numberer('Plain')
    ops.system('FullGeneral')
    ops.test('NormDispIncr', 1e-14, 5)
    ops.algorithm('Linear')
    ops.integrator('LoadControl', 1.0)
    ops.analysis('Static')
    if ops.analyze(1) != 0:
        raise RuntimeError('analysis failed')
    ops.reactions()
    out = {'disp': {}, 'react': {}, 'bars': {}}
    for nid in case['nodes']:
        out['disp'][nid] = [ops.nodeDisp(nid, d) for d in range(1, 7)]
    for nid, mask in case['fixed'].items():
        out['react'][nid] = [ops.nodeReaction(nid, d) for d in range(1, 7)]
    for bid, tags in ele_of_bar.items():
        f0 = ops.eleResponse(tags[0], 'localForce')
        f1 = ops.eleResponse(tags[-1], 'localForce')
        th_i, th_j = thermal_local.get(bid, (np.zeros(6), np.zeros(6)))
        out['bars'][bid] = [-(v - t) for v, t in zip(f0[:6], th_i)] + [v - t for v, t in zip(f1[6:], th_j)]
    return out


def release_link(bar, ex, ey, end, case, next_node, next_ele, next_mat):
    """Dummy node + zeroLength link (local orientation of the bar) that emulates the rotational releases of a bar end."""
    real = bar['i'] if end == 0 else bar['j']
    nid = next_node[0]
    next_node[0] += 1
    ops.node(nid, *case['nodes'][real])
    rel = bar['rel_i'] if end == 0 else bar['rel_j']  # {'x': K|'free', 'y': ..., 'z': ...}
    mats = []
    for key in ('x', 'y', 'z'):
        v = rel.get(key)
        if v is None:
            mats.append(1)
        elif v == 'free':
            mats.append(2)
        else:
            m = next_mat[0]
            next_mat[0] += 1
            ops.uniaxialMaterial('Elastic', m, v)
            mats.append(m)
    tag = next_ele[0]
    next_ele[0] += 1
    ops.element('zeroLength', tag, real, nid, '-mat', 1, 1, 1, *mats, '-dir', 1, 2, 3, 4, 5, 6, '-orient', *ex.tolist(), *ey.tolist())
    return nid


# ----------------------------------------------------------------------------------------------------------------------------------
# Case definitions. Every one is mirrored by hand in tests/fem_opensees_ref_tests.cpp (same ids, coordinates, loads).
# ----------------------------------------------------------------------------------------------------------------------------------
CLAMP = [True] * 6
XZ = (0, 0, 1)  # vecxz of a bar along X (local z = global Z)
ZX = (1, 0, 0)  # vecxz of a bar along Z (local z = global X, local y = -Y)


def cases():
    c = {}
    # A1: cantilever 4 m, nodal load at the tip in -Z, +Y, axial and torque
    c['A1_cantilever_nodal'] = dict(
        nodes={1: (0, 0, 0), 2: (4, 0, 0)}, fixed={1: CLAMP}, bars=[dict(id=1, i=1, j=2, vecxz=XZ)],
        nodal={2: [7.0, 3.0, -10.0, 2.0, 0.0, 0.0]})
    # A2: simply supported 6 m, uniform global load, and an off-centre point load (one element each)
    ss_fixed = {1: [True, True, True, True, False, False], 2: [False, True, True, False, False, False]}
    c['B1_simply_supported_uniform'] = dict(
        nodes={1: (0, 0, 0), 2: (6, 0, 0)}, fixed=ss_fixed, bars=[dict(id=1, i=1, j=2, vecxz=XZ)],
        barloads=[dict(bar=1, kind='uniform', q=(0, 0, -10), **{'global': True})])
    c['B2_simply_supported_point'] = dict(
        nodes={1: (0, 0, 0), 2: (6, 0, 0)}, fixed=ss_fixed, bars=[dict(id=1, i=1, j=2, vecxz=XZ)],
        barloads=[dict(bar=1, kind='point', P=(3.0, 2.0, -10.0), x=1.5)])
    # C1: planar portal frame in XZ, columns 4 m, beam 6 m, clamped bases, H = 20 kN at node 2, q = 8 kN/m on the beam
    c['C1_portal_frame'] = dict(
        nodes={1: (0, 0, 0), 2: (0, 0, 4), 3: (6, 0, 4), 4: (6, 0, 0)}, fixed={1: CLAMP, 4: CLAMP},
        bars=[dict(id=1, i=1, j=2, vecxz=ZX), dict(id=2, i=2, j=3, vecxz=XZ), dict(id=3, i=3, j=4, vecxz=ZX)],
        nodal={2: [20.0, 0, 0, 0, 0, 0]},
        barloads=[dict(bar=2, kind='uniform', q=(0, 0, -8), **{'global': True})])
    # D1: spatial frame, skew bars, roll angles (vecxz not on a principal plane), Avy != Avz
    sd = Sec(avy=0.003, avz=0.005)
    c['D1_space_frame'] = dict(
        nodes={1: (0, 0, 0), 2: (0, 0, 4), 3: (5, 0, 4.5), 4: (5, 3, 4), 5: (5, 3, 0)}, fixed={1: CLAMP, 5: [True] * 6},
        bars=[dict(id=1, i=1, j=2, vecxz=(1, 0.3, 0), sec=sd), dict(id=2, i=2, j=3, vecxz=(0.2, 0.4, 1), sec=sd),
              dict(id=3, i=3, j=4, vecxz=(0.5, 0, 1), sec=sd), dict(id=4, i=4, j=5, vecxz=(1, 1, 0), sec=sd)],
        nodal={3: [5.0, -3.0, -8.0, 1.0, 2.0, 3.0]},
        barloads=[dict(bar=2, kind='uniform', q=(0, 0, -12), **{'global': True}),
                  dict(bar=3, kind='uniform', q=(0, 4.0, -6.0), **{'global': False}),
                  dict(bar=3, kind='point', P=(1.0, -2.0, -9.0), x=1.2)])
    # E1: propped cantilever, hinge at the far end (both bending planes) under a uniform load
    c['E1_end_hinge'] = dict(
        nodes={1: (0, 0, 0), 2: (6, 0, 0)}, fixed={1: CLAMP, 2: [True, True, True, False, False, False]},
        bars=[dict(id=1, i=1, j=2, vecxz=XZ, rel_j={'y': 'free', 'z': 'free'})],
        barloads=[dict(bar=1, kind='uniform', q=(0, 3, -10), **{'global': True})])
    # E2: two bars with an internal hinge (plane xz only) at the common node, load at the hinge
    c['E2_internal_hinge'] = dict(
        nodes={1: (0, 0, 0), 2: (3, 0, 0), 3: (6, 0, 0)}, fixed={1: CLAMP, 3: CLAMP},
        bars=[dict(id=1, i=1, j=2, vecxz=XZ, rel_j={'y': 'free'}), dict(id=2, i=2, j=3, vecxz=XZ, rel_i={'y': 'free'})],
        nodal={2: [0, 0, -10.0, 0, 0, 0]})
    # E3: partial fixity (rotational springs) at both ends of a beam, both planes, with a uniform load
    c['E3_partial_fixity'] = dict(
        nodes={1: (0, 0, 0), 2: (6, 0, 0)}, fixed={1: CLAMP, 2: CLAMP},
        bars=[dict(id=1, i=1, j=2, vecxz=XZ, rel_i={'y': 2.0e4, 'z': 1.0e4}, rel_j={'y': 5.0e3, 'z': 3.0e3})],
        barloads=[dict(bar=1, kind='uniform', q=(0, 3, -10), **{'global': True})])
    # F1..F3: temperature (equivalent nodal loads, see the loads section)
    c['F1_temperature_uniform_restrained'] = dict(
        nodes={1: (0, 0, 0), 2: (4, 0, 0)}, fixed={1: CLAMP, 2: CLAMP}, bars=[dict(id=1, i=1, j=2, vecxz=XZ)],
        barloads=[dict(bar=1, kind='temp', uniform=10.0)])
    c['F2_temperature_gradient_cantilever'] = dict(
        nodes={1: (0, 0, 0), 2: (4, 0, 0)}, fixed={1: CLAMP}, bars=[dict(id=1, i=1, j=2, vecxz=XZ)],
        barloads=[dict(bar=1, kind='temp', uniform=5.0, gz=20.0, gy=-15.0)])
    c['F3_temperature_gradient_restrained'] = dict(
        nodes={1: (0, 0, 0), 2: (4, 0, 0)}, fixed={1: CLAMP, 2: CLAMP}, bars=[dict(id=1, i=1, j=2, vecxz=XZ)],
        barloads=[dict(bar=1, kind='temp', uniform=0.0, gz=20.0, gy=10.0)])
    # G1: trapezoid load on part of a cantilever (x 1..3 m, local z from -4 to -12 kN/m, y from 0 to 6), mesh of 200 elements
    c['G1_trapezoid_partial'] = dict(
        nodes={1: (0, 0, 0), 2: (4, 0, 0)}, fixed={1: CLAMP}, bars=[dict(id=1, i=1, j=2, vecxz=XZ, mesh=200)],
        barloads=[dict(bar=1, kind='trap', x1=1.0, x2=3.0, w1=(0, 0, -4.0), w2=(0, 6.0, -12.0))])
    # H1: roll angle: bar along X rolled 30 deg (local z = (0, -sin30, cos30)), global uniform and nodal load
    roll = math.radians(30.0)
    c['H1_roll_angle'] = dict(
        nodes={1: (0, 0, 0), 2: (5, 0, 0)}, fixed={1: CLAMP}, bars=[dict(id=1, i=1, j=2, vecxz=(0, -math.sin(roll), math.cos(roll)),
                                                                          sec=Sec(avy=0.003, avz=0.005))],
        nodal={2: [0, 0, -5.0, 0, 0, 0]}, barloads=[dict(bar=1, kind='uniform', q=(0, 0, -6), **{'global': True})])
    # I1: two-span continuous beam, settlement of the middle support (5 mm down) and 2 mrad rotation of the clamp
    c['I1_support_settlement'] = dict(
        nodes={1: (0, 0, 0), 2: (5, 0, 0), 3: (10, 0, 0)},
        fixed={1: [True, True, True, True, False, False], 2: [False, True, True, False, False, False],
               3: [False, True, True, False, False, False]},
        bars=[dict(id=1, i=1, j=2, vecxz=XZ), dict(id=2, i=2, j=3, vecxz=XZ)],
        prescribed={2: {3: -0.005}})
    c['I2_clamp_rotation_settlement'] = dict(
        nodes={1: (0, 0, 0), 2: (6, 0, 0)}, fixed={1: CLAMP, 2: CLAMP}, bars=[dict(id=1, i=1, j=2, vecxz=XZ)],
        prescribed={1: {5: 0.002, 3: 0.004}, 2: {2: 0.003}})
    return c


# Sections whose depth matters for the thermal gradient are the default Sec (dy = 0.1, dz = 0.2); the C++ mirror uses the same values.
CASE_BEAMS = {'eb': 'elasticBeamColumn (Euler-Bernoulli)', 'ti': 'ElasticTimoshenkoBeam'}


def fmt(v):
    return '%.12e' % v


def main():
    allref = {}
    for name, case in cases().items():
        for beam in ('eb', 'ti'):
            key = '%s_%s' % (name, beam)
            try:
                allref[key] = run(case, beam)
            except Exception as ex:  # unsupported eleLoad etc. is reported, never hidden
                print('FAILED', key, ex, file=sys.stderr)
    with open(os.path.join(HERE, 'opensees_reference.json'), 'w') as f:
        json.dump({'openseespy': ops.version(), 'cases': allref}, f, indent=1)
    lines = ['// GENERATED by tools/validation/opensees_reference.py (public OpenSees %s via openseespy). Do not edit by hand.' % ops.version(),
             '// Disp/react: global axes (m, rad, kN, kN.m). Bar forces: 12 numbers (origin 6, end 6) in the clone convention.']
    for key, r in allref.items():
        lines.append('const RefResult kRef_%s{' % key)
        lines.append('    {%s},' % ', '.join('{%d, {%s}}' % (n, ', '.join(map(fmt, v))) for n, v in r['disp'].items() if n < 10000))
        lines.append('    {%s},' % ', '.join('{%d, {%s}}' % (n, ', '.join(map(fmt, v))) for n, v in r['react'].items()))
        lines.append('    {%s}};' % ', '.join('{%d, {%s}}' % (n, ', '.join(map(fmt, v))) for n, v in r['bars'].items()))
    with open(os.path.join(ROOT, 'tests', 'opensees_reference_data.inc'), 'w') as f:
        f.write('\n'.join(lines) + '\n')
    print('cases written:', len(allref), 'openseespy', ops.version())


if __name__ == '__main__':
    main()
