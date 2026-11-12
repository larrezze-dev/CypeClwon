"""Prototype of the clone's 6-node Mindlin plate triangle (T-P8-02) before the C++ port.

Plate 4x4 m, h, E 30 GPa, nu 0.3, soft simple support (w = 0 on the edges, rotations free), net load q = 10000 - h*24.53.
Structured mesh: n x n cells, each split into two triangles; 6-node triangles (corner + mid-side nodes).
Shear integration variants: 'gauss3' (3 interior points), 'mitc' (assumed tangential shear strains tied at the edge Gauss points).
"""
import numpy as np, sys

def tri6_shape(r, s):
    t = 1 - r - s
    N = np.array([t*(2*t-1), r*(2*r-1), s*(2*s-1), 4*t*r, 4*r*s, 4*s*t])
    dr = np.array([-(4*t-1), 4*r-1, 0, 4*(t-r), 4*s, -4*s])
    ds = np.array([-(4*t-1), 0, 4*s-1, -4*r, 4*r, 4*(t-s)])
    return N, dr, ds

G3 = [(1/6, 1/6, 1/6), (2/3, 1/6, 1/6), (1/6, 2/3, 1/6)]

def element(xy, Db, Ds, q, shear):
    ke = np.zeros((18, 18)); fe = np.zeros(18)
    # DOF per node: w, bx, by with bx = rotation of the normal in xz (u = z*bx) -> gamma_x = dw/dx + bx
    def geom(r, s):
        N, dr, ds = tri6_shape(r, s)
        J = np.array([[dr@xy[:, 0], dr@xy[:, 1]], [ds@xy[:, 0], ds@xy[:, 1]]])
        dJ = np.linalg.det(J); Ji = np.linalg.inv(J)
        dx = Ji[0, 0]*dr + Ji[0, 1]*ds; dy = Ji[1, 0]*dr + Ji[1, 1]*ds
        return N, dx, dy, dJ, J
    for r, s, w in G3:
        N, dx, dy, dJ, J = geom(r, s)
        B = np.zeros((3, 18))
        B[0, 1::3] = dx; B[1, 2::3] = dy; B[2, 1::3] = dy; B[2, 2::3] = dx
        ke += B.T@Db@B*w*dJ
        fe[0::3] += N*q*w*dJ
        if shear == 'gauss3':
            Bs = np.zeros((2, 18)); Bs[0, 0::3] = dx; Bs[0, 1::3] = N; Bs[1, 0::3] = dy; Bs[1, 2::3] = N
            ke += Bs.T@Ds@Bs*w*dJ
    if shear == 'mitc':
        # MITC6-like: covariant shear strains e_r, e_s interpolated from tying points on the edges (2 per edge) + 1 interior
        # simple variant: covariant strains tied at edge Gauss points along the edge tangent, linear field (Bathe-Lee MITC6a style)
        a = 0.5 - 1/(2*np.sqrt(3)); b = 1 - a
        def cov(r, s):
            N, dr, ds = tri6_shape(r, s)
            Br = np.zeros(18); Bs_ = np.zeros(18)
            # e_r = dw/dr + bx*dx/dr + by*dy/dr
            xr = dr@xy[:, 0]; yr = dr@xy[:, 1]; xs = ds@xy[:, 0]; ys = ds@xy[:, 1]
            Br[0::3] = dr; Br[1::3] = N*xr; Br[2::3] = N*yr
            Bs_[0::3] = ds; Bs_[1::3] = N*xs; Bs_[2::3] = N*ys
            return Br, Bs_
        # tying: edge r (s=0) gives e_r at r=a,b; edge s (r=0) gives e_s at s=a,b; hypotenuse gives e_t=(e_s-e_r)/sqrt2 at (b,a),(a,b)
        er1 = cov(a, 0)[0]; er2 = cov(b, 0)[0]
        es1 = cov(0, a)[1]; es2 = cov(0, b)[1]
        Brb, Bsb = cov(b, a); Bra, Bsa = cov(a, b)
        et1 = Bsb - Brb; et2 = Bsa - Bra
        # interior tying at centroid for both
        Brc, Bsc = cov(1/3, 1/3)
        # linear assumed fields: e_r = a1 + b1 r + c1 s ; e_s = a2 + b2 r + c2 s  (6 unknowns, 6 edge conditions)
        # conditions: e_r(a,0), e_r(b,0), e_s(0,a), e_s(0,b), (e_s-e_r)(b,a), (e_s-e_r)(a,b)
        A = np.zeros((6, 6)); R = np.zeros((6, 18))
        rows = [((1, a, 0, 0, 0, 0), er1), ((1, b, 0, 0, 0, 0), er2), ((0, 0, 0, 1, 0, a), es1), ((0, 0, 0, 1, 0, b), es2),
                ((-1, -b, -a, 1, b, a), et1), ((-1, -a, -b, 1, a, b), et2)]
        for i, (c, v) in enumerate(rows):
            A[i] = c; R[i] = v
        C = np.linalg.solve(A, R)  # coefficients (a1 b1 c1 a2 b2 c2) as functions of dofs
        for r, s, w in G3:
            N, dx, dy, dJ, J = geom(r, s)
            er = C[0] + C[1]*r + C[2]*s; es = C[3] + C[4]*r + C[5]*s
            Ji = np.linalg.inv(J)
            gx = Ji[0, 0]*er + Ji[0, 1]*es; gy = Ji[1, 0]*er + Ji[1, 1]*es
            Bs = np.vstack([gx, gy])
            ke += Bs.T@Ds@Bs*w*dJ
    return ke, fe

def run(h=0.2, n=4, shear='mitc', a=4.0, E=30e6, nu=0.3, hard=False, diag='alt'):
    q = 10000 - h*24.53
    G = E/(2*(1+nu)); D = E*h**3/(12*(1-nu**2))
    Db = D*np.array([[1, nu, 0], [nu, 1, 0], [0, 0, (1-nu)/2]]); Ds = 5/6*G*h*np.eye(2)
    m = 2*n + 1  # node grid incl. mid nodes
    hstep = a/(2*n)
    def nid(i, j): return j*m + i
    nn = m*m
    K = np.zeros((3*nn, 3*nn)); F = np.zeros(3*nn)
    for j in range(n):
        for i in range(n):
            c = [(2*i, 2*j), (2*i+2, 2*j), (2*i+2, 2*j+2), (2*i, 2*j+2)]
            flip = (diag == 'alt' and (i+j) % 2 == 1)
            tris = [(0, 1, 2), (0, 2, 3)] if not flip else [(0, 1, 3), (1, 2, 3)]
            for t in tris:
                P = [c[k] for k in t]
                mids = [((P[0][0]+P[1][0])//2, (P[0][1]+P[1][1])//2), ((P[1][0]+P[2][0])//2, (P[1][1]+P[2][1])//2),
                        ((P[2][0]+P[0][0])//2, (P[2][1]+P[0][1])//2)]
                nodes = [nid(*p) for p in P + mids]
                xy = np.array([[p[0]*hstep, p[1]*hstep] for p in P + mids])
                ke, fe = element(xy, Db, Ds, q, shear)
                dofs = np.array([[3*k, 3*k+1, 3*k+2] for k in nodes]).ravel()
                K[np.ix_(dofs, dofs)] += ke; F[dofs] += fe
    fixed = set()
    for j in range(m):
        for i in range(m):
            if i in (0, m-1) or j in (0, m-1):
                fixed.add(3*nid(i, j))
                if hard:
                    fixed.add(3*nid(i, j) + (2 if i in (0, m-1) else 1))
    free = np.array([d for d in range(3*nn) if d not in fixed])
    u = np.zeros(3*nn); u[free] = np.linalg.solve(K[np.ix_(free, free)], F[free])
    return u[3*nid(n, n)]*1000

if __name__ == '__main__':
    for shear in ('gauss3', 'mitc'):
        for n in (2, 4, 8, 16):
            print(shear, n, 'soft w=%.2f' % run(0.2, n, shear), 'h0.4 %.2f' % run(0.4, n, shear), 'hard %.2f' % run(0.2, n, shear, hard=True))
