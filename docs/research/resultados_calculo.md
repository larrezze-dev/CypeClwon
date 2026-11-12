# Resultados del cálculo, combinaciones no lineales e informe final (T-P8-06)

Fecha: 2026-11-12. Agente: BACKEND-DEVELOPER (paso 1, núcleo). Fuentes: medidas ya documentadas en `solver_medidas_original.md` (§2-§3, §6, §9-§10) y `solver_diafragmas_ligaduras_apoyos.md` (§5, §10.1, §12.5); capturas del original `docs/screenshots/s_5.png`, `s_9.png`, `s_10.png`, `s_lin.png`, `s_nl2.png`, `rr_e2.png`, `tk108_B1.png`; tabla de cadenas `docs/research/ui/data/strings_ed3dlib.tsv` y `strings_merged_latin.tsv` (REA de `ed3dlib.dll` y `combsanl.dll`). En esta pasada **no se lanzó el original**.

Marcas: **[V]** verificado (medida, captura o cadena del original); **[N]** no verificado (decisión del clon, documentada).

## 1. Qué muestra el original

### 1.1 Paneles de resultados [V capturas `s_lin.png`, `s_nl2.png`]

Cálculo > Desplazamientos abre el panel flotante «Propiedades - Desplazamientos»:

- Radios «**Combinación seleccionada**» (por defecto) / «**Envolventes**».
- Grupo «**Comb. seleccionada**»: primer combo con el tipo de resultado (vistos: «**Hipótesis simple**» (#64399) y «**Combinaciones no lineales**» (#377012); los demás valores —familias de combinaciones lineales— no se capturaron [N]) y segundo combo con el elemento («Peso propio», «[1] PP»).
- Grupo «**Desplazamientos**»: casillas Dx, Dy, Dz, Gx, Gy, Gz (todas marcadas por defecto).
- En la vista: globo por nudo con «Dx: 0.000 mm / Dy / Dz / Gx: 0.000 mRad / Gy / Gz» (3 decimales; mm y mRad).
- Reacciones y Esfuerzos tienen paneles análogos (Esfuerzos: leyes por barra; «Sólo las barras seleccionadas» + «Consultar valores» da el globo N, Vy, Vz (kN), Mt, My, Mz (kN·m) en el punto X; `solver_medidas_original.md` §1, §3.2).
- Cadenas de títulos de listados: #377027 «Desplazamientos de los nudos, por combinación no lineal», #377028 «Reacciones en los nudos, por combinación no lineal», #377029 «Esfuerzos en barras, por combinación no lineal»; envolventes #64546-#64548 y #38878-#38885.

### 1.2 Combinaciones no lineales [V `s_9.png`, `s_10.png`, §10.1 y §12.5]

- Botón «Combinaciones no lineales» (NL) del grupo Tensión / Deformación. Diálogo redimensionable «Combinaciones no lineales»: barra (+, copiar, subir, bajar, borrar…, flecha azul = «Generar combinaciones para las distintas situaciones de proyecto», pincel); tabla **Referencia | Actúa | Escalones de carga** (10 por defecto); paneles «**Tipos de combinación**» (Referencia; lista con Resistencia, Resistencia…, Fisuración, Fisuración…, Desplazamientos, Equilibrio, Tensiones) y «**Factores de combinación**» (+, copiar, borrar; Hipótesis | coeficiente).
- Diálogo de generación: casillas **Resistencia**, **Fisuración**, **Fisuración bajo cargas de larga duración**, **Desplazamientos**, **Equilibrio de los elementos de cimentación**, **Tensiones en el terreno**. Obra con solo peso propio, Resistencia + Desplazamientos → `PP`, `1.35·PP`, ambas con Actúa y 10 escalones.
- Parámetros del análisis no lineal (cadenas de `combsanl`, valores por defecto **[N]** salvo los escalones): #377014 «Número de escalones de carga», #377016 «Tolerancia permitida para considerar que se ha alcanzado la convergencia», #377018 «Número máximo de iteraciones en cada escalón de carga», #377020 «Número máximo de reintentos», con sus ayudas #377021-#377024 (cargas divididas en escalones; criterio en energía; reinicio doblando los escalones).
- Resultados: «[n] referencia» en el combo «Combinaciones no lineales» (`[1] PP` → Gy = −0,507 mRad; lineal −0,339, `s_nl2.png`, `s_lin.png`).

### 1.3 Informe final de cálculo [V]

Ventana «Informe final de cálculo», apartado «Cálculo de la estructura», una línea por mensaje con icono:

| Situación | Texto | Icono | Fuente |
|---|---|---|---|
| Muelle con Sentido y sin combinaciones no lineales | «No se ha realizado el análisis de tensión/deformación no lineal» (#378613; **sin espacios** alrededor de la barra, como la cadena; §12.5 del otro documento lo transcribió con espacios) | rojo | `s_5.png` |
| Siempre que calcula sin comprobar | «La obra ha sido calculada sin realizar la comprobación de las barras.» | aviso | `s_5.png` |
| Combinación no lineal que queda en mecanismo | «No es posible calcular el caso de carga '[1] PP'. La estructura es un mecanismo. Revise la definición de cargas y las condiciones de vinculación del modelo.» (#214001) | rojo | `rr_e2.png` |
| Mecanismo en el análisis lineal | «Esta estructura es un mecanismo. Revise los datos introducidos.» (el detalle con el gdl y el punto sale en la ventana de proceso) | – | `tk108_B1.png` |
| Giro de nudo > π/2 | «Se han obtenido desplazamientos demasiado grandes. Puede consultar…» | – | §9.2 |
| Obra vacía | «La obra está vacía. Para poder calcular debe introducir, al menos, una barra o una lámina.» (#64197) | – | cadena |
| Combinación no lineal con hipótesis que no actúa | #381789 «No es posible realizar el cálculo ya que hay combinaciones no lineales en las que intervienen hipótesis que no actúan…» | – | cadena |

Con mecanismo o desplazamientos excesivos los botones de resultados quedan inactivos [V §9.1, §9.2].

## 2. Implementación en el clon (núcleo)

### 2.1 Datos de proyecto: `core/nonlinear_combinations.h`

- `Project::nonlinearCombinations` (sección JSON `nonlinear_combinations`, añadida al final de la lista; ausente en obras antiguas = vacía): `items` (referencia, actúa, escalones = 10, factores por **id** de hipótesis, tipos), `generateKinds` (última elección del diálogo de generación) y `updateAfterLinear` («Actualizar las combinaciones no lineales tras el análisis lineal», marcada por defecto [V §12.5]).
- `generateNonlinearCombinations(project, kinds)`: **Resistencia** = situaciones E.L.U. (persistente, sísmica, accidental) de `core::generateCombinations` con el código de la familia de **hormigón**: con las normas por defecto de una obra nueva es la única que da `PP` (coeficiente favorable 1,0, EN 1990); la de acero laminado (Código Estructural con radio CTE → CTE DB SE, favorable 0,8) daría `0.8·PP`, que el original no generó (**[R]** por coincidencia con la medida; qué familia lee realmente el original: [N]); **Desplazamientos** = combinaciones características de servicio; deduplicación por vector de coeficientes **entre tipos**, en orden de generación. Reproduce `PP`, `1.35·PP` [V]. Fisuración, Fisuración de larga duración, Equilibrio y Tensiones en el terreno **no se generan** (situaciones no medidas; las dos últimas dependen del módulo de cimentación): se devuelven en `unsupported` (ticket).
- Etiqueta `"[n] referencia"` (n = posición desde 1).
- #381789: `checkNonlinearCombinations` (solo combinaciones que actúan; factor no nulo de una hipótesis que no actúa).
- #380751: borrar una hipótesis borra las combinaciones no lineales con factor no nulo para ella (dentro del mismo comando deshacible) y `loadCaseDependents` las lista.
- Escalones de carga: **solo dato**. El clon resuelve los muelles con Sentido por conjunto activo exacto; el estado final de un problema elástico con apoyos unilaterales no depende del camino de carga (medido: `[2] 1.35·PP` = 1,35 × `[1] PP`). Tolerancia, iteraciones y reintentos del original no se modelan [N].

### 2.2 Flujo de cálculo: `fem/project_analysis.h` (`analyseProject`)

1. Obra vacía → #64197.
2. `buildFrameModel`: `errors` y `unsupported` detienen el cálculo (los textos de `unsupported` son del clon).
3. `solveLinear` de todas las hipótesis (Sentido ignorado [V]); mecanismo / desplazamientos excesivos → línea del informe y `calculated = false`.
4. Si hay muelles con Sentido y «Análisis no lineal» (por defecto): sin combinaciones → #378613 (rojo) [V]; con «Actualizar…» y tipos elegidos se regeneran (se conservan Actúa y Escalones de las que tienen la misma referencia, [N]); #381789; cada combinación que **actúa** se resuelve con `solveNonlinear` como «[n] referencia»; un fallo da su mensaje (rojo) y deja disponibles los resultados lineales [N].
5. «La obra ha sido calculada sin realizar la comprobación de las barras.» (aviso).

Combinaciones lineales para los resultados: `combinationResults(analysis, project, family)` = `generateCombinations` + superposición (ver 2.3); los coeficientes ortogonales de sismo XY no se aplican (el adaptador no tiene cargas sísmicas) [N].

### 2.3 Consultas de resultados: `fem/results.h`

| Función | Qué da |
|---|---|
| `nodeDisplacement` | Dx..Gz del nudo, ejes globales (m, rad) |
| `LoadCaseResult::reactions` | fuerzas de los apoyos sobre la estructura, globales [V §3.3] |
| `LoadCaseResult::barDisplacements` | desplazamientos de los **extremos de barra** (con el giro propio de un extremo liberado o parcialmente empotrado: es lo que el original muestra al pulsar una barra [V §9.4, §12]) |
| `barForcesAt(x)` | N, Vy, Vz, Mt, My, Mz en ejes locales con el convenio de `applySignConvention` (TK-107: solo My invertido) |
| `barDisplacementAt(x)` | **deformada exacta del elemento**: desplazamiento del extremo de barra en el nudo inicial + integral de las deformaciones (N/EA, V/GAv con cortante, T/GJ, M/EI y temperatura) por el método de la carga unitaria. No es una cúbica de Hermite (en una viga biapoyada con carga uniforme la cúbica de los giros extremos daría qL⁴/96EI en lugar de 5qL⁴/384EI). Cierra exactamente en el otro extremo |
| `barLaw(divisions)` | puntos equiespaciados + límites de tramo + ambos lados de cada carga puntual (salto vertical) |
| `superposeCases` / `superposeResult` | combinaciones por superposición de todo lo lineal (también láminas); si interviene una hipótesis fallida, la combinación no tiene resultados |
| `shellDisplacementAt` (`shell_analysis.h`) | deformada de lámina en un punto (interpolación cuadrática de los 6 nudos del triángulo, [N]) |
| `smoothedShellForces`, `shellForcesAt`, `bandForcesAt` | esfuerzos de lámina y bandas (T-P8-02), válidos también sobre `superposeResult` por índice |

**Barras partidas en aristas de lámina**: la lámina actúa en los nudos de malla intermedios, así que la estática desde el extremo J no vale dentro de la barra. El solver guarda ahora por barra partida los tramos (`LoadCaseResult::barSegments`: x0, x1, esfuerzos en ambos extremos, desplazamiento en x0) y las consultas usan el tramo que contiene x (`pieceForcesAt`, `pieceDisplacementAt` en `linear_solver.h`).

## 3. Verificación (tests `tests/fem_results_tests.cpp`)

| Test | Referencia |
|---|---|
| Deformada viga biapoyada de 6 m (C2 en una barra): x = 3 → −8,175 mm, Gy(N1) = 4,286 mRad, cuarto de luz analítico, cierre en x = L | [V C2] + fórmula Timoshenko |
| Voladizo C1: punta −11,475 mm y 4,201 mRad; x = 1; 2; 3,3 m contra la fórmula con P, q y cortante | [V C1] |
| Extremos articulados con nudos empotrados: flecha de biapoyada y giro del extremo de barra ≠ giro del nudo | analítico |
| Temperatura (gradiente 20/0 en canto 50 mm + media): punta +38,400 mm, Dx +0,480 mm, w(L/2) = w(L)/4 | [V §4] |
| Esfuerzos: C1 en x = 1,9976: Vz −11,542, My −21,57; empotramiento My −46,16; C2 en x = 2,934: Vz −0,658, My 44,976 (el original muestra 44,93 por interpolar la ley muestreada) | [V §3.2] |
| Ley con carga puntual: salto de Vz = 10 kN | analítico |
| Superposición = caso combinado resuelto (desplazamientos, reacciones, esfuerzos y deformada en x) | identidad lineal |
| Barra en arista de lámina: continuidad por tramos, igualdad con el nudo de malla, My ≈ qL²/8 | analítico aproximado |
| Generación NL `PP`, `1.35·PP`; Fisuración no soportada | [V s_10] |
| Informe sin combinaciones NL (#378613 + aviso) y lineal −5,070 mm | [V s_5, §10.1] |
| NL `[1] PP` −10,283; `[2] 1.35·PP` −13,882 mm | [V §10.1] |
| Muelle de giro: lineal −0,339; NL Negativo −0,339, Positivo −0,507 mRad | [V §12.5] |
| Mecanismo NL `[1] PP` con su texto, lineal disponible | [V rr_e2] |
| #381789, #380751 (+ deshacer), JSON | cadenas |
| Obra vacía (#64197) y mecanismo lineal (informe, sin aviso de comprobación) | [V] |

## 4. Pendiente

- **Muestreo de las leyes del original** [N]: el globo de C2 en x = 2,934 m da My = 44,93 frente a 44,976 exacto. Interpolando linealmente una ley muestreada, el error 0,046 encaja con 14 o 15 intervalos en la barra de 3 m (paso ≈ 0,2 m) entre puntos 2,8 y 3,0; no se ha leído en el código cuántos puntos usa. El clon devuelve el valor exacto; la GUI puede muestrear cuando se mida.
- Escalas por defecto de la deformada de barras y de las leyes de esfuerzos (TK-104): solo está medido el factor de la deformada de **láminas** en Isovalores (0–100000, defecto 10, `elemento_lamina_bandas.md`). Para barras: [N].
- Otros valores del primer combo de resultados (familias de combinaciones lineales) y el panel «Envolventes»: por capturar.
- Tipos de combinación no lineal Fisuración, Fisuración de larga duración, Equilibrio y Tensiones en el terreno; parámetros por defecto de tolerancia, iteraciones y reintentos (`combsanl`).
- #377976 «No ha realizado el análisis de tensión/deformación no lineal para todas las combinaciones necesarias.»: no se sabe cuándo aparece.
- Resultados lineales cuando una combinación no lineal falla: el clon los deja disponibles [N].
- Hipótesis «No actúa»: `analyseProject` también las resuelve, de modo que un mecanismo o unos desplazamientos excesivos en una hipótesis que no actúa bloquean todo el cálculo; si el original las calcula es [N].
- Familia que usa «Resistencia» al generar combinaciones no lineales ([R] hormigón, ajustado a un solo punto). Medición discriminante: cambiar la norma de una familia para que hormigón y acero laminado den listas distintas con las mismas hipótesis y ver cuál devuelve el generador.
