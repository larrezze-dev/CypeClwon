import numpy as np
a=4.0; E=30e6; nu=0.3; h=0.2; q=10000-0.2*24.53
D=E*h**3/(12*(1-nu**2))
def fields(x,y,N=199):
    w=Mx=My=Qx=0.0
    for m in range(1,N+1,2):
        for n in range(1,N+1,2):
            k=(m**2+n**2)*np.pi**2/a**2
            A=16*q/(np.pi**2*m*n)
            wm=A/(D*k**2)
            sx=np.sin(m*np.pi*x/a); sy=np.sin(n*np.pi*y/a)
            w+=wm*sx*sy
            Mx+=D*wm*((m*np.pi/a)**2+nu*(n*np.pi/a)**2)*sx*sy
            My+=D*wm*((n*np.pi/a)**2+nu*(m*np.pi/a)**2)*sx*sy
            Qx+=D*wm*k*(m*np.pi/a)*np.cos(m*np.pi*x/a)*sy
    return w,Mx,My,Qx
print("D",D,"q",q)
for x in [0.501,0.994,1.487,1.995,2.991]:
    w,Mx,My,Qx=fields(x,2.0)
    print(x,"w mm %.2f Mx %.1f My %.1f Qx %.1f -> My_band %.1f Vz_band %.1f"%(w*1000,Mx,My,Qx,Mx*0.15,Qx*0.15))
