"""Independent planar frame check (XZ plane, 3 dof per node) for the C3 portal measured in the original.
Beam: Euler-Bernoulli or Timoshenko. Used to decide which section inertia / shear area the original applies to vertical bars."""
import numpy as np, itertools
E=210e6; nu=0.3; G=E/(2*(1+nu)); A=0.01
def kbar(EI,GAs,EA,L,tim):
    phi=12*EI/(GAs*L*L) if tim else 0.0
    k=np.zeros((6,6)); a=EA/L
    k[0,0]=k[3,3]=a; k[0,3]=k[3,0]=-a
    c=EI/((1+phi)*L**3)
    k[1,1]=k[4,4]=12*c; k[1,4]=k[4,1]=-12*c
    k[1,2]=k[2,1]=6*L*c; k[1,5]=k[5,1]=6*L*c
    k[2,4]=k[4,2]=-6*L*c; k[4,5]=k[5,4]=-6*L*c
    k[2,2]=k[5,5]=(4+phi)*L*L*c; k[2,5]=k[5,2]=(2-phi)*L*L*c
    return k
def solve(EIc,GAsc,EIb,GAsb,tim,H=20.0):
    nodes={0:(0,0),1:(0,4),2:(6,4),3:(6,0)}
    bars=[(0,1,EIc,GAsc),(1,2,EIb,GAsb),(2,3,EIc,GAsc)]
    K=np.zeros((12,12))
    for i,j,EI,GAs in bars:
        xi,zi=nodes[i]; xj,zj=nodes[j]; L=np.hypot(xj-xi,zj-zi); c=(xj-xi)/L; s=(zj-zi)/L
        # local (axial u, transverse w(+90deg ccw of axis), rot) -> global (ux,uz,ry) ; rotation about +y = -rotation about ccw-in-xz plane
        # use plane angle convention in (x,z): w along (-s,c). ry (about +y) = -theta_ccw
        T=np.array([[c,s,0],[-s,c,0],[0,0,1]])
        Tb=np.zeros((6,6)); Tb[:3,:3]=T; Tb[3:,3:]=T
        kg=Tb.T@kbar(EI,GAs,E*A,L,tim)@Tb
        idx=list(range(3*i,3*i+3))+list(range(3*j,3*j+3))
        for a in range(6):
            for b in range(6): K[idx[a],idx[b]]+=kg[a,b]
    # theta here is ccw in the (x,z) plane == -ry
    F=np.zeros(12); F[3*1]=H
    free=[3,4,5,6,7,8]
    u=np.zeros(12); u[free]=np.linalg.solve(K[np.ix_(free,free)],F[free])
    R=K@u-F
    th=lambda n:-u[3*n+2]
    return u,R,th
Iy=1e-4; Iz=5e-5
cases={}
for name,(EIc,GAc,EIb,GAb,tim) in {
 'EB all Iy':(E*Iy,G*0.004,E*Iy,G*0.004,False),
 'Tim all Iy':(E*Iy,G*0.004,E*Iy,G*0.004,True),
 'EB col Iz, beam Iy':(E*Iz,G*0.004,E*Iy,G*0.004,False),
 'Tim col Iz, beam Iy':(E*Iz,G*0.004,E*Iy,G*0.004,True),
 'EB all Iz':(E*Iz,G*0.004,E*Iz,G*0.004,False),
 'Tim all Iz':(E*Iz,G*0.004,E*Iz,G*0.004,True)}.items():
    u,R,th=solve(EIc,GAc,EIb,GAb,tim)
    print(f"{name:22s} ux1 {u[3]*1e3:8.4f} ux2 {u[6]*1e3:8.4f} uz1 {u[4]*1e3:8.4f}  ry1 {th(1)*1e3:7.4f} ry2 {th(2)*1e3:7.4f} | R0 Fx {R[0]:8.4f} Fz {R[1]:8.4f} My {R[2]*-1:8.4f}| R3 Fx {R[9]:8.4f} Fz {R[10]:8.4f} My {R[11]*-1:8.4f}")
print("measured: ux1 6.950 ux2 6.921 uz1 0.011 ry1 0.869 ry2 0.863 | R1 Fx -10.015 Fz -5.908 My -22.31 | R4 Fx -9.985 Fz 5.909 My -22.23")
