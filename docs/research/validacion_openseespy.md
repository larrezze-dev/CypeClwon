# Validación del solver lineal del clon contra el OpenSees público (openseespy)

Fecha: 2026-11-12. Vía de apoyo a T-P8-05. Tests: `tests/fem_opensees_ref_tests.cpp` (suite `FemOpenSeesRef`). Generador: `tools/validation/opensees_reference.py`. Constantes: `tests/opensees_reference_data.inc` (generado; ctest no ejecuta Python). Referencia: `tools/validation/opensees_reference.json`.

## Qué es y qué NO es esta validación

- **Es** una comprobación independiente de que la teoría de pórtico lineal 3D del clon (`src/core/fem/linear_solver.cpp`) coincide con un solver de referencia ampliamente usado, con elementos estándar.
- **NO sustituye** la validación contra el original. CYPE 3D embebe su propio elemento `ElasticBeam3dCype`, su gestor de ligaduras `CYPETransformation` y su solver `CYPEMumpsSolver` (ver `solver_lineal_portico.md`); el OpenSees público no los contiene. Que el clon coincida con OpenSees prueba que el clon calcula bien un pórtico; no prueba que reproduzca las convenciones, la ley de empotramiento parcial ni la elección Euler-Bernoulli/Timoshenko del original (TK-105, TK-107 siguen abiertos). T-P8-05 contra el original sigue bloqueada por la licencia M3D.

## Entorno

- `.toolchain/venv-opensees` (dentro del proyecto, ya ignorado por `.gitignore` vía `.toolchain/`): Python 3.12.9, `openseespy 3.8.0.0` (es la versión que publica PyPI; no hay 3.2.x/3.7.x para este intérprete). `numpy 2.5.4`.
- Regenerar: `.toolchain/venv-opensees/Scripts/python.exe tools/validation/opensees_reference.py` (escribe el JSON y el `.inc`).
- Elementos: `elasticBeamColumn` (Euler-Bernoulli) y `ElasticTimoshenkoBeam` (3D, `Avy`/`Avz`), `geomTransf Linear` con `vecxz` = eje local z. Restricciones `Transformation` (con `Plain`, openseespy ignora los valores no nulos de `sp`: los asientos salían 0).
- Convenio de esfuerzos en la salida (el del clon): origen = −`localForce`(I), final = +`localForce`(J).

## Casos (15 modelos × 2 teorías = 30 comparaciones)

Comparados: desplazamientos y giros de todos los nudos, reacciones de los nudos con apoyo y esfuerzos de extremo (N, Vy, Vz, T, My, Mz) de cada barra. Tolerancia relativa al mayor valor de cada grupo (traslaciones, giros, fuerzas, momentos). Desviación máxima medida (propiedad `RecordProperty` del test):

| Caso | Contenido | EB | Timoshenko |
|---|---|---|---|
| A1 | voladizo 4 m, carga nodal en Z, Y, axil y torsor | 7,5e-14 | 7,5e-14 |
| B1 | biapoyada 6 m, carga uniforme global | 6,7e-14 | 1,3e-7 (malla) |
| B2 | biapoyada, carga puntual descentrada con componentes X/Y/Z | 6,7e-14 | 8,9e-10 |
| C1 | pórtico plano XZ (empotrado-empotrado), H nodal + q en el dintel | 1,3e-13 | 1,1e-10 |
| D1 | pórtico espacial, 4 barras oblicuas con ángulos de giro (vecxz arbitrario), Avy ≠ Avz, carga nodal + uniforme global + uniforme local + puntual | 4,1e-13 | 2,3e-7 (malla) |
| E1 | articulación en el extremo (ambos planos), carga uniforme | 8,8e-9 | 8,7e-9 |
| E2 | articulación interna (plano xz) en el nudo común, carga nodal | 1,8e-8 | 1,3e-8 |
| E3 | empotramiento parcial (muelles de giro distintos en cada extremo y plano), carga uniforme | 2,8e-9 | 1,3e-5 (malla) |
| F1 | temperatura uniforme, barra empotrada ambos extremos | 1e-16 | 1e-16 |
| F2 | uniforme + gradientes Y y Z, voladizo | 1,4e-8 | 2,5e-8 |
| F3 | gradientes Y y Z, empotrada ambos extremos | 7,1e-9 | 7,1e-9 |
| G1 | carga trapezoidal parcial (x 1…3 m, componentes Y y Z distintas) | 3,2e-8 | 2,7e-8 |
| H1 | ángulo de giro de 30°, carga nodal + uniforme global, Avy ≠ Avz | 3,8e-13 | 1,2e-10 |
| I1 | viga continua de dos vanos, asiento 5 mm del apoyo central | 3,6e-9 | 3,6e-9 |
| I2 | asientos y giro impuestos en un doble empotramiento | 1,4e-13 | 1,9e-13 |

Tolerancias aplicadas en el test: 1e-9 por defecto (casos nativos); 1e-7 en los casos con emulación (E1-E3 rigidez 1e11/1e-6 del enlace `zeroLength`, F2/F3/G1/I1 por redondeo frente a escalas casi nulas); 1e-6 en Timoshenko con carga repartida (malla); 5e-5 en E3 Timoshenko.

**Resultado: coincidencia en los 15 casos y las dos teorías; no se ha encontrado ningún fallo del solver, ni se ha modificado el solver.** Además `ComparisonDistinguishesBeamTheories` comprueba que la comparación es sensible (el clon Euler-Bernoulli contra la referencia Timoshenko difiere > 1e-3 en A1) y `SettlementCasesAreInEquilibrium` comprueba el equilibrio de reacciones con asientos.

## Limitaciones y cómo se emularon (leer antes de confiar en un caso)

1. **Timoshenko con cargas de barra.** `ElasticTimoshenkoBeam3d` no admite `eleLoad` (el propio OpenSees lo avisa). Las barras cargadas se mallaron en 240 elementos con cargas nodales consistentes (Hermite, integración de Gauss) calculadas en el script. Es un error de malla O(h²), no del clon: en E3, 30/60/120/240 elementos dan 10,3619/10,3455/10,3414/10,3404 kN·m y el clon da 10,34002 (valor límite de Richardson 10,3400). Las cargas nodales equivalentes son independientes del código del clon, pero no son un elemento de OpenSees.
2. **Carga trapezoidal (G1)** y puntual en barras mallada: mismas cargas nodales consistentes (también para EB, porque `elasticBeamColumn` solo admite uniforme y puntual).
3. **Temperatura (F1-F3).** `elasticBeamColumn` no tiene carga térmica nativa (solo `-beamThermal` en elementos de fibras). Se emplean las cargas nodales equivalentes exactas de una barra prismática (N = EAαΔT; My = E·Iy·α·ΔTz/hz; Mz = −E·Iz·α·ΔTy/hy) y se restan a las fuerzas de elemento. Las fórmulas y el signo (cara +z más caliente flecta hacia −z) los deduje yo del enunciado del clon: este caso confirma que el clon es coherente con la mecánica clásica con esas fórmulas, no una comprobación independiente del convenio de signo del original. Ningún valor de OpenSees prueba qué hace CYPE con el gradiente.
4. **Liberaciones y empotramiento parcial (E1-E3).** OpenSees no tiene condensación de extremos en el elemento estándar: se emulan con un nudo duplicado y un `zeroLength` orientado con los ejes locales de la barra (traslaciones 1e11 kN/m; giro libre 1e-6, parcial K). Los giros de nudo que ninguna barra rigidiza (E1 nudo 2 Gy/Gz; E2 nudo 2 Gy) son indeterminados en OpenSees y el clon los bloquea (`LinearResult::lockedDofs`); no se comparan.
5. **Asientos** con `sp` en patrón y `constraints Transformation`.
6. **Esfuerzos en barras mallada** (Timoshenko cargado): origen del primer elemento y final del último, con la carga consistente del elemento restada.

## Discrepancias / posibles bugs del clon

Ninguna detectada por esta vía. Los puntos abiertos no son del clon respecto a OpenSees sino respecto al original y siguen sin validar (no los puede resolver esta vía):

- **Euler-Bernoulli vs Timoshenko** (TK-105): las dos teorías coinciden con OpenSees; la que aplica `ElasticBeam3dCype` sigue desconocida (diferencia de hasta 1,2-3,7 % en los casos de referencia).
- **Ley del empotramiento parcial** (`EndPlaneFixity`, coeficiente 0…1 → `K = coef/(1−coef)·4EI/L`, [N] en `solver_lineal_portico.md` §4.4): el clon resuelve exactamente un muelle de giro de rigidez dada (E3 lo confirma); que esa traducción coeficiente→K sea la del original no se puede comprobar con OpenSees público.
- **Convenio de signos de esfuerzos** (TK-107) y **la condensación con relaciones de rigidez 1e-3** del original frente a la exacta del clon: no verificables aquí.
- **No cubierto** (el clon no lo implementa aún o no hay elemento estándar equivalente): brazos rígidos/«tamaño de nudo», ligaduras, diafragmas, apoyos oblicuos, apoyos de compresión, alabeo, láminas.

## Cómo ampliar

Añadir un `case['X']` en `cases()` del script, regenerar el `.inc` y espejar el modelo en `buildCases()` del test (mismos ids, coordenadas y cargas). Las referencias se escriben como constantes `kRef_<caso>_{eb,ti}`.
