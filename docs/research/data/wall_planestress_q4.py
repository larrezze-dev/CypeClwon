import numpy as np
# Plane-stress Q4 (incompatible modes) deep beam: L=4 (x), H=2 (z), t=0.2, fixed at x=0, uniform vertical area load q (kN/m2 of mid-surface)
E=30e6; nu=0.3; t=0.2; L=4.0; H=2.0; q=1000+0.2*24.53
def run(Fxx=1,Fyy=1,Fxy=1,s12="sqrt",nx=64,nz=32):
    f12={"sqrt":np.sqrt(Fxx*Fyy),"a":1.0,"F":Fxx,"row":Fxx}[s12]
    D=E*t/(1-nu**2)*np.array([[Fxx,f12*nu,0],[f12*nu,Fyy,0],[0,0,Fxy*(1-nu)/2]])
    hx=L/nx; hz=H/nz
    n=(nx+1)*(nz+1)
    K=np.zeros((2*n,2*n)); F=np.zeros(2*n)
    gp=[-1/np.sqrt(3),1/np.sqrt(3)]
    def nid(i,j): return j*(nx+1)+i
    for j in range(nz):
        for i in range(nx):
            nd=[nid(i,j),nid(i+1,j),nid(i+1,j+1),nid(i,j+1)]
            dofs=np.array([[2*k,2*k+1] for k in nd]).ravel()
            ke=np.zeros((8,8)); fe=np.zeros(8)
            xs=[-1,1,1,-1]; es=[-1,-1,1,1]
            Jx=hx/2; Jz=hz/2
            # incompatible modes: extra shape functions (1-xi^2),(1-eta^2)
            Kaa=np.zeros((4,4)); Kau=np.zeros((4,8))
            for xi in gp:
                for eta in gp:
                    dNx=np.array([xs[k]*(1+es[k]*eta)/4 for k in range(4)])/Jx
                    dNz=np.array([es[k]*(1+xs[k]*xi)/4 for k in range(4)])/Jz
                    N=np.array([(1+xs[k]*xi)*(1+es[k]*eta)/4 for k in range(4)])
                    B=np.zeros((3,8))
                    for k in range(4):
                        B[0,2*k]=dNx[k]; B[1,2*k+1]=dNz[k]; B[2,2*k]=dNz[k]; B[2,2*k+1]=dNx[k]
                    G=np.zeros((3,4))
                    G[0,0]=-2*xi/Jx; G[1,1]=-2*eta/Jz; G[2,0]=0; G[2,1]=0
                    # bubble dofs: modes u:(1-xi^2) [a0,a1 for x?]. Use 2 modes per direction: 4 dofs (ux:xi,eta ; uz:xi,eta)
                    G=np.zeros((3,4))
                    G[0,0]=-2*xi/Jx; G[2,0]=0
                    G[0,1]=0;        G[2,1]=-2*eta/Jz   # ux: (1-eta^2)
                    G[2,2]=-2*xi/Jx                     # uz: (1-xi^2)
                    G[1,3]=-2*eta/Jz                    # uz: (1-eta^2)
                    # fix mode meaning: dofs a0=ux(1-xi^2), a1=ux(1-eta^2), a2=uz(1-xi^2), a3=uz(1-eta^2)
                    G[:,:]=0
                    G[0,0]=-2*xi/Jx; G[2,0]=0
                    G[2,1]=-2*eta/Jz
                    G[2,2]=-2*xi/Jx
                    G[1,3]=-2*eta/Jz
                    w=Jx*Jz
                    ke+=B.T@D@B*w; Kaa+=G.T@D@G*w; Kau+=G.T@D@B*w
                    fe+=np.array([N[k] if c==1 else 0 for k in range(4) for c in range(2)])*(-q)*w
            ke-=Kau.T@np.linalg.solve(Kaa,Kau)
            K[np.ix_(dofs,dofs)]+=ke; F[dofs]+=fe
    fixed=[]
    for j in range(nz+1):
        k=nid(0,j); fixed+= [2*k,2*k+1]
    free=np.array([d for d in range(2*n) if d not in set(fixed)])
    u=np.zeros(2*n); u[free]=np.linalg.solve(K[np.ix_(free,free)],F[free])
    tip=[nid(nx,j) for j in range(nz+1)]
    uz=min(u[2*k+1] for k in tip); ux=max(abs(u[2*k]) for k in tip)
    return uz*1000, ux*1000
if __name__=="__main__":
    b=run()
    print("baseline uz %.2f ux %.2f"%b)
    for s in ("a","sqrt","F"):
        r=run(Fxx=0.5,s12=s); print("Fxx=0.5 D12 %-5s uz %.2f (%.3f) ux %.2f (%.3f)"%(s,r[0],r[0]/b[0],r[1],r[1]/b[1]))
    print("original ratios uz 1.797 ux 1.994")
    r=run(Fyy=0.5,s12="sqrt"); print("Fyy=0.5 sqrt uz %.2f (%.3f) ux %.2f (%.3f)"%(r[0],r[0]/b[0],r[1],r[1]/b[1]))
    r=run(Fyy=0.5,s12="a"); print("Fyy=0.5 a    uz %.2f (%.3f) ux %.2f (%.3f)"%(r[0],r[0]/b[0],r[1],r[1]/b[1]))
    r=run(Fxy=0.5); print("Fxy=0.5 uz %.2f (%.3f) ux %.2f (%.3f)"%(r[0],r[0]/b[0],r[1],r[1]/b[1]))
