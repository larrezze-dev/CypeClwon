import numpy as np
E=210e6; G=E/2.6; Iy=1e-4; Avz=0.004; L=3.0; q=10.0
EI=E*Iy; GA=G*Avz; phi=12*EI/(GA*L*L)
def kel():
    c=EI/((1+phi)*L**3)
    return c*np.array([[12,6*L,-12,6*L],[6*L,(4+phi)*L*L,-6*L,(2-phi)*L*L],[-12,-6*L,12,-6*L],[6*L,(2-phi)*L*L,-6*L,(4+phi)*L*L]])
fe=np.array([-q*L/2,-q*L*L/12,-q*L/2,q*L*L/12])
kii=(4+phi)/(1+phi)*EI/L
def solve(Kspr):
    # dofs: w1,t1, w2,t2(node), t2b(end of bar1), w3,t3
    n=7; K=np.zeros((n,n)); f=np.zeros(n)
    ke=kel()
    idx1=[0,1,2,4] if Kspr is not None else [0,1,2,3]
    for a in range(4):
        for b in range(4): K[idx1[a],idx1[b]]+=ke[a,b]
        f[idx1[a]]+=fe[a]
    idx2=[2,3,5,6]
    for a in range(4):
        for b in range(4): K[idx2[a],idx2[b]]+=ke[a,b]
        f[idx2[a]]+=fe[a]
    if Kspr is not None:
        K[3,3]+=Kspr;K[4,4]+=Kspr;K[3,4]-=Kspr;K[4,3]-=Kspr
    free=[1,2,3,6]+([4] if Kspr is not None else [])
    u=np.zeros(n); u[free]=np.linalg.solve(K[np.ix_(free,free)],f[free]); return u
print('kii',kii,'4EI/L',4*EI/L)
print('no spring', solve(None)[[2,1,3,6]]*1000)
for name,K in (('Timoshenko kii',kii),('EB 4EI/L',4*EI/L)):
    Ks=0.5/(1-0.5)*K
    u=solve(Ks); print(name,'Dz2 mm',u[2]*1000,'th1 mRad',u[1]*1000,'th2node',u[3]*1000,'th2b',u[4]*1000,'th3',u[6]*1000)
