# Diafragmas rígidos, ligaduras, apoyos elásticos y oblicuos en el solver (T-P8-03, TK-106 / TK-110)

Fecha: 2026-11-12. Vía: REA primero (`cyopensees.dll`, funciones `cyosvincext_*`, `cyosrel2nodos_*`, `cyosdiafrigid_*`, `cyosnudo`, `cyosrelac`, `cyostbecrelac`), luego medición en el original (secciones marcadas **[V]**). Marcas: **[REA]** leído en el código (pseudocódigo de Ghidra, no es texto original), **[V]** medido en el original, **[N]** no medido / no leído.

## 1. Arquitectura de las relaciones entre gdl [REA]

- Todas las ligaduras multi-gdl se convierten a **ecuaciones de relación** `dofRelation tag_esclavo idx n (nudo gdl coef)*` (`FUN_180093480`, `cyostbecrelac`) y se **eliminan por expansión** (método de transformación; `CYPETransformation`), no por penalización. Solo si la variable de entorno `CYOPENSEES_WRITE_EXPANDED_RELATIONS` está definida (`FUN_180090aa0`) se vuelcan al script Tcl como texto (`equalDOF`, `dofRelation`) en vez de expandirse internamente. Con o sin ella la cinemática es la misma.
- Constantes (`cyosrelac_cte_*`): **infinita = 1e10**, infinita reducida = 1e8, ínfima = 1e-3 (kN/m, kN·m/rad). La rigidez «infinita» solo se usa en los apoyos oblicuos (penalización, §4), en ligaduras genéricas con muelle y en relaciones elásticas; los diafragmas y `equalDOF` son rígidos exactos.
- Materiales de muelle (`cyosrel2nodos_crea_material_*`): `Elastic`, `ElasticPPGap`, **`ENT` (solo compresión)**, `Hardening`, `Concrete01` compresión, `Multilinear`, `PySimple1`. Elementos: `zeroLength` (1, 2, 3 o 6 gdl, `-orient x y z`), `twoNodeLink`, `rigidLink beam|bar`.

## 2. Diafragma rígido [REA]

Función `FUN_18008d280` (cyosrelac.c:0x696, llamada por `FUN_180075200` = función de nudo en cyosnudo.c:0x720), parámetros: nudo maestro (nudo de diafragma generado, tag y coordenadas x_m, y_m, z_m), nudo esclavo (tag y x_s, y_s, z_s), bandera «traslaciones» A y bandera «giro» B.

1. Aserción: `|z_m − z_s| < 1e-3`: **un diafragma es siempre horizontal (plano XY, eje 3 = Z)**; todos sus nudos deben estar a la misma cota.
2. Si A: Dx_s = Dx_m − (y_s − y_m)·Gz_m ;  Dy_s = Dy_m + (x_s − x_m)·Gz_m (`dofRelation` con coeficientes 1 y −Δy / +Δx).
3. Si B: Gz_s = Gz_m.
4. **Dz, Gx, Gy del esclavo quedan libres** (el diafragma no coarta flexión fuera de plano; equivale a `rigidDiaphragm 3 maestro esclavo` de OpenSees, que usa la otra rama del código).
5. Por nudo, `FUN_180075200` itera los **grupos de giro** del nudo (el nudo de cálculo se duplica por cada grupo de barras con la misma coacción de giro; un nudo con barras articuladas tiene varias copias): A solo en el grupo 0 (nudo básico); B en el grupo 0 y en los grupos cuyo flag «coeficiente de empotramiento parcial» es falso. Si algún grupo es de empotramiento parcial, el nudo adicional `tag_nudo_coef_empotramiento_giro` recibe A y B. Es decir: **el giro Gz de una barra articulada en un nudo de diafragma no se liga al giro del diafragma** salvo que sea del grupo básico.
6. El nudo maestro depende solo de la geometría: como las relaciones son de sólido rígido, **la posición del maestro no altera los resultados de los nudos**; solo interviene al aplicar las cargas de diafragma (`cyosdiafrigid_append_carga_global`: carga global Fx, Fy aplicada en un punto (x, y), transferida al maestro con el momento correspondiente M = x·Fy − y·Fx respecto al maestro).
7. Mensaje del proceso de cálculo (id 214013): «Procesando diafragmas rígidos» (fr: «Traitement des diaphragmes rigides»).

## 3. Ligaduras entre nudos (`equalDOF`) [REA]

`FUN_18008fde0` (cyosrelac.c:0x15a): `equalDOF maestro esclavo gdl…` (lista de gdl no vacía, aserción `num_gdls_relacionar > 0`); en modo expandido, una ecuación u_esclavo,gdl − u_maestro,gdl = 0 por gdl con coeficiente 1. Cuál es maestro no cambia el resultado. Ligadura rígida de 6 gdl, de 3 gdl (desplazamiento) o de 3 gdl (giro) y de 1 gdl dado: `cyosrel2nodos_crea_relacion_rigida_*`; elástica entre nudos: `relacion_elastica` (`Elastic`, `ENT`...).

## 4. Vinculación exterior [REA]

Estructura `cyosvincext` (0x7c bytes): tipo 0 = **por gdl** (`crea_por_gdl`, `crea_libre`), 1 = **recta** (`crea_coaccion_desps_en_recta`), 2 = **plano** (`crea_coaccion_desps_en_plano`). Cada gdl lleva una «coacción» (0x20 bytes): `coaccionado` (short), `elástica` (short), `K` (double), `sentido` (int), lista de hipótesis con desplazamiento prescrito y sus valores. Aserción de creación: **`k_coaccion_elastica > 0`** si el gdl es coaccionado y elástico.

- **Tipo 0**: `fix tag f1..f6` con 1 en los gdl coaccionados rígidos (`FUN_180088880`); un gdl elástico NO se fija: va con muelle (`zeroLength` con `Elastic` o `ENT`, `-dir`). Asientos: `sp tag gdl valor;` **por hipótesis** (`FUN_180087d80`: solo se emite en las hipótesis que tienen valor prescrito; valores en m / rad).
- **Plano y recta** (apoyos oblicuos): se crea un **nudo de apoyo duplicado** en la misma posición con los giros fijados según las coacciones de giro (`fix` sobre el nudo duplicado); los **giros del nudo estructural se ligan a los del duplicado** (`equalDOF` de los gdl 4-6, `FUN_18008e5b0`), y las **traslaciones se coartan con un muelle `zeroLength` de rigidez 1e10 (`cyosrelac_cte_rigidez_infinita`) entre el nudo y el duplicado**, en 1 dirección para el plano (normal dada) y en 2 direcciones perpendiculares (producto vectorial) para la recta. Aserción: Dx, Dy, Dz del propio nudo deben estar libres (no coaccionados) para este tipo. **Las oblicuas son por penalización 1e10, no por transformación exacta**: reacciones con precisión ~1e-10 relativa, irrelevante para el clon (se puede usar ligadura exacta con el mismo resultado numérico a 6 cifras).
- **Sentido** (`ENT`, `uniaxialMaterial ENT tag K`, «Elastic-No-Tension»): el muelle elástico con «Sentido» + o − es un material **solo compresión** (no lineal por hipótesis) cuya orientación la da el `-orient`. Ver §6 para la medición.

## 5. Cálculo no lineal ligado a los muelles con «Sentido» [V GUI + REA]

- [V] En cuanto la obra tiene un apoyo elástico con «Sentido» distinto de «Ambos» (único elemento no lineal de barras/nudos de esta tarea), el diálogo **Cálculo** muestra activa la casilla «**Análisis no lineal de tensión / deformación**» y, debajo, «**Actualizar las combinaciones no lineales tras el análisis lineal**» («El programa generará automáticamente las combinaciones no lineales tras el análisis lineal.»), y el ribbon Cálculo tiene el grupo «Combinaciones no lineales» (botón «NL»). Es decir: el análisis lineal resuelve todas las hipótesis y combinaciones con el muelle bilateral; después se **generan combinaciones no lineales** que se resuelven aparte (con `ENT`, solo compresión; `CYOSGSMOD_FACTOR_ALWAYS` fuerza refactorizar). **Consecuencia para el clon**: los resultados con «Sentido» no se obtienen por superposición de hipótesis; hace falta un caso de carga combinado por combinación no lineal con iteración de conjunto activo (muelle activo si comprime, retirado si tracciona) y refactorización cuando cambia el conjunto.
- Mensajes del solver no lineal (ids de `cyopensees.dll`): 328573 «Con las condiciones impuestas, no se alcanza la convergencia para el caso de carga '%s'. Revise las condiciones del modelo.»; test de convergencia `EnergyIncr 1.e-8` 10 iteraciones, algoritmo `Newton`.

## 6. Mediciones en el original

(ver siguientes apartados)

### 6.1 Lo medido en la GUI del original [V]

Capturas: `docs/screenshots/dg_a3.png` (Vinculación exterior > Genérica con Dx, Dy «Fijo» y Dz «Elástico»), `dg_a4.png` (Sentido = Positivo), `dg_b2.png` (informe), `dg_b4.png` (tooltip del aviso).

- Panel «Genérica»: grupos Desplazamientos (radios Independientes / Sobre una recta / Sobre un plano; Dx, Dy, Dz con Libre / Fijo / Elástico) y Giros (Gx, Gy, Gz con Libre / Fijo / Elástico). Al marcar «Elástico»: campo **K** (entero, defecto 100000), unidad **kN/m** (giros kN·m/rad) y combo **Sentido** con Ambos / Positivo / Negativo. Coordenadas con el diálogo abierto sobre el nudo (ventana en 660..1260 x 315..727): icono «?» (704,376), Dx Fijo (890,479), Dy Fijo (890,510), Dz Elástico (937,541), K (1042,543), Sentido (1205,546), Aceptar (717,704).
- **Conducción**: el combo Sentido SÍ se cambia por teclado: clic en el campo K, `Ctrl+A`, valor, `{TAB}` (el foco pasa al combo sin abrir la lista) y `{DOWN}` = Ambos -> Positivo -> Negativo (captura `dg_a4.png`). Los combos solo fallaban al abrir la lista con clic.
- El diálogo **Cálculo** cambia de aspecto y posición cuando hay elementos no lineales: casilla «Actualizar las combinaciones no lineales tras el análisis lineal»; Aceptar quedó en (746,740) y en (810,739) según la sesión (capturar antes de pulsar).
- Con un apoyo elástico Dz con Sentido «Positivo» y carga hacia -Z el informe dio «**Se han obtenido desplazamientos demasiado grandes.**» (aviso con triángulo amarillo sobre la barra, `dg_b4.png`), pero la obra de partida (`sv_c1.ed3` de pruebas anteriores) arrastraba muelles de giro/Dx y cargas térmicas de la §9, así que **el resultado no es concluyente** (la obra se descartó). Es indicio, no medida, de que un muelle con Sentido que no recibe compresión se retira y deja el modelo cerca de un mecanismo/«desplazamientos demasiado grandes» si no hay otra restricción.

## 7. Implementación propuesta para el clon (T-P8-03)

1. **Ligaduras y diafragmas por transformación** (eliminación de gdl esclavos, `u = T u_r`) en `linear_solver`; los apoyos fijos/prescritos ya eliminan gdl. Estructuras nuevas en `frame_model.h`: `std::vector<FemEqualDof> equalDofs {master, slave, mask[6]}`; `std::vector<FemDiaphragm> diaphragms {masterId, slaveIds[], translations/rotation flags}`; el maestro se crea como `FemNode` generado en el centroide de los nudos (cota media; posición irrelevante para los nudos, ver §2.6), con Dz, Gx, Gy bloqueados o aislados según la regla de gdl sin rigidez que ya tiene `lockedDofs`.
2. **Reglas del diafragma** (§2): Dx_s = Dx_m − (y_s − y_m) Gz_m; Dy_s = Dy_m + (x_s − x_m) Gz_m; Gz_s = Gz_m solo si el extremo de barra no es un grupo de giro liberado/parcial; Dz, Gx, Gy libres; cota igual (1e-3) o error interno (la GUI media las Z; `diafragmas.md` §2); un nudo con vinculación exterior o ligado no puede estar en un diafragma (#375640, #404575, #375639), de modo que el solver no necesita resolver ese conflicto.
3. **Cargas de diafragma** (Fx, Fy en (x, y)): se aplican al nudo maestro como fuerza Fx, Fy y momento Mz = (x − x_m)·Fy − (y − y_m)·Fx [REA, signo por analogía con la regla de relaciones; [N] sin medir].
4. **Muelles lineales** (Sentido «Ambos»): ya soportados (`FemNode::spring`, K en kN/m y kN·m/rad, medido en §9.2 del documento del solver). Falta el adaptador desde `NodeSupports`.
5. **Muelles con Sentido ±** (`ENT`): bucle de conjunto activo por caso (combinado) sobre la hipótesis; resultados no superponibles (las combinaciones no lineales son casos de carga propios, ver §5); sin convergencia -> mensaje 328573.
6. **Oblicuas (recta/plano)**: ligadura exacta n·u = 0 (plano) o u ∥ d (recta, 2 ecuaciones n1·u = n2·u = 0 con n1, n2 perpendiculares a d) sobre el nudo; equivale a la penalización 1e10 del original dentro de la precisión de lectura (reacciones a 0,001 kN). Rotación del nudo libre/fija según las coacciones de giro de la vinculación.
7. **Vinculación interior** (TK-106): grupos de giro por nudo = copias del nudo con los giros de las barras de ese grupo; relación entre copias con `equalDOF` de Dx, Dy, Dz y los giros del grupo básico; empotramiento parcial: nudo extra `tag_nudo_coef_empotramiento_giro` con muelle de giro (ley K = c/(1−c)·k_ii medida en §5/§9.4 del documento del solver).

## 8. Abierto y receta de medición

| Tema | Por qué sigue abierto | Receta (usar «Sentido» por teclado, ver §6.1) |
|---|---|---|
| Ley de «Sentido» (¿ENT de compresión pura? ¿signo de Positivo/Negativo respecto a Dz/ejes globales?) | La primera obra estaba contaminada; la importación limpia falló por el arranque (Calculadora abierta por teclas sueltas) | Obra limpia `sv_c1.txt` + `sv_desc 100 40 40 10000 5000 8000 210000 0.3 0.01`; N1 empotrado; N2 genérica Dx, Dy Fijo, Dz Elástico K = 1000 + Sentido Positivo; carga 10 kN en N2 hacia -Z. Esperado si el muelle trabaja: Dz = 5,04 mm (muelle activo, con Timoshenko 10,283·984/(984+1000) aprox.); si se retira: 10,283 mm. Repetir con Negativo y con carga +Z. Leer N2 Dz y Rz(N2) |
| Diafragma: Dz/Gx/Gy libres y reparto por rigidez | No se llegó a abrir Propiedades > Diafragmas > Editar/Diafragma por script | Tres pilares verticales idénticos (0,0), (6,0), (6,3), cota 4 m, nudos superiores en D1; carga de diafragma 30 kN en X en (x, y) fuera del centro; leer Dx, Dy, Gz de los tres nudos superiores (deben cumplir la rotación rígida) y las reacciones; predicción por `chk_diaphragm.py` (por escribir): k = 1/(L³/3EI + L/GAv) por dirección y plano de flexión (Izz para flexión en XZ) |
| Ligaduras Dx/Dy/Dz | Misma razón | Dos voladizos de 4 m en (0,0) y (0,3), cargas distintas en X en cada punta, ligar Dx de las puntas: Dx iguales, suma de reacciones = suma de cargas |
| Oblicuas (plano/recta de usuario) | Idem | Plano de normal (1,0,1)/√2 en N2 de un voladizo: la reacción debe ir según la normal; plano con normal exacta +Y para ver si el original falla por el vector orient |
| Vinculación interior parcial (coeficiente de grupo de giro) | No medida | Voladizo con nudo intermedio y empotramiento parcial c = 0,5 en un grupo |
| TK-110 (aristas de lámina: elástico por unidad de longitud, oblicuo, interior, excentricidad) | Medición de reparto a nudos de malla costosa; hooks REA: `i_es_lamina_con_comportamiento_balasto_no_lineal` (aserciones del balasto lineal en `element ShellCype`), aviso #64886 (aristas con distinta vinculación exterior: nudo sin coacción) | Placa 4x4 con arista elástica (K por metro) y lectura de reacciones por nudo de malla; ver `elemento_lamina_bandas.md` |

## 10. Mediciones en el original, segunda pasada (2026-11-12, RE-ANALYST) [V]

Obra limpia nueva (asistente «Obra vacía» + `sv_c1.txt` importado + Describir genérico A = 100, Avy = Avz = 40, Iyy = 1e4, Izz = 5e3, It = 8e3, E = 210000, ν = 0,3, γ = 0,01 kN/m³). Capturas `docs/screenshots/rr_*.png`; conducción `tools/rr_lib.sh` (usa `tools/da.sh`). Convenios de lectura: Dz del extremo en mm, tooltip de «Desplazamientos» en la barra.

### 10.1 Muelle con «Sentido» [V]

Voladizo N1–N2 (L = 4 m, N1 «Empotrado»), N2 «Genérica»: Dx, Dy Fijo; **Dz Elástico K = 1000 kN/m + Sentido**; giros libres; carga de nudo 10 kN en N2 (hipótesis «Peso propio», por defecto −Z; +Z con el botón 3). Rigidez de punta de la barra sola = 10/10,283 mm = 972,5 kN/m (Timoshenko, §2 del documento de medidas), luego con el muelle activo Dz = 10/(972,5 + 1000) = **5,070 mm**, con el muelle retirado **10,283 mm** (la estimación 5,04 de §8 era aproximada).

| Sentido | Carga | Análisis **lineal** (hipótesis simple «Peso propio») | Combinación **no lineal** `[1] PP` | Interpretación |
|---|---|---|---|---|
| Positivo | −Z | Dz = **−5,070**, Gy = 1,878 mRad | Dz = **−10,283**, Gy = 3,810 mRad (muelle retirado) | muelle inactivo |
| Negativo | −Z | – (no leído) | Dz = **−5,070**, Gy = 1,878 | muelle activo |
| Negativo | +Z | – | Dz = **+10,282**, Gy = −3,809 (el 1 μm de diferencia es el peso propio −Z de la barra) | inactivo |
| Positivo | +Z | – | Dz = **+5,070**, Gy = −1,878 | activo |
| Positivo, carga −Z, combinación NL `[2] 1.35·PP` | | | Dz = **−13,882** = 1,35 × 10,283 (muelle retirado) | |

**Ley**: el muelle de un gdl con Sentido ≠ «Ambos» **solo reacciona cuando el desplazamiento del nudo en ese gdl tiene el signo del «Sentido»** (Negativo: actúa si Dz < 0, es decir, es el «apoyo» que impide bajar; Positivo: actúa si Dz > 0), en ejes globales. El tooltip del nudo en Vinculación exterior lee «Dz : Elástico, 1000 kN/m. Sentido: Positivo.». El análisis lineal (hipótesis simple) **ignora el Sentido** (muelle bilateral: con Positivo y carga −Z dio Dz = −5,070); solo las **combinaciones no lineales** lo aplican.

**Combinaciones no lineales** (botón «Combinaciones no lineales» NL del ribbon Cálculo, diálogo redimensionable «Combinaciones no lineales»): tras el primer cálculo con un muelle con Sentido el programa **genera una combinación no lineal por cada combinación de la obra** (aquí «PP» y «1.35·PP», columnas Referencia | Actúa | **Escalones de carga = 10**), con un panel «Tipos de combinación» (lista: Resistencia…, Fisuración…, Desplazamientos, Equilibrio, Tensiones) y «Factores de combinación» (Hipótesis | coeficiente). Los resultados se leen en Desplazamientos/Reacciones/Esfuerzos seleccionando en el primer combo **«Combinaciones no lineales»** (entradas `[1] PP`, `[2] 1.35·PP`). El diálogo Cálculo con elementos no lineales muestra «Análisis lineal…», «Análisis no lineal…», «Actualizar las combinaciones no lineales tras el análisis lineal», «Análisis de segundo orden elástico» (desmarcado), Perfiles, «Comprobar las barras», «Considerar la dimensión finita de los nudos».

**Sin convergencia / mecanismo** (N1: Dx, Dy Fijo, Dz Elástico K = 1000 Positivo, giros Fijo; N2 libre; carga −Z en N2 → al retirarse el muelle la estructura es un sólido libre en Z): el informe final dice, con icono rojo de error, «**No es posible calcular el caso de carga '[1] PP'. La estructura es un mecanismo. Revise la definición de cargas y las condiciones de vinculación del modelo.**» (`rr_e2.png`). El mensaje de «no se alcanza la convergencia» (id 328573) no se provocó. Para el clon: el bucle de conjunto activo debe devolver este mensaje cuando el conjunto activo final deja un mecanismo.

### 10.2 Diafragma rígido con tres pilares y carga de diafragma [V]

Obra `docs/sample_project/rr_diaf.txt` (importada en obra vacía): pilares verticales de 4 m N1–N2 (0,0), N3–N4 (6,0), N5–N6 (6,3) (z = 0..4), mismo perfil genérico (Iyy = 1e4, Izz = 5e3, Avy = Avz = 40, It = 8e3, E = 210000, ν = 0,3), bases N1, N3, N5 «Empotrado» (selección por ventana de dos clics + RMB), nudos superiores N2, N4, N6 asignados a **D1** (Propiedades > Diafragmas > Editar: «+» → fila D1 magenta, Referencia «D1», «+» de cargas → fila **Hipótesis «Peso propio» | Fx | Fy | Coord. x | Coord. y** con 0,000; las celdas se editan con clic + teclear + `{TAB}` pasa a la siguiente; el diálogo se maximiza con su botón; luego «Diafragma» + ventana de dos clics + RMB: el rótulo pasa a «N2: D1»). Carga de diafragma: **Fx = 30 kN, Fy = 20 kN en (x, y) = (1, 2) m**. Peso propio mínimo (γ = 0,01).

| Nudo | Dx (mm) | Dy (mm) | Dz | Gx (mRad) | Gy (mRad) | Gz (mRad) |
|---|---|---|---|---|---|---|
| N2 (0,0) | 17,549 | 18,422 | 0 | −6,825 | 6,541 | −2,892 |
| N4 (6,0) | 17,549 | 1,071 | 0 | −0,397 | 6,541 | −2,892 |
| N6 (6,3) | 26,225 | 1,071 | 0 | −0,397 | 9,775 | −2,892 |

Reacciones (kN, kN·m): N1 Rx −8,585, Ry −17,916, Mz +4,67 (Mx 71,66, My −34,34); N3 Rx −8,585, Ry −1,042, Mz +4,67; N5 Rx −12,829, Ry −1,042, Mz +4,67 (Mx 4,17, My −51,32). ΣRx = −30,000, ΣRy = −20,000.

**Verificación analítica** (cada pilar = voladizo de Timoshenko: kx = 1/(L³/3EIzz + L/GAvy) = 489,2 kN/m, ky = 1/(L³/3EIyy + L/GAvz) = 972,5 kN/m, kt = G·It/L = 1615,4 kN·m/rad; extremo con Gx, Gy libres): resolviendo el sólido rígido en el plano con la carga trasladada al origen como Fx = 30, Fy = 20, **Mz = x·Fy − y·Fx = 1·20 − 2·30 = −40 kN·m** da Gz = −90/31121 = **−2,892 mRad**, Dx(0,0) = 17,550 mm, Dy(0,0) = 18,423 mm: **coincide con el original a 4 cifras**. Relaciones verificadas: Dx6 − Dx4 = −Δy·Gz = 8,676 mm (leído 26,225 − 17,549 = 8,676); Dy4 − Dy2 = Δx·Gz = −17,352 mm (leído −17,351); Gz igual en los tres; **Dz, Gx, Gy libres** (cada pilar gira distinto: Gy 6,541 / 9,775); Rx(N1) = kx·Dx = 8,585, Ry(N1) = ky·Dy = 17,916; Mz de base = kt·|Gz| = 4,67 por pilar; equilibrio de momentos respecto al origen = −40 (carga) + 39,99 (reacciones).

**Conclusión**: la carga de diafragma (Fx, Fy en (x, y)) equivale a la fuerza en el punto con **Mz = (x − xm)·Fy − (y − ym)·Fx** respecto a cualquier punto de referencia (la posición del maestro no interviene en los resultados), con signo positivo antihorario (+Z). Confirmado el signo de §7.3 (antes [N]). Sin medir: giro Gz de barra articulada en nudo de diafragma, diafragma con láminas, cotas distintas.

### 10.3 Ligadura (equalDOF) entre dos nudos [V]

Obra `docs/sample_project/rr_lig.txt`: dos voladizos de 4 m en X, N1–N2 en (0,0,0)-(4,0,0) y N3–N4 en (0,3,0)-(4,3,0), N1 y N3 «Empotrado», mismo perfil. **Propiedades > Ligaduras** (panel «Propiedades - Ligaduras»: Opciones Asignar/Eliminar; Desplazamientos: «Desplazamiento en X / Y / Z» (casillas); se abre maximizado la primera vez), Z marcada, N2 y N4 por ventana de dos clics + RMB; los rótulos pasan a «N2: Dz(1)» y «N4: Dz(1)» (el 1 es el índice de la ligadura en esa dirección). Cargas de nudo −Z: **10 kN en N2 y 20 kN en N4**.

| | N2 | N4 |
|---|---|---|
| Dz (mm) | **−15,424** | **−15,424** |
| Gy (mRad) | 5,714 | 5,714 |
| Reacción Rz base (kN) | N1: **15,000** | N3: **15,000** |
| My base (kN·m) | −60,00 | −60,00 |

Analítico: cada voladizo (Timoshenko, k = 972,5 kN/m en punta) con la fuerza media: Dz = (10 + 20)/(2·972,5) = **15,424 mm**, Rz = 15 kN cada uno, My = 15·4 = 60 kN·m: **coincide**. Es decir, la ligadura impone u_N2,z = u_N4,z y reparte las cargas por rigidez; los giros de los nudos no se ligan (aquí iguales por simetría de rigidez). Obra sin peso propio relevante (γ = 0,01).

### 10.4 Apoyos oblicuos (plano y recta de usuario) y apoyo elástico lineal [V]

Misma obra que §10.3 sin la ligadura (borrada con Ligaduras > Eliminar + Z + ventana + RMB: los rótulos «Dz(1)» desaparecen). Voladizo N1–N2 (L = 4 m, N1 «Empotrado»), N2 «Genérica» con vector (1, 0, 1), giros libres, carga 10 kN en −Z en N2. Rigideces de punta del voladizo (Timoshenko): kx = EA/L = 525000 kN/m, kz = 972,5 kN/m.

Diálogo «Genérica»: al marcar **«Sobre una recta»** o **«Sobre un plano»** aparecen los campos **X, Y, Z del vector** (3 decimales; al cambiar de radio se reinician: plano → (0, 0, 1), recta → (1, 0, 0)); con vector alineado con un eje global (p. ej. plano (0,0,1) o recta (1,0,0)) aparecen debajo las filas «Dz / Dy, Dz: Fijo | **Elástico**», pero con un vector oblicuo (1, 0, 1) **esas filas desaparecen: una vinculación oblicua es siempre rígida** (no se ofrece K). El tooltip del nudo en Vinculación exterior muestra el vector **normalizado**: «Sobre un plano : (0.707, 0.000, 0.707)». Símbolo 3D del apoyo oblicuo: dibujo de rodillos inclinado según el vector.

| Caso | Desplazamiento N2 (mm) | Reacción en N2 (kN, kN·m) | Predicción |
|---|---|---|---|
| **Plano** de normal (1,0,1) | Dx = **+0,019**, Dz = **−0,019**, Gy = 0,007 mRad | Rx = **9,982**, Ry = 0, Rz = **9,982**, M = 0 | λ = 10/(1 + kz/kx) = 9,9815 sobre la normal (1,0,1); u ⟂ n |
| **Recta** de dirección (1,0,1) | Dx = **−0,019**, Dz = **−0,019** | Rx = **−9,982**, Ry = 0, Rz = **+9,982**, M = 0 | u = −10/(kx + kz) = −1,9013e-2 mm a lo largo de la recta; reacción ⟂ recta, Ry = 0 |

Es decir: el plano deja libres los desplazamientos perpendiculares al vector (el nudo desliza por el plano con la rigidez de la barra) y la reacción va **solo según la normal**; la recta deja libre el movimiento a lo largo del vector y la reacción es ⟂ a la recta (en el plano xz ± y). Las lecturas coinciden con la ligadura exacta u·n = 0 / u ∥ d a 3 cifras; la penalización 1e10 del original (§4) no se distingue con la resolución de la lectura (0,001 kN). Los giros libres no generan momentos (Mx = My = Mz = 0).

**Muelle lineal por gdl (apoyo elástico, Sentido «Ambos»)**: K = 1000 kN/m en Dz con carga 10 kN da, en el original (Sentido Negativo activo, §10.1), Dz = **5,070 mm** = 10/(972,5 + 1000) = 5,0697 (kN/m en traslación, K entero en el campo; defecto 100000). El valor en giros (kN·m/rad) está medido en `solver_medidas_original.md` §9.2 (K = 30: Gy(N2) = 1337,170 mRad y Dz = −5343,723 mm de la barra libre). Sin discrepancia con la ley K·u.

### 10.5 Vinculación interior de nudo: «Nudo empotrado con coeficiente de empotramiento parcial para todas las piezas» (TK-106) [V]

Obra `docs/sample_project/sv_c2.txt` (nuevo): N1 (0,0,0), N2 (3,0,0), N3 (6,0,0), barras N1–N2 y N2–N3 (perfil C2 estándar), N1 «Empotrado», N3 libre con 10 kN en −Z, peso propio mínimo. N2 con **Propiedades > Vinculación interior** (diálogo 748x449; al abrir sobre un nudo las piezas se rotulan A, B en magenta; 4 botones en este orden **(1) empotrado (cruz rosa), (2) empotrado parcial (cruz con doble círculo), (3) articulado (círculo), (4) genérico («?»)**; al elegir (2) aparece **Coef.Emp.Parc. [0,00] kN·m/rad**; la rejilla de iconos se repinta solo tras mover el ratón). Aceptar (639,722).

| Coef.Emp.Parc. c (kN·m/rad) | Dz(N2) | Gy extremo de barra 2 en N2 / en N3 | Dz(N3) | Salto de giro entre barras en N2 |
|---|---|---|---|---|
| (sin vínculo: viga continua) | −10,807 (calc.) | 6,429 / 8,571 (calc.) | −34,472 (calc.: 34,286 flexión + 0,186 cortante) | 0 |
| **5000** | −10,807 | 18,429 / 20,572 | **−70,473** | 12,0 mRad = M/K con M = 10·3 = 30 kN·m → **K = 2500** |
| **20000** | −10,807 | 9,429 / 11,572 | **−43,472** | 3,0 mRad → **K = 10000** |

**Ley medida**: con dos piezas el nudo aporta una rigidez rotacional total **K = c/2** entre las dos barras (lineal en c; el valor se introduce directamente en kN·m/rad, no es un coeficiente 0..1). Interpretación coherente con el código leído (§7.7, nudo adicional `tag_nudo_coef_empotramiento_giro`): **cada pieza se une al nudo de empotramiento parcial con un muelle de giro de rigidez c y las dos uniones quedan en serie** (c·c/(c + c) = c/2). Con dos piezas no se distingue de «un muelle único c/2» ni de «c/(n piezas)»: la comprobación con 3 piezas cargadas en el nudo queda **[N]** (ticket TK-106). El gdl afectado es el giro de flexión (Gy aquí; la torsión Gx no se midió).

### 10.6 Pendiente tras la segunda pasada (sin bloqueo técnico; presupuesto de contexto)

- **TK-110 (aristas de lámina: elástica, oblicua, interior distinta de «Empotrada», excentricidad): NO medido.** Receta viable: placa 4x4 (`m6_lam.txt`), Propiedades > Láminas > Vinculación exterior (810,95), las 4 aristas «Elástico» con K = 1 kN/m² (kN/m² por unidad de longitud de arista), sin otra vinculación, solo peso propio (γ·t = 25·0,1 = 2,5 kN/m²): si la ley es **K por metro de arista** la placa baja casi rígida **Dz ≈ −γt/K = −2,5 m** con cualquier malla; si fuese K por nudo de malla, con malla de 2 m (8 nudos de borde frente a 16 m de perímetro) saldría ≈ −5 m. Leer Dz en Isovalores (`tools/lam_lib.sh`: `lam_mesh 2`, `lam_calc`, `lam_view`, `lam_q`). Después, arista oblicua (vector (1,0,1)) y vinculación interior libre/articulada con una barra que llegue a mitad de arista.
- **TK-105a con barras y láminas (rotación > π/2)**: no medido. Receta: voladizo de barra con Gy elástico K = 20 kN·m/rad (§9.2 de `solver_medidas_original.md`) en la misma obra que una lámina cargada, para ver si el aviso se emite también cuando el giro excesivo está en un nudo de barra y si cambia el texto; y una placa con un nudo girando > π/2 sin barras (hoy: sin aviso, §9.8).
- **TK-106**: comprobar K = c/2 con 3 piezas cargadas en el nudo (¿c/n? ¿c por pieza en serie?); coeficiente de la torsión y de la vinculación interior genérica con casilla de empotramiento parcial.
- **Diafragma**: Gz de barra articulada en nudo de diafragma, diafragma con cotas distintas (aviso #375642 en cálculo), carga de diafragma con varias hipótesis.
- **Sentido**: el aviso 328573 de no convergencia no se provocó; solo se vio «La estructura es un mecanismo» (§10.1). Tampoco se midió el comportamiento con muelles de giro con Sentido, ni varios muelles con Sentido interdependientes (conjunto activo).
- Las obras de esta pasada no se guardaron como `.ed3` (el original guarda por defecto en su carpeta de proyectos y se cerró sin guardar); se reproducen con los `.txt` (`rr_diaf.txt`, `rr_lig.txt`, `sv_c1.txt`, `sv_c2.txt`) y los pasos descritos en §10.1-§10.5.

## 11. Tercera pasada (2026-11-12, RE-ANALYST): aristas de lámina (TK-110) [V]

Obras: `docs/sample_project/m6_lam.txt` (placa 4 x 4 m, barras borradas, lámina por defecto h = 100 mm, C25/30, peso propio 24,53 kN/m³ => 2,453 kN/m²) y `docs/sample_project/rr_lam2.txt` (dos láminas 4 x 4 m contiguas, arista común x = 4). Capturas `docs/screenshots/tk110_*.png`, `u8..u14.png`.

### 11.1 Vinculación exterior de lámina: diálogo y unidades
- Propiedades > Láminas > Vinculación exterior (810,80 en la pestaña Propiedades); la lámina se elige con **ventana** (clic en una esquina, clic en la opuesta, RMB; un clic dentro de la lámina no la selecciona porque se dibuja en alambre). Se aplica a **todas las aristas de la lámina seleccionada** (incluida la arista compartida con otra lámina: con dos láminas contiguas y «Dx, Dy, Dz Fijo» la arista común queda apoyada, ver §11.4).
- Es el mismo diálogo de nudos (9 + 9 iconos; primero «Genérica»), pero **la unidad del muelle es kN/m²** (fuerza por metro de arista y por metro de longitud de arista: K es por unidad de longitud), con tooltip «…módulo de balasto del terreno, en las unidades indicadas». Sentido: Ambos / Positivo / Negativo como en nudos. Valor por defecto K = 100000. El diálogo se rellena con la vinculación actual al reabrirlo.

### 11.2 Ley del muelle elástico de arista: **por metro de arista, independiente de la malla** [V]
Placa 4 x 4, las cuatro aristas Dx, Dy «Fijo», **Dz «Elástico» K = 1 kN/m²**, solo peso propio (carga total 2,453 · 16 = 39,25 kN, perímetro 16 m):

| Malla máx. | Dz mín. | Dz máx. (centro) | Predicción K por metro: w = γt/K |
|---|---|---|---|
| 1 m (defecto, 57 nudos) | **−2451,63 mm** (aristas) | **−2454,29 mm** | 2,453 m |
| 2 m | **−2451,60 mm** | **−2454,41 mm** | 2,453 m |

Con K por nudo de malla saldría ≈ 2x (malla de 2 m) o un valor dependiente del número de nudos: **descartado**. Ley: la rigidez de arista es **K · L_arista** repartida por el contorno (carga nodal tributaria de cada nudo = K · longitud tributaria); la placa (E t³/12) casi no se flexiona (±1,4 mm entre arista y centro). El clon debe integrar el muelle por longitud tributaria de cada nudo de arista, no por nudo.

### 11.3 Arista oblicua («Sobre un plano», vector (1,0,1))
Con «Sobre un plano» el diálogo muestra X, Y, Z del vector (defecto 0, 0, 1; rango «-1000.000 y 1000.000»), y solo **Dz: Fijo | Elástico** si el vector está alineado con un eje; con (1,0,1) **desaparece la fila elástica** (igual que en nudos, §10.4): la arista oblicua es siempre rígida. El símbolo 3D es el del rodillo inclinado. Calcular con peso propio: «**Esta estructura es un mecanismo. Revise los datos introducidos. Alguna de las barras de la estructura no está vinculada correctamente en sus extremos, es decir, no es estable.**» (**sin** línea de gdl ni coordenadas: variante de texto distinta de la de nudos; la barra de progreso muestra «gstif(0,0)=100»). Es lo esperado: una placa con todas sus aristas «sobre un plano» de normal (1,0,1) deja libres dos traslaciones de sólido rígido (Dy y (1,0,−1)), y el peso propio excita la segunda. [N]: no hay forma de medir un valor numérico de arista oblicua con una sola lámina sin otra restricción (la selección aplica la misma vinculación a todas las aristas).

### 11.4 Vinculación interior de lámina: diálogo y efecto [V]
Propiedades > Láminas > Vinculación interior (741,80): diálogo 474 x 310 con **4 radios: Libre, Empotrada (defecto), Articulada, Genérica**. «Genérica» muestra «Desplazamientos en ejes locales de la arista» (Dx, Dy, Dz) y «Giros en ejes locales de la arista» (Gx, Gy, Gz), cada uno Libre/Fijo/Elástico, con K en **kN/m²** (desplazamientos) y **kN·m/rad.m** (giros, por metro de arista). Se aplica a las aristas de la lámina seleccionada **compartidas con otro elemento** (vecinas); con una lámina aislada no hay efecto que medir.

Medición con `rr_lam2.txt` (dos láminas 4 x 4 contiguas, exterior «Dx, Dy, Dz Fijo» en las dos (apoya también la arista común), peso propio, malla por defecto), Dz máximo (centro de cada lámina):

| Interior | Dz máx. | Lectura |
|---|---|---|
| **Empotrada** (defecto) | **−0,71 mm** | continuidad del giro por la arista común: por simetría equivale a placa con una arista empotrada |
| **Articulada** | **−0,96 mm** | giro libre en la arista común: cada lámina es una placa simplemente apoyada en 4 lados (cociente 0,74 frente al caso continuo) |
| **Libre** | no calcula: error «**Se ha encontrado un nudo que no está conectado a ninguna barra ni a ninguna lámina.**» | los nudos de la arista común quedan desconectados de las dos láminas; el original aborta antes de resolver |
| **Genérica** (Dx, Dy, Dz Fijo + Gx Elástico K = 10000, o con Gx «Fijo») | **aserción interna** «Atención: No se cumple una comprobación de seguridad. Expresión: **ancho_banda_nudo_arista > 0**. Fichero: cm3dmalla.c (CALCMALLAD3D.DLL). Línea: 12350. Módulo: CYPE 3D.exe» (botones Salir / Ignorar / «Ignorar todos los errores de este tipo»; «No» en la pregunta de continuar **cierra el programa**) | la combinación «exterior fijo en la arista + interior genérica» dispara una comprobación en el mallado (anchura de banda de nudo de arista nula); no se pudo medir una ley de giro elástico. Captura `tk110_int_gx_assert.png` |

Notas de conducción: el diálogo «Vinculación interior» de lámina: Libre (751,416), Empotrada (751,438), Articulada (751,460), Genérica (751,482), Aceptar (776,652) con la ventana de selección (1250,470)-(1912,840) tras `move:1500,600|wheel:-7` (zoom). «Nueva obra» pregunta «El fichero ya existe. ¿Desea sobreescribirlo?» (Sí 840,572) cuando `nuevo.ed3` existe de una sesión anterior; si se salta, la cadena `SV_BASE` se desincroniza (Aceptar del asistente (672,725), Terminar (1336,883), Estados límite Aceptar (799,666)).

### 11.5 Pendiente de esta pasada
- Excentricidad del plano medio: no se encontró en la GUI de Propiedades > Láminas (solo Discretización, Bandas de integración, Factores de rigidez, Pandeo); queda [N] (sin evidencia de que la GUI la ofrezca).
- Vinculación interior de arista con barra que llega a mitad de arista y arista oblicua numérica: no medidas (ver arriba).

### 11.6 TK-105a con barras y láminas en la misma obra (rotación > π/2) [V]
Obra `docs/sample_project/rr_bl.txt`: voladizo N1–N2 (L = 4 m, Genérico A = 100 cm², Avy = Avz = 40, Iy = 10000, Iz = 5000, It = 8000 cm⁴, E = 210000 MPa, **peso específico 77 kN/m³ => 78,5 kg/m**) con N1 «Genérica» Dx, Dy, Dz, Gx, Gz Fijo y **Gy Elástico K = 2 kN·m/rad**, más una placa 4 x 4 m aparte (aristas «Articulación», peso propio), solo peso propio. Capturas `tk105a_bl.png` (sin peso en la barra), `tk105a_bl2.png`, `tk105a_inc.png`, `tk105a_bl3.png`.

| Caso | Informe final de cálculo |
|---|---|
| Barra sin peso (peso específico por defecto 0: «Gen.(0.0 kg/m)») | solo «La obra ha sido calculada sin realizar la comprobación de las barras.» (N1: Gy = 0,400 mRad, Dz extremo −1,600 mm: era el peso de la lámina; la barra no carga) |
| Barra con 0,785 kN/m y K = 2 (giro del empotramiento elástico ≈ M/K = 6,16/2 ≈ 3,08 rad > π/2) con la placa en la obra | «La obra ha sido calculada sin realizar la comprobación de las barras.» **y** «**Se han obtenido desplazamientos demasiado grandes. Puede consultar los mensajes de aviso correspondientes mediante la opción 'Mostrar/Oculta incidencias' del menú 'Cálculo'.**» (mismo texto que sin láminas); con Incidencias activo el triángulo amarillo aparece **sobre la barra** (a mitad), **no** sobre la lámina |
| Barra con K = 100000 (sin giro grande) y la placa con factores de flexión y cortante 0 (Mxx = Mxy = Myy = Vxz = Vyz = 0, w ~ 1e9 mm, §15.4 de `elemento_lamina_bandas.md`) | **sin aviso** de desplazamientos |

Conclusión: con barras y láminas el aviso se da **solo por nudos de barras** (confirma la regla [N] del clon: los nudos solo de lámina no se comprueban); la comprobación no cambia el texto ni se localiza en la lámina. [N] no se leyó el giro de un nudo de lámina en el tercer caso (el nudo interior no es seleccionable), pero el desplazamiento Dz de 9,6e8 mm del mismo tipo de placa ya se midió.

Otros datos de esta obra: al aceptar una K igual al valor por defecto (100000 o, en este caso, tras editar a 100000) el original pregunta «**El valor introducido para la constante de apoyo elástico (K) coincide con el valor por defecto. Recuerde que debe asignar un valor conforme al grado de empotramiento que posee la vinculación exterior en el nudo. ¿Desea continuar?**» (Sí / No); el diálogo se abre con un desplazamiento de +2 px respecto al de lámina (K de giro Gy en (930,627)); Describir > «Peso específico» es el campo del extremo inferior de «Material» (campo 9.º de `sv_desc`, por defecto 0; con 77 la barra pesa 78,5 kg/m).
