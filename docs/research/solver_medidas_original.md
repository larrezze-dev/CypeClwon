# Solver lineal de CYPE 3D: medidas en el original (T-P8-05, cierre de TK-105 / TK-107)

Fecha: 2026-11-12. Original CYPE 3D v2027.a lanzado con la licencia activada por el usuario: **Cálculo > Calcular funciona sin barrera «Permisos necesarios»** (solo apareció «Existen barras sin describir» en una obra sin secciones, y «Esta estructura es un mecanismo» en el caso a propósito inestable). Obras de prueba: `docs/sample_project/sv_c1.ed3`, `sv_c2.ed3`, `sv_c3.ed3`, `sv_c6.ed3` (copias de lo que el original guarda solo en `D:\CYPE Ingenieros\Proyectos\CYPE 3D\nuevo.ed3`, carpeta del propio programa, no tocada) y los textos de importación `sv_c1.txt`... `sv_c6.txt`. Scripts: `tools/sv_lib.sh` (conducción), `tools/sv_portal_check.py` (solver plano independiente usado para atribuir el resultado de C3).

Marcas: **[V]** medido en el original (valor numérico leído en pantalla o decisión por coincidencia numérica con una solución cerrada); **[N]** no medido.

## 1. Método (todo por GUI, sin evasión de licencia)

Obra vacía (asistente) + «Geometría > Importar > Fichero de texto» (nudos y barras) + «Geometría > Barras > Sección» con **Perfil genérico** (Describir > icono 8: campos A, Avy, Avz, Iyy, Izz, It, E, ν, α, γ editables; las propiedades no se recalculan al cambiar la geometría) + Vinculación exterior + cargas + Cálculo > Calcular («No dimensionar perfiles», sin «Comprobar las barras»). Lectura: Cálculo > Desplazamientos / Reacciones / Esfuerzos (leyes «Sólo las barras seleccionadas» + «Consultar valores»: tooltip con N, Vy, Vz, Mt, My, Mz en el punto clicado). Resolución de la lectura: desplazamientos 0,001 mm / 0,001 mRad, fuerzas 0,001 kN, momentos 0,01 kN·m.

Propiedades usadas en todas las obras: A = 100 cm² (0,01 m²), Iyy = 10000 cm⁴ (1e-4 m⁴), Izz = 5000 cm⁴ (5e-5 m⁴), It = 8000 cm⁴ (8e-5 m⁴), E = 210000 MPa, ν = 0,3 (G = 80769,2 MPa), Avz = 40 cm² (0,004 m²), Avy = 40 cm² salvo C6 (80 cm²), α = 1,2e-5, γ = 77,01 kN/m³ (C1) o **0,01 kN/m³ (mínimo admitido; rango 0,01..100000)** en C2-C6 para que el peso propio sea despreciable (q_pp = 1e-4 kN/m).

## 2. Resultados y comparación

| Caso | Lectura en el original [V] | Referencia analítica | Veredicto |
|---|---|---|---|
| **C1** voladizo L = 4 m (nudo 1 empotrado), P = 10 kN en −Z en el extremo, peso propio activo (q = 0,7701 kN/m) | Dz(N2) = **−11,475 mm**; Gy(N2) = 4,201 mRad; Rz(N1) = 13,080 kN; My(N1) = −46,16 kN·m | EB: P L³/3EI + q L⁴/8EI = 10,159 + 1,174 = **11,333 mm**; Timoshenko: + P L/(G Avz) + q L²/(2 G Avz) = 10,283 + 1,174 + 0,019 = **11,475 mm** | **Timoshenko** (+1,25 % sobre EB). Gy = P L²/2EI + q L³/6EI = 3,810 + 0,391 = 4,201 OK |
| **C6** (carga solo P = 10 kN, Avy = 80, Avz = 40; barra 1 en X, L = 4; barra 2 en Y, L = 2; cargas en N2: +X 10, +Y 10; en N3: −Z 10) | N2: Dx = 0,019; Dy = **20,379**; Dz = **−10,283** mm; Gx = **−12,381**; Gy = 3,810; Gz = 7,619 mRad | Dz Timoshenko con Avz: 10,1587 + 0,1238 = 10,2825; Dy: 20,317 + P L/(G Avy) = 20,317 + 0,0619 = 20,379 (con Avz sería 20,441); Gx = T L/(G It) = 20·4/6461,5 = 12,381; Dx = P L/EA = 0,01905 | **Cada plano usa su área de cortante: Avz en flexión xz (Iy), Avy en flexión xy (Iz); torsión = G·It (St. Venant)** |
| **C2** biapoyada L = 6 m (2 barras de 3 m; N1 «Apoyo fijo», N3 «recta X articulada»), q = 10 kN/m global −Z | Dz(N2) = **−8,175 mm**; Gy(N1) = 4,286 mRad; Rz(N1) = 30,000 kN; en X = 2,934 m: Vz = −0,658 kN, My = +44,93 kN·m | EB 8,036 mm; **Timoshenko 8,175000 mm** (doc §8); θ = qL³/24EI = 4,2857 | **Timoshenko exacto** |
| **C3** pórtico plano XZ (pilares 4 m, vano 6 m, bases empotradas, H = 20 kN en +X en N2) | ux N2/N3 = **6,950 / 6,921 mm**; uz = ±0,011; Gy = 0,869 / 0,863 mRad; N1: Rx −10,015, Rz −5,908, My −22,31; N4: Rx −9,985, Rz +5,909, My −22,23 | `tools/sv_portal_check.py`: «Tim pilares Iz, viga Iy» = 6,9497 / 6,9212; 0,8693 / 0,8630; R0 Fx −10,0152, Fz −5,9088, My −22,3124 (las demás variantes difieren: EB todo Iy 4,083; Timoshenko todo Iy 4,233) | **Timoshenko y los pilares flectan con Izz** (ver §3.1) |

Conclusión sobre TK-105(a): **[V] el original aplica deformación por cortante (Timoshenko) con las áreas de cortante de la sección** (coincide a 4 cifras en C1, C2, C3 y C6). `SolverOptions::shearDeformation = true` es el comportamiento del original. Con área de cortante 0 («ignorar») no se midió [N].

## 3. Convenios [V]

### 3.1 Ejes locales y asignación de inercias
- Barra horizontal a lo largo de X: x local = +X, z local = +Z (arriba), y local = +Y (dibujo de ejes: rojo/verde/azul). Flexión en el plano XZ (giro Gy) usa **Iyy y Avz**; en el plano XY (Gz) usa **Izz y Avy**; torsión Gx usa It.
- Barra vertical: ejes dibujados x = +Z (arriba), y = +X, z = +Y. La flexión del pilar en el plano global XZ (giro alrededor de Y global = z local) usa **Izz**: **[V]** por el ajuste numérico de C3 (con Iyy en los pilares no coincide). Barra horizontal a lo largo de Y: x = +Y, y = −X, z = +Z (dibujo).

### 3.2 Esfuerzos (tooltip de «Esfuerzos», ejes locales de la barra en el punto X)
Resultante canónica = acción de la parte [x, L] sobre [0, x] en ejes locales (= −(cargas aplicadas sobre [x, L] más reacciones), vectores y momentos por regla de la mano derecha). El original muestra:

| Esfuerzo | Signo respecto a la resultante canónica | Evidencia |
|---|---|---|
| N | **igual** (tracción positiva) | C6: carga +X en el extremo, N = +10,000; C3: pilar con reacción vertical hacia abajo, N = +5,908 |
| Vy | igual | C6: Vy = +10,000 con carga +Y en el extremo; C3 pilar: Vy = +10,015 |
| Vz | igual | C1: Vz = −11,542 (carga hacia abajo en voladizo); C2: Vz = −0,658 (x = 2,934 m, lado izquierdo con reacción hacia arriba = positivo clásico) |
| Mt | igual | C6: Mt = −20,00 (momento de las cargas de la derecha respecto al eje X = −20) |
| **My** | **opuesto** | C1: My = −21,57 con carga hacia abajo (la resultante canónica es +21,57); C2: My = **+44,93** en flexión positiva (tracción abajo); C6: My = −20,03 |
| Mz | igual | C6: Mz = +20,03; C3 pilar: Mz = +3,80 |

Regla práctica: dMy/dx = −Vz y dMz/dx = −Vy (ambas con signo menos); viga biapoyada con carga hacia −Z tiene **My > 0 (positivo) en el vano**, el voladizo cargado hacia −Z tiene My < 0 en el empotramiento. `fem::applySignConvention` debe invertir solo My respecto a la resultante canónica del clon (hoy identidad; el clon da My < 0 en el vano biapoyado, el original My > 0). TK-107 queda **resuelto**.

El tooltip de esfuerzos interpola puntos discretos de la ley (C2: en X = 2,934 m dio My = 44,93 frente a 44,98 exacto; la desviación 0,1 % es del muestreo de la ley, no del solver; el cortante sí coincide: V = 30 − 10·2,934 = 0,66).

### 3.3 Reacciones y desplazamientos
- Reacciones (Rx, Ry, Rz en kN; Mx, My, Mz en kN·m): **fuerzas y momentos que ejercen los apoyos sobre la estructura, ejes globales**: C1: Rz = +13,080 (hacia arriba), My = −46,16 con la carga en +X hacia abajo; C6: Rx = −10,000, Ry = −10,000, Rz = +10,001 (peso propio mínimo), Mx = +20,00, My = −40,00, Mz = −40,00; equilibrio comprobado (momentos de las cargas respecto al origen: Mx = −20, My = +40, Mz = +40).
- Desplazamientos (Dx, Dy, Dz en mm; Gx, Gy, Gz en mRad): ejes globales; giros por regla de la mano derecha (voladizo cargado hacia abajo a lo largo de +X: Gy positivo; carga que produce torsor negativo: Gx negativo).

## 4. Cargas

- **Peso propio** [V parcial]: la hipótesis «Peso propio» incluye siempre el peso de las barras en el cálculo. Tras marcar «Desactivar» en Carga > Barras > «Peso propio» («Generación de cargas», diálogo global sin selección de barras; reabre con «Desactivar» marcado) los resultados siguieron incluyendo el peso (C1: Rz = 13,080 = 10 + 0,7701·4; C2 con γ = 77,01: Rz = 32,310 = 30 + 0,7701·3). El valor medido es **γ·A por metro de barra**, repartido uniforme (Dz C1 consistente al 4.º dígito). El mínimo de peso específico es 0,01 kN/m³, usado en el resto de pruebas. Áreas medias por tramo con sección variable: [N] (sin cartelas ensayadas).
- **Carga uniforme global en barra inclinada** [V]: nudos (0,0,0)-(4,0,3), L = 5 m, q = 10 kN/m global −Z, empotrada en el origen: **Rz = 50,001 kN y My = −100,00 kN·m** ⇒ la carga global se aplica **por metro de longitud de barra** (no por proyección horizontal, que daría 40 kN). Desplazamiento del extremo (Dx 18,014, Dz −24,079 mm, Gy 7,937 mRad) coincide con la descomposición axial (6 kN/m) + transversal (8 kN/m) con Timoshenko.
- **Temperatura uniforme** [V]: ΔT = 10 ºC sobre la barra voladizo de 4 m ⇒ ΔDx = **0,480 mm** = α ΔT L con α = 1,2e-5 (sin efecto en Dz).
- **Temperatura variable** [V]: T1 = 20 (cara −Z, ejes locales, dirección «Z+», por defecto), T2 = 0 (cara +Z) en la misma barra ⇒ aumento de Dx de 0,480 mm (se aplica la **media (T1+T2)/2 = 10 ºC como incremento uniforme**), extremo hacia **+Z**: +38,400 mm y Gy −19,200 mRad ⇒ curvatura κ = 4,8e-3 1/m = α (T1−T2)/h con **h = 50 mm = Canto inferior (3) + Canto superior (4) de la geometría del perfil genérico** (25 + 25 mm; no se deriva de A ni de I). Con el botón «Z+» (por defecto) el dibujo del diálogo coloca T2 en la cara +z y T1 en la −z; la cara más caliente (inferior, T1 = 20) se alarga más y el extremo sube (+Z). Dirección Y+ y ángulo libre: [N].
- Rangos de los diálogos (tooltips): carga uniforme 0,001..10000,000 kN/m; temperatura −100..1000 ºC (2 decimales); carga de nudo 0,01..49000,00 kN.

## 5. Empotramiento parcial de extremo de barra [V]

Diálogo «Empotramiento en extremos» (Propiedades > Barras > Empotramiento; selección LMB + RMB): por plano (xy, xz, yz) y por extremo (Origen / Extremo): casilla «Asignar», radio «**Coef. empotramiento**» (campo 0..1, 3 decimales, defecto 1,000) o «**Rigidez rotacional**», y el tooltip de la barra muestra «Empotramiento en extremos: Origen: xy: 1.000 / xz: 0.500 / yz: 1.000».

Ensayo: barra inclinada del §4 con coeficiente en el plano xz del origen. Momento en el extremo = 100 kN·m (estático, reacción −100). Giro del muelle leído como Gy del nudo 1 (0 con empotramiento total):

| c (xz, origen) | θ muelle leído | K = M/θ | c/(1−c)·k_ii (k_ii = (4+φ)EI/((1+φ)L)) | c/(1−c)·4EI/L (EB) |
|---|---|---|---|---|
| 0,5 | 6,090 mRad (a partir de Gy(N2) 14,027 − 7,937 = 6,090 y Dx/Dz del extremo = giro rígido 3θ, 4θ) | 16420 kN·m/rad | **16418** | 16800 |
| 0,8 | 1,523 mRad (Gy en N1) | 65660 kN·m/rad | **65672** | 67200 |

con EI = 21000 kN·m², L = 5 m, φ = 12EI/(G Av L²) = 0,0312 (Avz = 40 cm²). **Ley verificada con dos puntos: K = c/(1−c) · k_ii**, donde k_ii es el término diagonal de rigidez al giro del extremo del elemento **con deformación por cortante** ((4+φ)/(1+φ)·EI/L, no 4EI/L). El clon usa 4EI/L (`fem/frame_builder.cpp`): hay que sustituirlo por la forma con φ (diferencia 2,3 % en este caso). Planos xy/yz, extremo final y modo «Rigidez rotacional»: [N] (la rigidez rotacional se interpreta como K directo, no medido).

## 6. Mecanismo y límite de desplazamientos

- **Mecanismo** [V]: base de la barra solo con «Apoyo fijo» (giros libres) y una carga ⇒ ventana «Proceso de cálculo… Resolución del sistema de ecuaciones» con aviso: «**Esta estructura es un mecanismo. Revise los datos introducidos. / Giro alrededor del eje Y / El error se encuentra en el punto de coordenadas: (x = 0.000000, y = 0.000000, z = 0.000000) / Alguna de las barras de la estructura no está vinculada correctamente en sus extremos, es decir, no es estable.**» (identifica el gdl con pivote nulo, su tipo y el nudo). Captura `sv_t1.png`. El mensaje del código (#214001 «No es posible calcular el caso de carga...») no es el que ve el usuario en este caso.
- **Umbral de «desplazamientos excesivos»**: [N] no medido (requiere buscar una estructura casi-mecanismo con carga máxima 49000 kN; queda en TK-105).

## 7. Otras observaciones útiles

- «Cálculo» abre «Informe final de cálculo»: «La obra ha sido calculada sin realizar la comprobación de las barras.» (con «Comprobar las barras» desmarcado).
- Calcular con barras sin sección: error «Existen barras sin describir.» (toda barra necesita sección).
- Los paneles flotantes de resultados («Propiedades - Desplazamientos/Reacciones/Esfuerzos/Hipótesis vista») aparecen maximizados la primera vez en cada obra; **no** hay que usar el botón de restaurar de la ventana principal (se desmaximiza el programa).
- El original guarda las obras nuevas por sí mismo en `D:\CYPE Ingenieros\Proyectos\CYPE 3D\nuevo.ed3` (y `nuevo.ed$`): carpeta del programa, solo leída/copiada, no modificada por el investigador.

## 8. Capturas (docs/screenshots, añadidas con `git add -f`)

`sv_e5.png` C1 desplazamientos; `sv_e7.png` C1 reacciones; `sv_f4.png` C1 esfuerzos; `sv_g5.png` temperatura uniforme; `sv_h3.png` temperatura variable; `sv_l4.png` C2 desplazamientos; `sv_m2.png` C2 esfuerzos; `sv_n5.png`, `sv_n6.png`, `sv_n7.png`, `sv_o2.png` C3; `sv_p5.png`, `sv_p6.png`, `sv_p7.png` C6; `sv_q3.png`, `sv_q4.png` (inclinada, sin empotramiento parcial); `sv_r3.png` (c = 0,5); `sv_s3.png` (c = 0,8); `sv_r4.png` diálogo «Empotramiento en extremos»; `sv_t1.png` aviso de mecanismo; `sv_k5.png` diálogo «Generación de cargas».

## 9. Segunda pasada de medición (2026-11-12): TK-108 y pendientes de TK-105

Original CYPE 3D v2027.a con la licencia activada por el usuario (sin barrera de licencia). Obras (copias en `docs/sample_project/`; el original **guarda la obra al calcular y al aceptar diálogos**, por eso `sv_c1.ed3`, `sv_c2.ed3` y `sv_c6.ed3` han cambiado respecto a §1). Scripts nuevos: `tools/tk_lib.sh` (apoyos, cálculo, lectura de nudos). Marcas: **[V]** medido, **[N]** no medido, **[R]** regla deducida que encaja con todos los puntos medidos (no verificada con código).

### 9.1 TK-108: modos rígidos no excitados y aviso de mecanismo

Lectura de resultados: panel «Desplazamientos», clic en el nudo = tooltip con Dx..Gz (0,001 mm / 0,001 mRad). Diálogo «Vinculación exterior» (iconos fila 1: «?», empotrado, …; fila 2: libre/«Apoyo fijo» (751,421), «recta X articulada» (797,421) …). **«Apoyo fijo» = Dx, Dy, Dz fijos y giros libres; «recta X articulada» = Dy, Dz fijos, Dx libre sobre la recta X y giros libres** (tooltip: «Desplazamiento libre sobre una recta en la dirección X con giros sin coacción»).

| Caso | Resultado [V] |
|---|---|
| C2 (2 barras X de 3 m, q = 10 kN/m global −Z): N1 «Apoyo fijo», N3 «recta X articulada» | **Calcula sin aviso.** N2: Dz = −8,175 mm, Gx = 0,000; N1 Gy = +4,286, N3 Gy = −4,286 mRad. El giro de torsión (modo nulo) sale **exactamente 0,000** en N1, N2 y N3 |
| C2 con N3 también «Apoyo fijo» | Igual: sin aviso, mismos valores, Gx = 0,000 en los tres nudos (capturas `tk108_E2.png`, `tk108_E3.png`) |
| Obra en L (N1 (0,0,0), N2 (4,0,0), N3 (4,2,0); barra 1 en X, barra 2 en Y; cargas en N2 +X 10 y +Y 10 kN, en N3 −Z 10 kN): N1 **empotrado**, N2 «recta X articulada» | Calcula sin aviso: N2 Dx = 0,019 mm, Gx = −12,381 mRad (como en §2 C6) |
| Misma obra en L: N1 **«Apoyo fijo»**, N2 «recta X articulada» (el giro de toda la estructura alrededor del eje N1–N2 es un modo nulo acoplado con el voladizo N2–N3) | **Aviso**: «Esta estructura es un mecanismo. Revise los datos introducidos. / **Desplazamiento según el eje Z** / El error se encuentra en el punto de coordenadas: (x = 4.000000, y = 2.000000, z = 0.000000) / Alguna de las barras de la estructura no está vinculada correctamente en sus extremos, es decir, no es estable.» El «Informe final de cálculo» dice «Esta estructura es un mecanismo. Revise los datos introducidos.» y los botones de resultados quedan inactivos (`tk108_B1.png`, `tk108_B2.png`) |
| La misma, **borrando la carga de N3** (sin carga que excite el modo; las cargas de N2 no lo excitan) | **Igual aviso, mismo gdl (N3, Dz)** (`tk108_D2.png`) |

Conclusiones:
1. **El aviso no depende de que la carga excite el modo**: el modo de giro acoplado avisa con y sin carga torsora; el modo de torsión aislado (barras colineales, solo rigidez GJ/L) **no** avisa y devuelve giros 0.
2. **[R]** Regla compatible con los cuatro puntos: tras factorizar, un modo nulo cuyo soporte son solo giros alrededor del eje común de barras colineales (Gx de los nudos de una cadena recta) se **fija a cero sin aviso**; cualquier otro modo nulo (acoplado a desplazamientos o a otros giros) da «Esta estructura es un mecanismo». Para el clon: `solveLinear` debe regularizar el torsor aislado (no `Mechanism`) y mantener `Mechanism` para el resto, con carga o sin ella. (El solver del original es dispersivo directo: mensajes «Displacement on null pivot…», «pfsfct(): The diagonal entry is zero»; la fijación a 0 del caso aislado es coherente con una detección de pivote nulo con «ignorar» para esos grados de libertad, pero **no se ha leído en el código**.)
3. **Qué gdl nombra el aviso (TK-105 e)**: puntos medidos: voladizo con base «Apoyo fijo» y carga en la punta → **N1 / Giro alrededor de Y** (§6); obra en L anterior → **N3 / Desplazamiento según Z** (N3 = nudo libre del voladizo de 2 m). Ninguna regla simple los reproduce: «gdl de mayor amplitud del modo» daría N2/Dz en el voladizo; «primer nudo» daría N1/Gx en la L. Depende del orden de eliminación del solver (**[N]**, regla abierta; el clon puede nombrar cualquier gdl del modo, el texto es el mismo).

### 9.2 TK-105 a) Umbral de «desplazamientos excesivos» [V]

Voladizo de 4 m (N1 con Gy «Elástico», K variable en kN·m/rad; todo lo demás fijo; carga P = 10 kN en −Z en el extremo ⇒ giro del nudo N1 ≈ 40/K rad, Dz del extremo ≈ 160/K m). Aviso mostrado en el informe: «Se han obtenido desplazamientos demasiado grandes. Puede consultar los mensajes de aviso correspondientes mediante la opción 'Mostrar/Oculta incidencias' del menú 'Cálculo'.» (los botones de resultados quedan inactivos).

| K (kN·m/rad) | Giro N1 (rad) | Dz extremo (m) | Resultado |
|---|---|---|---|
| 100; 30 (leído: Dz = −5343,723 mm, Gy(N2) = 1337,170 mRad); 27; 26,8; 26,7; 26,6; **25,6** | 0,40 … **1,5625** | 1,6 … 6,25 | **Calcula** |
| **25,4**; 25; 20; 10; 1 | **1,5748** … 40 | 6,30 … 160 | **«Desplazamientos demasiado grandes»** |
| Dx elástico K = 1,5 kN/m (carga X 10 kN, Dx ≈ 6,67 m) | 0 | – | **Calcula** (un desplazamiento de 6,67 m no avisa) |

El umbral está entre 1,5625 y 1,5748 rad (nudo N2: 1,5663–1,5786) y encierra **π/2 = 1,5708 rad**: **[V] el aviso salta cuando algún giro de nudo supera π/2 (90°)**; la traslación Dx = 6,67 m no avisa, por lo que no hay un límite de desplazamiento lineal del orden de 6 m (límite lineal mayor: **[N]**, la prueba con K = 0,01 kN/m no se pudo completar porque el diálogo no admite ese valor). Capturas `thr_K*.png`.

### 9.3 TK-105 c) Temperatura variable con dirección Y+ y ángulo libre [V]

Diálogo «Incremento de temperatura variable»: Valor 1 (T1) y Valor 2 (T2) en ºC, «Ejes locales de la barra», tres iconos de «Dirección y sentido de la variación»: (1) T1|T2 a lo largo del eje y local (**Y+**), (2) T2 arriba / T1 abajo, a lo largo de z (**Z+**, por defecto), (3) **ángulo libre**: aparece el campo «Ángulo» (grados, defecto 0,000, rango −180..180). Voladizo de 4 m, T1 = 20, T2 = 0, α = 1,2e-5 (todas las lecturas son la contribución de esta carga sola, resta de la base conocida):

| Caso | Extremo | Giro |
|---|---|---|
| Icono Z+ (ya medido) y **ángulo 0°** | Dz = **+38,400** mm | Gy = −19,200 mRad |
| Icono **Y+**, ancho = 100 mm (ancho izq. 50 + dcha. 50), canto = 50 mm | Dy = **+19,200** mm | Gz = +9,600 mRad |
| Icono Y+, anchos 25 + 25 (= 50 mm) | Dy = +38,400 mm | Gz = +19,200 mRad |
| **Ángulo 90°** | Dy = **−38,400** mm | Gz = −19,200 mRad |
| **Ángulo 45°** (anchos 100 mm, cantos 50 mm) | **Dz = +27,153, Dy = −13,576 mm** | Gy = −13,577 mRad (lectura −9,767 menos 3,810 de P), Gz = −6,788 mRad |

Reglas: la curvatura en el plano xz es κz = α (T1 − T2) cos θ / hz con **hz = Canto inferior (3) + Canto superior (4)**; en el plano xy κy = α (T1 − T2) · (±sin θ) / hy con **hy = Ancho izquierda (1) + Ancho derecha (2)** (comprobado con anchos distintos de los cantos: Dy se reduce a la mitad al duplicar el ancho). Con el icono Y+ el extremo se mueve hacia +Y (T1 en −y); con ángulo θ la dirección es la rotación de **+z alrededor del eje x local** un ángulo θ (θ = 90° lleva a −y): Dz = α ΔT L²/(2 hz) cos θ y Dy = −α ΔT L²/(2 hy) sin θ (45°: 38,4·cos 45° = 27,153 y 19,2·sin 45° = 13,576, exactos). La media (T1+T2)/2 se sigue aplicando como incremento uniforme (Dx +0,480 mm por cada carga variable de 20/0 °C, como en §4).

### 9.4 TK-105 d) Empotramiento parcial: planos xy/xz, extremo final, «Rigidez rotacional» [V]

Diálogo «Empotramiento en extremos» (sin cambios de §5). Voladizo C1 (EIz = 10500 kN·m², Avy = 40 cm², G = 80769 MPa, L = 4 m, φ = 12EI/(G·Av·L²) = 0,02438, k_ii = (4+φ)/(1+φ)·EI/L = 10312 kN·m/rad):

| Ensayo | Predicción | Original [V] |
|---|---|---|
| Plano **xy**, Origen, c = 0,5, carga Y 10 kN en el extremo: K = c/(1−c)·k_ii | Dy = 20,317 + 0,124 + 15,515 = **35,956 mm** (con 4EI/L: 35,679) | Dy = **35,956 mm**, Gz(N1) = 3,879 mRad (θ = 40/10312) ✓ |
| «**Rigidez rotacional**» K = 5000 kN·m/rad en xy Origen | K directo: Dy = 20,441 + 4·8 rad = **52,441 mm** | Dy = **52,441 mm**, Gz(N1) = 8,000 mRad ✓ (el valor por defecto que muestra el campo es **500000,00 kN·m/rad**) |
| **Extremo final**, plano xz, c = 0,5 en la barra 1 de C2 (el muelle queda en el extremo de la barra 1 junto al nudo N2; resolución independiente con vigas de Timoshenko y muelle K = c/(1−c) k_ii) | N2 Dz = −10,739; Gy(N1) = 5,140; Gy(N2) = 0,855 (con 4EI/L: −10,586; 5,089; 0,804) | Dz(N2) = **−10,739 mm**, Gy(N1) = **5,140**, Gy(N2) = **0,855 mRad** ✓ |

Conclusión: **la ley K = c/(1−c)·k_ii con k_ii de Timoshenko (con el área de cortante del plano correspondiente) vale en los planos xy y xz y en ambos extremos**; «Rigidez rotacional» es K directo en kN·m/rad. Plano **yz** (torsión): el desplegable solo ofrece **0,000 o 1,000** (articulado/empotrado; no hay valores intermedios para c; «Rigidez rotacional» y su valor en yz: [N]). Cálculo analítico usado (Timoshenko + muelle): `docs/research/data/chk_spring.py`.

### 9.5 TK-105 g) y h)

- **Área de cortante 0 no es admisible**: «El valor de 'Área de cortante (Avz)' debe estar entre 0.01 y 1000000.00.» (cm²; igual para Avy). No existe la opción «ignorar»: el original siempre aplica Timoshenko; para flexión pura se puede introducir 1 000 000 cm² (mín. 0,01). Rangos vistos en el diálogo Describir: ancho izquierda/derecha «entre 0.1 y 10000.0» mm; carga de nudo 0,01..49000 kN. `avz_tip.png`, `avz_c.png`.
- **Interruptor de peso propio por barra**: no se encuentra en la GUI (revisados Describir, Carga > Barras > Peso propio = diálogo global «Generación de cargas», Propiedades > Editar es de diafragmas). El peso se anula en la práctica con peso específico mínimo 0,01 kN/m³. `sv_k5.png`.

### 9.6 Pendiente de esta pasada

- **TK-105 b)** peso propio con áreas medias por tramo (sección variable) y **f)** barra vertical descendente / signo del ángulo de giro: no medidos (requieren una obra nueva con sección variable y otra con barra hacia −Z; ver `.tickets.md`).
- Aviso de mecanismo: regla de nombre del gdl (§9.1.3).
- **Láminas** (`elemento_lamina_bandas.md` §9): no medidas en esta pasada (ver `.tickets.md`).

### 9.7 Notas de conducción (original)

- El panel flotante de resultados/«Hipótesis vista» aparece **maximizado solo la primera vez**; `sv_panel` (clic en 1848,13) solo sirve entonces. Si el panel ya está en la franja izquierda, ese clic cae en el botón «restaurar» de la **ventana principal** y la desmaximiza (se recupera con su botón de maximizar en la barra de título de la ventana, que queda a la derecha). Comprobar con una captura antes de usar `sv_panel`.
- El diálogo «Cálculo» recuerda «Comprobar las barras» desmarcado solo mientras todos los cálculos terminan bien; tras un aviso grave vuelve a marcarse y hay que desmarcarlo (817,565) antes de Aceptar.
- Teclear un número con el foco en la vista 3D abre la «Calculadora» de coordenadas; cerrarla (X) y no seguir tecleando.
- Borrar una carga de barra: «Carga > Barras > Borrar» + clic en el dibujo de la carga (cada plano de temperatura se borra pinchando dentro de su plano) + RMB; `Ctrl+Z` deshace borrados anteriores.
- Los tooltips de nudo de «Vinculación exterior» muestran el apoyo con K redondeado a entero (K = 1,5 se ve «2 kN/m»).
