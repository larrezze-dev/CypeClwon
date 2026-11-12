import numpy as np, sys
# Mindlin plate, 4-node Q4 with 2x2 bending and 1-point (reduced) shear, uniform mesh, SS edges soft/hard
def run(h, nel=32, hard=False, kappa=5/6, E=30e6, nu=0.3, q=None, a=4.0, Fm=(1,1,1), Fv=(1,1)):
    if q is None: q = 10000 - h*24.53
    G = E/(2*(1+nu)); Dm = E*h**3/(12*(1-nu**2))
    Db = Dm*np.array([[Fm[0],np.sqrt(Fm[0]*Fm[1])*nu,0],[np.sqrt(Fm[0]*Fm[1])*nu,Fm[1],0],[0,0,Fm[2]*(1-nu)/2]])
    Ds = np.diag([kappa*G*h*Fv[0], kappa*G*h*Fv[1]])
    n = nel+1; nn = n*n; le = a/nel
    K = np.zeros((3*nn,3*nn)); F = np.zeros(3*nn)
    gp = [-1/np.sqrt(3), 1/np.sqrt(3)]
    def node(i,j): return j*n+i
    for ej in range(nel):
        for ei in range(nel):
            nd = [node(ei,ej),node(ei+1,ej),node(ei+1,ej+1),node(ei,ej+1)]
            dofs = np.array([[3*k,3*k+1,3*k+2] for k in nd]).ravel()
            ke = np.zeros((12,12)); fe = np.zeros(12)
            xi_n = [-1,1,1,-1]; eta_n = [-1,-1,1,1]
            J = le/2  # square element: dx/dxi = le/2
            def Bparts(xi,eta):
                N = np.array([(1+xi_n[k]*xi)*(1+eta_n[k]*eta)/4 for k in range(4)])
                dNx = np.array([xi_n[k]*(1+eta_n[k]*eta)/4 for k in range(4)])/J
                dNy = np.array([eta_n[k]*(1+xi_n[k]*xi)/4 for k in range(4)])/J
                return N,dNx,dNy
            # DOF per node: w, thx (rotation about x), thy (rotation about y); use beta_x = psi_x (rotation of normal about y -> -?) use Reddy: u=zψx?
            # Use: kx = dψx/dx, ky = dψy/dy, kxy = dψx/dy+dψy/dx ; gamma_x = dw/dx + ψx ; gamma_y = dw/dy + ψy  (ψ = rotation of normal in x,y)
            for xi in gp:
                for eta in gp:
                    N,dNx,dNy = Bparts(xi,eta)
                    B = np.zeros((3,12))
                    for k in range(4):
                        B[0,3*k+1] = dNx[k]
                        B[1,3*k+2] = dNy[k]
                        B[2,3*k+1] = dNy[k]; B[2,3*k+2] = dNx[k]
                    ke += B.T@Db@B*J*J
                    fe += np.array([N[k] if c==0 else 0 for k in range(4) for c in range(3)])*q*J*J
            N,dNx,dNy = Bparts(0,0)
            Bs = np.zeros((2,12))
            for k in range(4):
                Bs[0,3*k] = dNx[k]; Bs[0,3*k+1] = N[k]
                Bs[1,3*k] = dNy[k]; Bs[1,3*k+2] = N[k]
            ke += Bs.T@Ds@Bs*4*J*J
            K[np.ix_(dofs,dofs)] += ke; F[dofs] += fe
    fixed = set()
    for i in range(n):
        for j in range(n):
            k = node(i,j)
            if i in (0,n-1) or j in (0,n-1): fixed.add(3*k)
            if hard:
                if i in (0,n-1): fixed.add(3*k+2)   # tangential rotation psi_y on x=const edges
                if j in (0,n-1): fixed.add(3*k+1)
    free = np.array([d for d in range(3*nn) if d not in fixed])
    u = np.zeros(3*nn); u[free] = np.linalg.solve(K[np.ix_(free,free)], F[free])
    c = node(nel//2, nel//2)
    # moment at centre (average of 4 elements Gauss values at centre): use curvature by finite difference of psi
    psi_x = u[3*np.array([node(nel//2+1,nel//2),node(nel//2-1,nel//2)])+1]
    psi_y = u[3*np.array([node(nel//2,nel//2+1),node(nel//2,nel//2-1)])+2]
    kx = (psi_x[0]-psi_x[1])/(2*le); ky = (psi_y[0]-psi_y[1])/(2*le)
    Mx = Db[0,0]*kx+Db[0,1]*ky
    return u[3*c], Mx
if __name__ == "__main__":
    for h in (0.2,0.4):
        for hard in (False,True):
            w,Mx = run(h,32,hard)
            print("h=%.1f %s: w_c=%.2f mm  Mx_c=%.0f"%(h,"hard" if hard else "soft",w*1000,Mx))
