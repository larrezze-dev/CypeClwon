# Elemento lámina (EF) y bandas de integración de CYPE 3D: investigación para T-P8-02

Fecha: 2026-11-12. Vía: REA (cyopensees.dll, calcmef.dll, calcmallad3d.dll; mismas DLL y sesión que `solver_lineal_portico.md`). El original **no se lanzó** (aviso del coordinador: otro agente lo usa para medir referencias del solver). Todo lo que requiere ejecución queda en la sección 9 como pendiente.

Marcas: **[V]** verificado en código o cadenas del original (REA); **[N]** no verificado (inferencia, hipótesis o hueco). Las direcciones son de las DLL de la versión 2027 instalada.

## 1. Resumen

- **[V]** El análisis de láminas lo hace OpenSees embebido (`cyopensees.dll`), con elementos propios de CYPE. La malla la genera `calcmallad3d.dll` (solo triángulos en láminas: `cm3dmalla_lamina_t` con `triangulos`; no hay mallado en cuadriláteros de láminas). `calcmef.dll` solo hace postproceso: integración de esfuerzos en bandas, tensiones en caras, Von Mises, tensiones principales y armado.
- **[V]** Elemento por defecto: triángulo de **6 nudos** (`element ShellCype`, ficheros de resultados `itri6n.cyres`). Con la variable de entorno `CYOS_USE_TRI3N_SHELLS` definida se usa el de **3 nudos** (`itri3n.cyres`, familia `ShellDKGT*`). Los cuadriláteros independientes (`cyoscuad_*`, `iquads.cyres`) usan `ShellDKGQ` (4 nudos); no los genera la malla de láminas de la obra (son para los elementos de otros módulos) [N: origen].
- **[V]** Formulación del material: sección elástica membrana + placa (`ElasticMembranePlateSectionCype`) con 8 factores de rigidez (los de «Rigidez de la lámina»: Fxx, Fxy, Fyy, Mxx, Mxy, Myy, Vxz, Vyz), espesor, E y ν (o los 8 módulos ortótropos).
- **[V]** Tensiones en caras: `σ = N/h ± 6M/h²` y `τ = V/h` (sin factor 3/2). Von Mises: `√(σx² − σxσy + σy² + 3(τxy² + τxz² + τyz²))`.
- **[N]** No se ha podido leer la formulación interna del elemento (clase `ShellCype`, en el ejecutable OpenSees comprimido de `cyopensenesbas.dll`; ver `solver_lineal_portico.md` §10, TK-007). Por eso la rigidez de flexión/cortante/drilling del elemento es una decisión del clon (sección 8) y se valida con casos analíticos.

## 2. Tipos de elemento y comandos Tcl [V]

Cadenas UTF-16 de `cyopensees.dll` (direcciones 0x180131a40 … 0x1801322b0 y 0x18012cd70 …):

| Comando (formato exacto) | Uso |
|---|---|
| `section ElasticMembranePlateSectionCype %lu %11.9f ×12 %s;` | Sección lámina isótropa CYPE (ver §3) |
| `section ElasticMembranePlateOrthotropicSectionCype %lu %11.9f ×17 %s;` | Sección ortótropa CYPE |
| `section ElasticMembranePlateSection %lu %11.9f %11.9f %11.9f %11.9f;` | Sección estándar de OpenSees (E ν h ρ), usada en cuadriláteros/otros |
| `nDMaterial PlateFiber %lu %lu;` + `section PlateFiber %lu %lu %11.9f;` | Capacidad no lineal (pushover): `nDMaterial J2Plasticity %lu E ν σy H0 0 …` |
| `element ShellCype %lu %s %s %s n1 n2 n3 n4 n5 n6 %lu;` | Triángulo de 6 nudos (por defecto) |
| `element ShellDKGQ %lu n1 n2 n3 n4 secTag;` | Cuadrilátero de 4 nudos |
| `element ShellNLDKGQ`, `ShellNLDKGT` | Variantes no lineales (capacidad) |
| `element ShellDKGQCypeBuckling`, `ShellDKGTCypeBuckling` | Variantes para análisis de pandeo |

- **[V]** `element ShellCype tag <A> <B> <C> n1..n6 secTag` con `<A>` = `-TCCL` (por defecto) o `-Quadratic` si la variable de entorno `NO_TCCL` está definida; `<B>` y `<C>` dependen del tipo de cálculo (`param_10`: 0 = estático/modal; 1 = pandeo primario/secundario; 2..5 = otros): pandeo primario `-IsPrimaryElementBuckling -assemblyStiffnessMatrixAlways`, resto `-IsSecondaryElementBuckling -assemblyStiffnessMatrixOnce`. Los seis `%s` de nudos se rellenan con `FUN_18009d180` (etiquetas de nudo, posible `-1` para nudo ausente [N]).
- **[N]** «TCCL» no se expande en el binario; por el contexto («Quadratic» como alternativa) es la interpolación del triángulo de 6 nudos. No se puede afirmar nada más sin el código del elemento.
- **[V]** Resultados por elemento: `recorder Element … stresses` / `stressesNodes` (esfuerzos por punto de Gauss y por nudo, ficheros `itri6n.cyres` etc.), y para láminas multicapa `material k fiber j stresses` (k = punto de integración 1..4, fibras 1 y 3 = caras inferior/superior [N]); también `section forces`, `section deformations`. Mensajes: «Reading shell forces» (#214007), «Processing shells» (#214012), «Reading forces of 4-node quadrilaterals» (#320276), «Reading contact pressures» (#320353).

## 3. Material y espesor: parámetros de la sección [V]

`cyoslamina_crea_lamina` guarda una estructura de lámina (0xF8 bytes) con, entre otros: identificador, espesor `h` (+0x5C), indicador (+0x64), un doble en +0x68, puntero a material (+0x70), **ocho factores de rigidez** (+0x78 … +0xB0, en este orden: Fxx, Fxy, Fyy, Mxx, Mxy, Myy, Vxz, Vyz [N: el orden concuerda con el del cuadro de diálogo; el binario no los nombra]), un doble en +0xB8, y propiedades de tendón.

Material (`cyoslamina_crea_material_isotropo/ortotropo`):
- Isótropo (tipo 0, 0x5C bytes): `E` (+4), `ν` (+0xC), dilatación térmica `α` (+0x14); se validan `E > 0`, `coef_poisson > 0`, `espesor_lamina > 0` (aserciones internas de `cyostriangcom.c`).
- Ortótropo (tipo 1): al menos `α1` (+0x4C) y `α2` (+0x54) (las otras 15 constantes son los 17 doubles de la sección ortótropa; el significado campo a campo [N]: E1, E2, ν12, G12, G13, G23, ρ… coherente con el diálogo «Genérico ortótropo»).
- Tipo 1 en el selector `FUN_1800814e0` del primer parámetro (`*material == 1`) devuelve tags −1 (sin sección elástica; es el caso de material «no elástico/rígido», p. ej. láminas rígidas [N]).

Línea de sección isótropa de la lámina (`FUN_18009b9f0`, [V]):

```
section ElasticMembranePlateSectionCype tag  E  nu  h  X  f1 f2 f3 f4 f5 f6 f7 f8  r11 r12 r13 r21 r22 r23 r31 r32 r33;
```

- `E` en kN/m² (el material se pasa convertido [N: unidades del script no verificadas; el clon trabaja en kN y m]), `h` en m.
- `X` = el doble de +0xB8 de la lámina (solo en cálculo estático/pandeo, 0 en otros tipos). **[N]** Candidato razonable: el «módulo de balasto en Z local» (en la lámina el clon tiene `ballastEnabled/ballastKnM3/ballastBehaviour`); no hay prueba.
- `f1..f8` = los 8 factores de rigidez, tal cual (multiplican las rigideces de membrana, flexión y cortante de la sección). **[N]** Su efecto exacto lo implementa `ElasticMembranePlateSectionCype` (no leído); lo natural: Fxx, Fxy, Fyy multiplican las componentes de la matriz de membrana `D_m`, Mxx, Mxy, Myy las de flexión `D_b`, Vxz, Vyz las de cortante transversal.
- Los 9 últimos números son la **matriz identidad 3×3** cuando se define el material con ejes locales de la lámina (`1,0,0, 0,1,0, 0,0,1`): matriz de orientación de los ejes del material respecto a los de la lámina (para ortotropía girada) [N: semántica; el valor identidad es [V]].
- Con los 8 factores todos = 1 (`FUN_180085340` devuelve 1) se crean **una sección elástica y una sección «rígida»** compartidas por toda la lámina (`tag_material_triangulos_elasticos`, `tag_material_triangulos_rigidas`). La rígida es la misma con **E × 100** (`FUN_18009b9f0(E*100, …)`, solo si algún triángulo cumple la condición `FUN_1800843a0`, que corresponde a las zonas rígidas `cm3dmalla_crea_zona_rigida_lamina` [N]). Con algún factor ≠ 1 las secciones se crean por triángulo/zona (tags −1 en la compartida) [N: interpretación de la ramificación; el código [V]].
- **[V]** Peso propio de la lámina: carga superficial `peso_específico × espesor` (ver `solver_lineal_portico.md` §6).
- **[V]** Valores por defecto y rangos de los diálogos de lámina en el clon ya medidos (`shell_material.h`, `shell_properties.h`, `docs/research/ui/laminas.md`, `propiedades_laminas.md`): espesor por defecto 100 mm (0,10–10000), ν 0,01–0,49, factores 0–100 (def. 1), lado máx. del triángulo 0,01–100 m (def. 1 m).

## 4. Mallado (`calcmallad3d.dll`) [V código / N algoritmo]

- **[V]** Tipos: `cm3dmalla_lamina_t` (lamina + huecos `cm3dmalla_crea_hueco_lamina` + zonas rígidas `crea_zona_rigida_lamina`), `cm3dmalla_barra_t`, `cm3dmalla_tendon_t`; genera `i_m3d_mallado_lamina_t` (nudos de malla + triángulos), `i_m3d_mallado_barra_t` (barra dividida en tramos por nudos de malla) y mallado de tendones. Función principal `cm3dmalla_genera_mallado_de_elementos_modelo3d`; variante `cm3dmalla_genera_mallado_barras_pandeo_sin_laminas` (barras para pandeo sin láminas).
- **[V]** Precisión de coincidencia de nudos: `cm3dmalla_precision_nudos_iguales` = 0,001 m (solver_lineal §1); tolerancias internas 1e-6 m (puntos 3D iguales) y `i_PRECISION_PUNTO_MISMO_PLANO_MALLADO` (distancia de un nudo al plano de la lámina).
- **[V]** Aserciones útiles para el modelo del backend: cada lámina tiene `num_triangulos > 0`; `area_hueco > 0`; el número de contornos 2D (planos sin huecos) = número de planos; contornos y huecos se tratan en el plano de la lámina.
- **[V]** Nudos de malla: `es_nudo_original` / `id_nudo_original` (los de la obra se conservan), `indices_laminas`, `indices_triangulos_laminas`, `indices_vertices_triangulos_laminas`, `W_triangulos_laminas` (vector normal/eje W de cada triángulo en el nudo), `desps_globales_laminas_nudo`, `articulacion_laminas_conectadas_en_nudo` (nudo con láminas articuladas entre sí), `indices_aristas_vinc_ext`, `son_aristas_vinc_ext_con_coacc_elastica`, `anchos_banda_aristas_vinc_ext` (**ancho de banda de una arista con vinculación exterior**: convierte un apoyo elástico puntual en lineal, `factor_conversion_coaccion_elastica_nudo_malla_original_puntual_a_lineal`).
- **[V]** Uniones con barras: nudos de barra divididos (`indices_barras_divididas_por_nudo`, `id_barra_en_lado` de la arista): una barra que pasa por un lado de la lámina comparte los nudos de malla del lado, y se divide en tramos; `longitudes_rigidas_barra_sobre_long_definicion` y `coords_barra_elastica_sobre_nudos_definicion` ligan las zonas rígidas con la malla.
- **[V]** Vinculaciones de arista: `crea_vinc_ext_arista_lamina` (apoyo exterior de la arista) y `crea_vincint_arista_{libre,empotrada,articulada,generica}` (vinculación interior de la arista = «Vinc. interior» de la ficha; por defecto empotrada), con `relacion_gdl_vincint` (relaciones entre los dos nudos duplicados de la arista, que se traducen en `equalDOF`/muelles, como las relaciones de barra §4.4 del solver). Mensaje #64886: aristas de láminas con distinta vinculación exterior: el nudo se considera sin coacción.
- **[N]** Algoritmo de mallado (Delaunay/avance de frente), refinamiento, tratamiento de huecos, criterio de tamaño (`lado máximo del triángulo`: lo único configurable por lámina, 1 m por defecto) no están en cadenas; hace falta leer `cm3dmalla_genera_mallado_de_elementos_modelo3d` (no leído por coste). Para el clon: triangulación restringida de Delaunay con lado máximo = `ShellRecord::maxTriangleSideM`, respetando nudos de la obra, aristas con barras y huecos; los nudos intermedios de una arista se comparten con la lámina vecina (conformidad).

## 5. Cargas sobre láminas [V formatos / N semántica de campos]

| Carga | Comando Tcl (`cyopensees.dll`) | Notas |
|---|---|---|
| Superficial (uniforme/variable/peso propio/dirección Z global/perpendicular) | `eleLoad -ele %lu -type -uniftrappressure p1 p2 p3 %lu` | Los tres valores se obtienen como `factor × componente` (de la función `FUN_180084e40`: `param_6 × param_2/3/4`): son **tres presiones**, una por vértice del triángulo (carga «trapezoidal»; uniforme = tres iguales), y el último entero es la dirección/eje de la presión [N: 0/1/2 local o global]. Funciones `cyoslamina_append_carga_dir_z_global_lamina`, `…_perpendicular_lamina`, `cyostriang6n_append_carga_dir_z_global/perpendicular` |
| Masa para análisis modal | `eleMass -ele %lu <opciones> -type -uniftrappressure …` | Mismos tres valores |
| Térmica uniforme isótropa | `eleLoad -ele %lu -type -tempinc ΔT α` | `ΔT` = factor × valor de la carga; `α` = dilatación del material (+0x14) [V] |
| Térmica ortótropa | `eleLoad -ele %lu -type -OrthotropicTempInc ΔT α1 α2` | α1, α2 del material ortótropo [V] |
| Retracción | `eleLoad -ele %lu -type -RetractionInc ε` | `ε` = factor × valor; deformación impuesta isótropa de membrana [V formato / N efecto] |

- **[V]** Las cargas térmicas son **temperatura uniforme** (no gradiente) sobre láminas: `cyoslamina_append_carga_termica_uniforme_lamina` y `cyostriang6n_append_carga_termica_uniforme_triangulo`; coincide con `ShellLoadType::UniformTemperature` y `Shrinkage` del clon (shell_load.h, 5 tipos: uniforme, variable X, variable Y, temperatura uniforme, retracción). Peso propio: `peso_específico × espesor`.
- **[V]** Cargas puntuales sobre lámina: `cm3dmalla_append_carga_puntual_a_lamina` (se reparte a los nudos de la malla) y de superficie `cm3dmalla_append_carga_superficial_a_lamina`; cargas de panel (`SurfaceLoads` del clon) se transforman a barras/nudos antes (T-P8 de paños).
- **[N]** Fórmulas de las cargas equivalentes nodales del triángulo de 6 nudos: se obtienen integrando las funciones de forma (para presión uniforme p en triángulo de área A: esquinas 0, puntos medios p·A/3, es decir, **cero en esquinas y A/3 en medios** para el triángulo cuadrático estándar; para el de 3 nudos, p·A/3 en cada nudo).

## 6. Bandas de integración (`calcmef.dll`) [V]

Una banda de integración es un **rectángulo** definido por su línea central (dos puntos 2D en ejes locales de la lámina) y su ancho (por defecto 15 cm; el clon ya la modela, `ShellIntegrationBand`). Sobre ella se **integran los esfuerzos por unidad de ancho de los elementos** para obtener resultantes lineales por banda (kN, kN·m), que son las que se dimensionan (armado de losas/muros) y se muestran en gráficos de bandas.

- **[V]** Entrada de la integración (`cefbandint_integra_esfs_todas_hipos_en_punto_banda`): `ancho_banda_max > 0`, punto de la banda, coordenadas locales, método (0 = esfuerzos nodales / 1 = tensiones internas; es el selector «Integración de esfuerzos en bandas: método» del clon, `BandForceMethod`), indicador de suavizado (`smoothedStresses`) y lista de hipótesis con sus escalones (`num_escalones_por_hipotesis`: pasos de carga de cada hipótesis, uno para cálculo lineal).
- **[V]** Ancho en cada punto (`cefbandint_anchos_banda_integracion_en_punto`): se calculan dos anchos, **izquierdo y derecho** (`ancho_banda_izq`, `ancho_banda_der`) a cada lado del eje, recortados al contorno de las láminas (la banda se corta donde acaba la lámina o donde empieza un hueco); al menos uno debe ser > 0 (aserción «ancho_banda_izq > 0. || ancho_banda_der > 0.»). Nominalmente cada uno es medio ancho (`−param_5` y `param_4`: semianchos).
- **[V]** Puntos extremos: si en un punto extremo (inicio/fin de la banda) ambos anchos < 0,001 m (el punto cae sobre el borde), se desplaza el punto hacia el interior a lo largo de la banda `min(0,01 · param_9, 0,01 m)` (1 % de la distancia, máximo 1 cm); si aun así cae fuera, se proyecta con `FUN_180017890` sobre el contorno. Tolerancias: 1e-3 en esfuerzos de fuerza, 1e-4 en momentos al comparar integración por izquierda y por derecha (rutina de comprobación de coherencia, `|N_izq − N_der| ≤ 1e-3`, `|M_izq − M_der| ≤ 1e-4`).
- **[V]** Cada punto de banda se evalúa en dos lados (pasadas con `FUN_18001abf0` a izquierda y derecha del eje; los 6 esfuerzos resultantes: tres de fuerza (Nx, Ny, Nxy) y tres de momento (Mx, My, Mxy), más los dos cortantes) y se **gira al sistema de la banda** con `FUN_18001ca70` y `ceftransesf_*` (`ceftransesf_transforma_fuerzas_lamina`, `…_momentos_lamina`, `…_tensor`, `…_matrices_trans_esfs_sr1_a_sr2` = rotación 2D entre el sistema de la lámina y el de la banda).
- **[V]** Resultado por banda y por hipótesis: seis series (`local_1b8 … local_190`): fuerzas Nn, Nt (normal y tangencial a la banda), Ntn (rasante), momentos Mn, Mt, Mtn [N: nombres exactos; son 6 magnitudes por punto: dos axiles, un rasante, dos flectores, un torsor]; `cefbandint_integra_esfuerzos_todas_las_hipotesis`, `…_en_punto_banda_laminas` (variante con varias láminas en el mismo plano, banda que cruza láminas).
- **[V]** Tensiones en caras (`cefbandint_calcula_tensiones_en_caras`, exacto): con espesor `h > 0`, esfuerzos Nx, Ny, Nxy, Mx, My, Mxy, Vxz, Vyz:
  - cara superior: `σx = Nx/h + 6Mx/h²`, `σy = Ny/h + 6My/h²`, `τxy = Nxy/h + 6Mxy/h²`, `τxz = Vxz/h`, `τyz = Vyz/h`;
  - cara inferior: las mismas con `−6M/h²`; `τxz` y `τyz` no cambian.
- **[V]** `cefbandint_tension_von_mises(_ex)(σx, σy, τxy, τxz, τyz) = √(σx² − σxσy + σy² + 3(τxy² + τxz² + τyz²))` (el 3 multiplica la suma de los tres cuadrados de tangenciales).
- **[V]** `cefbandint_vectores_tension_princ_elemento`: direcciones principales por elemento (tensiones principales y vectores para el dibujo de isovalores/«tensiones principales»).
- **[V]** Posproceso de dimensionado: `cefcal_*`, `cefpunto_*`, `cefnorma_*` (T-P8-07 y fase de armado) usan las resultantes por banda.
- **[N]** Cómo se «suaviza» (`smoothedStresses`) y cómo se pasa de tensiones por punto de Gauss a nudos/banda: no leído (`FUN_18001bff0`, `FUN_18001b980`, `FUN_18001abf0`). Hipótesis razonable para el clon: esfuerzos nodales promediados entre elementos contiguos (suavizado) o extrapolados por elemento (no suavizado), y la integración en la banda como `∫ N dw / ∫ dw` ancho total (resultante por unidad de longitud de banda = esfuerzo medio por unidad de ancho × ancho de banda ÷ ancho), de modo que una banda dé **esfuerzos por metro de ancho** (kN/m, kN·m/m) [N] y no resultante total. Hay que medirlo en el original (§9).

## 7. Convenio de signos y ejes de la lámina [N / V parcial]

- **[V]** Ejes locales de la lámina: x, y en el plano, z perpendicular (`shellLocalAxes` ya en el clon; `W_triangulos_laminas` en la malla). Esfuerzos por unidad de ancho: membrana Nx, Ny, Nxy (kN/m), flexión Mx, My, Mxy (kN·m/m), cortante Vxz, Vyz (kN/m). Las tensiones usan `+6M/h²` en la cara superior (z positivo): por tanto **un momento M positivo comprime/tracciona la cara +z con signo +** (tracción positiva en la cara superior con M positivo) [V para la fórmula; la convención física «Mx > 0 tracciona la cara +z» es consecuencia directa].
- **[N]** Signo de Nx, Ny y de los cortantes en pantalla (listados y gráficos) y si los diagramas de bandas invierten el signo del flector: sin verificar (T-P8-06); se resolverá con la medición del original.

## 8. Referencias analíticas para validar el elemento del clon

Convención de unidades: kN, m. Datos para el caso placa: E = 30 000 MPa (30 000 000 kN/m²), ν = 0,3, h = 0,2 m, lado a = 4 m, q = 10 kN/m². Rigidez de flexión `D = E·h³/(12(1−ν²)) = 30e6·0,008/(12·0,91) = 21 978,0 kN·m`.

### 8.1 Placa cuadrada simplemente apoyada con carga uniforme (Navier / Timoshenko)

Apoyo simple en los cuatro lados (w = 0 en los bordes; giro libre; membrana libre en el plano salvo restricción de cuerpo rígido).

| Magnitud | Fórmula | Valor |
|---|---|---|
| Flecha en el centro | `w = 0,004062·q·a⁴/D` | `0,004062·10·256/21 978,0 = 4,731e-4 m` |
| Momento en el centro | `Mx = My = 0,0479·q·a²` (ν = 0,3; en general `0,03684(1+ν)·q·a²`) | `0,0479·10·16 = 7,664 kN·m/m` |
| Reacciones | Σ reacciones = q·a²; las esquinas tienden a levantarse (fuerza de anclaje de esquina `R = 0,065·q·a²`, Timoshenko, ν = 0,3) | Σ = 160 kN; R de esquina = 10,4 kN |

Tolerancia propuesta: flecha y momento al 1 % con malla fina (lado 0,25 m en una cuarta parte con simetría o 0,125 m en toda); el cortante transversal (placa gruesa, h/a = 0,05 → corrección ≈ +0,6 % [N]) es despreciable. Comprobar equilibrio: Σ reacciones = q·a².

### 8.2 Viga pared (membrana plana, tensión plana)

Voladizo de pared: longitud L = 4 m, canto H = 2 m, espesor t = 0,2 m, E = 30e6 kN/m², ν = 0,2, carga P = 100 kN vertical repartida en el canto en el extremo libre, empotrada la sección del extremo opuesto (uy y ux bloqueadas en todo el borde).

- Flecha del extremo de la teoría de vigas con deformación por cortante: `δ = P·L³/(3·E·I) + P·L/(κ·G·A)` con `I = t·H³/12 = 0,13333 m⁴`, `A = t·H = 0,4 m²`, `G = E/(2(1+ν)) = 12,5e6 kN/m²`, `κ = 5/6`:
  `δ = 100·64/(3·30e6·0,13333) + 100·4/(0,8333·12,5e6·0,4) = 5,333e-4 + 9,6e-5 = 6,293e-4 m`.
- **[N]** La solución exacta de tensión plana difiere de la de viga (L/H = 2); tolerancia del 5 % entre el elemento y esta referencia para malla fina, y converger monótonamente al refinar. Para una referencia sin ambigüedad: viga simplemente apoyada de luz 6 m y canto 0,6 m (L/H = 10), `δ ≈` fórmula de viga con tolerancia 2 %.
- Sin carga transversal: comprobar Nx lineal en el canto (`σx = 6M/(t H²)` en bordes), y que las fuerzas de membrana de una banda vertical integrada coincidan con `M/z`.

### 8.3 Comprobaciones adicionales

- Patch test de membrana (deformación constante) y de flexión (curvatura constante), exacto a 1e-9 en una malla distorsionada de 3 y 6 nudos.
- Equilibrio de nudo con láminas y barras comunes (una viga de borde con losa): el desplazamiento en la zona común coincide en barra y lámina.
- Banda de integración: integrar el campo de momentos de Navier sobre una franja y comparar con la suma de elementos (el momento total de la sección central de la placa de §8.1 es la integral de Mx a lo largo del eje y; no se da valor cerrado aquí [N]).

## 9. Medido en el original (2026-11-12, CYPE 3D v2027.a, licencia activada) y pendiente

### 9.0 Resultados de la medición [V]

Obra de prueba: `m6_lam.txt` (lámina 4x4 m) importada en obra vacía, **barras borradas** (solo lámina), material «Genérico, isótropo» E = 30000 MPa, ν = 0,30, espesor 200 mm, Vinculación exterior de las 4 aristas = «Articulación» (icono fila 2, 1.º: en aristas solo Dx, Dy, Dz fijos), carga uniforme de lámina (Carga > Láminas > Nueva, ejes locales, icono de sentido 1 = **+z local**) con **Q = 10000 kN/m²** para obtener 5 cifras (la lectura da 2 decimales; con Q = 10 solo salía «0,26 mm»). El peso propio de la lámina sigue activo (γ = 24,53 kN/m³ por defecto, **0,2 · 24,53 = 4,906 kN/m² en −z**, hipótesis «Peso propio»; la carga del diálogo se creó en esa misma hipótesis porque el combo no es operable por script), de modo que **Q neto = 10000 − 4,906 = 9995,094 kN/m²**. Capturas `docs/screenshots/lam_*`.

**Referencias exactas** (serie de Navier, a = 4, D = 21978,0, Q neto): w_K = 472,95 mm, Mx = My = 7658,1 kN·m/m (centro); con cortante Mindlin (κ = 5/6, G = E/2(1+ν)) w = 479,08 mm.

| Malla (lado máx.) | w máx (Dz) mm | Mx máx kN·m/m | My máx | Mxy mín / máx | Nota |
|---|---|---|---|---|---|
| 2,0 m | 537,65 | 8022,8 | 7992,1 | −4232 / 3800 | malla muy basta |
| 1,0 m (defecto) | 501,16 | 7887,5 | 7881,1 | −5015 / 4220 | |
| 0,5 m | 501,73 | 7927,1 | 7893,3 | −5153 / 4917 | |
| 0,25 m | 498,09 (punto central: 496,44) | 7886,9 (centro: 7857,5) | 7879,7 (centro: 7867,5) | −4905 / 4851 | en el centro Mxy = 18, Qx = −304, Qy = 627: ruido de malla no estructurada (deberían ser 0) |

Los extremos son los de la leyenda con «esfuerzos alisados» activo (valores nodales alisados).

Lectura:
- **Signo coherente con la carga**: carga +z ⇒ Dz > 0 y Mx, My > 0 en el centro (Mx positivo = `M = −D·∂²w/∂x²`, convenio estándar de placas). El peso propio solo da Dz mín = −0,24 mm (Q = 4,906; referencia Mindlin 0,235 mm). La correspondencia con tracción en la cara superior (§6) no se comprobó en pantalla.
- **Converge rápido con la malla** (1,0 m → 0,25 m: −0,6 % en w, ≈ 0 en Mx; con 2,0 m +7 % en w), pero **converge a un valor ≈ 4 % por encima de Navier–Mindlin en w (498 frente a 479) y ≈ 2,7 % por encima en M (7860–7890 frente a 7658)**, también con otro ν: con ν = 0,01 (malla 1,0) w = 545, Mx = 6178,6, My = 6157,9 frente a 524,5 y 5949,8 de la teoría (+3,9 % y +3,8 %). No es error de discretización ni depende de ν; **la causa no está resuelta** (hipótesis sin prueba: rigidez de cortante menor, o área/carga efectiva ligeramente mayor). Para el clon: objetivo exacto = estas cifras; tolerancias propuestas ±6 % en w y ±3,5 % en M frente a la teoría.
- **Mallado**: lado máximo 1,00 m por defecto (diálogo «Discretización»: «Las láminas se discretizan mediante una malla de triángulos, teniendo en cuenta las condiciones de contorno de cada lámina... realiza la compatibilidad entre mallas en las aristas de las láminas»; campo «Tamaño máximo de los lados de un triángulo», m). La malla es una **triangulación no estructurada** no simétrica; con 1,0 m en 4x4 m se ven del orden de 40–50 triángulos (visual, `lam_m10_full.png`, sin contar con exactitud).

**Factores de rigidez** (Propiedades > Láminas > «Factores de rigidez»; orden de los campos del diálogo: Membrana Fxx, Fxy, Fyy, Flexión Mxx, Mxy, Myy, Cortante Vxz, Vyz; el tooltip de la lámina los lista en el orden Fxx, Fyy, Fxy, Mxx, Myy, Mxy, Vxz, Vyz; defecto 1,000): con **Flexión Mxx = 0,5** (ν = 0,01, malla 1,0): w máx 545 → **626,41 mm (×1,149)**, Mx máx 6178,6 → **3457,8 (×0,56)**, My máx 6157,9 → 6992,9 (×1,136), Mxy ±7100–8100 (antes ±~4200). El factor multiplica la rigidez de flexión Dxx (no es un factor global) y los momentos se reparten hacia el eje y. Fxx = 0, los demás factores y el mensaje con 0 no se midieron.

**Parámetro X de la sección = módulo de balasto en Z local [V]**: con la casilla «Módulo de balasto en Z local» marcada y K = 1 000 000 kN/m³ (máximo del diálogo, rango 0,01–1 000 000) se obtiene w máx = **10,96 mm ≈ Q/K = 9,995 mm** (más la flexión residual) y Mx máx 333,6 (mín −175,1): cimentación Winkler en z local, en kN/m³ (defecto 29430,00, desactivada). «Sólo compresión» no medido.

**Esfuerzos y tensiones disponibles [V]**: Cálculo > Isovalores abre una **ventana propia** «3D v2027.a - [Desplazamiento total: Peso propio]» con menús Archivo (Terminar), Ver (Opciones), Esfuerzos (Consultar) y Ventana, panel «Opciones» y barra de leyenda inferior con los extremos. Modos «Deformada» (factor de escala 0–100000, defecto 10; «Valor máximo»; «Ver la estructura») e «Isovalores», con tres familias:
- **Desplazamientos**: Desplazamiento X, Y, Z, total; Giro X, Y, Z (mm, mRad).
- **Esfuerzos**: Axil X, Axil Y, Axil XY (kN/m); Flector X, Flector Y, Flector XY (kN·m/m); Cortante total, Cortante X, Cortante Y (kN/m).
- **Tensiones** (MPa): primer combo Cara superior, Cara inferior, Máxima de ambas caras, Mínima de ambas caras, Máxima absoluta de ambas caras; segundo combo Normal X, Normal Y, Tangencial XY, Tangencial XZ, Tangencial YZ, Tensión máxima, Tensión mínima, Tensión de Von Mises, Tensión tangencial máxima.

Casillas: «Mostrar esfuerzos alisados» (marcada por defecto; deshabilitada en desplazamientos), «Discretización» (dibuja la malla, marcada), «Dibujar los ejes de las láminas» (marcada), «Valor máximo/mínimo» (límites del colormap; defecto ±98100 en kN/m, kN·m/m y MPa; ±1 000 000 mm). Combo «Comb. seleccionada»: Hipótesis simple + hipótesis. **Esfuerzos > Consultar** (clic en un punto) da el valor en un globo con 2–3 decimales; con factor de escala > 0 el dibujo se deforma y el clic cae sobre el punto deformado (usar 0 para consultar). Axil X, Y, XY = 0 constante (placa sin membrana). Cortante total máx 84 498 (Q = 10000): en las esquinas (singularidad), no es el cortante de borde de Navier 0,338·Q·a = 13 520.

**Peso propio de láminas [V]**: carga superficial γ·h en −z (4,906 kN/m²): Dz = −0,24 mm (malla 0,25). El comando Carga > Láminas > «Peso propio» (diálogo «Generación de cargas»: Activar / Desactivar; luego selección + botón derecho) no pudo comprobarse: tras «Desactivar» el volumen amarillo de peso propio seguía dibujado y el cálculo lo seguía incluyendo.

**Bandas de integración [V parcial]**: Propiedades > Láminas > «Bandas de integración» (ayuda: «...definen líneas sobre las que, para un ancho de banda dado, se integran los esfuerzos de la lámina para obtener esfuerzos de barra... Puede consultar los esfuerzos en cada banda de integración como si se tratase de los esfuerzos sobre una barra de la estructura.»; solo se usa la geometría de la primera lámina seleccionada) abre el diálogo «Bandas de integración» (redimensionable): barra de herramientas con deshacer/rehacer, un icono de curva, **«Generar bandas de integración en zona»** (4 puntos que definen la zona), **«Nueva»** («Seleccione dos puntos dentro de la lámina y, a continuación, pulse el botón derecho»; al marcar el segundo punto se abre el diálogo «Nueva» con **«Anchura» = 15,0 cm** por defecto), tres iconos de editar/mover/borrar, tres de unir/dividir, un icono de aviso, y vista (zoom, ventana, ajustar, mano); opciones «Referencias de bandas» (cotas magenta) y panel «Isovalores» («Mostrar», Desplazamientos/Esfuerzos, hipótesis) con la leyenda sobre el plano de la lámina. **No se consiguió que la banda quedara creada** (tras Aceptar en «Nueva» no se dibuja ninguna banda y deshacer sigue inactivo): el valor integrado por banda queda sin medir.

**No medido**: viga pared (voladizo), método/suavizado de bandas, mensaje con factor de rigidez = 0, nº exacto de triángulos (TK-109).

**Notas de conducción nuevas**: la ventana Isovalores es otra ventana del proceso: `aclick` (clic absoluto) abre sus menús, `click` no; sus combos **sí** aceptan clic en la entrada de la lista (a diferencia de la ventana principal); en una obra sin barras el diálogo «Cálculo» no tiene «Comprobar las barras» y Aceptar está en (843,601), y no aparece informe; el panel «Hipótesis vista» aparece al tocar Carga (maximizado la primera vez: `sv_panel`) y hay que cerrarlo (507,249) antes de clicar la lámina; la lámina se selecciona clicando su interior (1491,520), las aristas en su punto medio. Helpers: `tools/lam_lib.sh` (lam_calc, lam_view, lam_sel, lam_q, lam_cap, lam_run).

### 9.1 Lista original de mediciones (referencia)

Obra de prueba en `docs/sample_project/`:
1. Placa cuadrada 4×4 m, h = 0,2 m, hormigón, apoyo simple en el contorno, q = 10 kN/m²: flecha y Mx, My en el centro (comparar con §8.1) con lado de triángulo 1 m y 0,25 m. Ayuda a decidir si el elemento es tipo MITC/DKT-like con cortante.
2. Misma placa con banda de 15 cm a lo largo del eje x en y = 2 m: valor integrado (kN·m/m o kN·m total) con método «tensiones internas» suavizado y sin suavizar y con método «esfuerzos nodales».
3. Efecto de cada uno de los 8 factores de rigidez (p. ej. Mxx = 0,5 → flecha cambia, Fxx = 0 → ¿error de matriz singular?).
4. Convenio de signos de Mx, My y de Nx en listados de láminas y en el gráfico de bandas.
5. Si el mallado respeta los puntos de carga puntual y las barras de borde, y si cambia con huecos (número de triángulos para el lado de 1 m en 4×4 m: dato objetivo).
6. Mensaje de error cuando la lámina tiene un factor de rigidez = 0 o la estructura es un mecanismo local por ese motivo.

## 10. Estado del clon (`src/core`) y huecos

Ya existe (datos, sin solver de láminas):
- `Shell{nodeIds, thicknessM, material (ShellMaterial: hormigón/acero laminado/conformado/aluminio/genérico isótropo/ortótropo), layout (desplazamiento del plano central), ballast, zAxisInverted, xEdge}` en `structure_model.h`, `shell_material.h`.
- `ShellProperties` (`shell_properties.h`): `maxTriangleSideM` (def. 1 m), 8 factores de rigidez, `ShellIntegrationBand{x1,y1,x2,y2,widthM (def. 0,15), forcePoints (3..1000, def. 10)}`, vinculación interior/exterior por arista, `BandForceOptions`, pandeo y modal. Operaciones de bandas (nueva, editar, unir, dividir, generar) con sus mensajes.
- `ShellLoads` (5 tipos), `Panels`/`SurfaceLoads` (paños). `ShellFrame` con ejes locales.
- `src/core/fem` (T-P8-01): `frame_model.h` / `frame_builder.cpp` / `linear_solver.cpp` solo barras; `FrameBuild::unsupported` lista láminas (TK-106).

Falta para T-P8-02:
1. Mallador de láminas (triangulación con lado máximo, huecos, nudos compartidos con barras y láminas vecinas, aristas con vinculación).
2. Elemento triangular (6 y 3 nudos) de placa+membrana con rigidez de drilling, sección con 8 factores, ejes de material, ortotropía.
3. Cargas (presión por nudos, temperatura uniforme, retracción, peso propio, masa).
4. Integración de esfuerzos en bandas y tensiones en caras (de §6, formulado cerrado ya conocido).
5. Ligaduras de arista (empotrada/articulada/libre/genérica) y apoyo exterior por arista (ancho de banda de arista).
6. Módulo de balasto Z (muelle, solo compresión es no lineal).

## 11. Especificación para backend (T-P8-02, C++20 + Eigen)

### 11.1 Estructuras (`src/core/fem/shell_*.h`)

```cpp
namespace cc::fem {
struct ShellSectionData {                 // per shell, already in kN/m
    double e, nu, thickness;              // isotropic; orthotropic via ortho{}
    std::array<double,8> factors{1,1,1,1,1,1,1,1}; // Fxx Fxy Fyy Mxx Mxy Myy Vxz Vyz (REA: ElasticMembranePlateSectionCype)
    bool orthotropic = false; struct { double e1,e2,nu12,g12,g13,g23,alpha1,alpha2,rho; } ortho{};
    double ballastKnM3 = 0; bool ballastCompressionOnly = false;
    std::array<double,9> materialAxes{1,0,0,0,1,0,0,0,1};   // identity = shell axes
    double alpha = 1e-5;                   // isotropic thermal expansion
};
struct ShellMeshNode { int id; Vec3 pos; bool original; int originalNodeId; };
struct ShellTriangle { int shell; std::array<int,6> n; bool quadratic = true; // false -> 3-node
                       Vec3 ex, ey, ez; bool rigid = false; };                  // rigid: E*100 zone
struct ShellLoadElem { int tri; std::array<double,3> pressure; int dir; double dT; double shrinkage; };
struct BandSpec { int shell; double x1,y1,x2,y2,width; int points; bool nodalMethod; bool smoothed; };
struct BandPointResult { std::array<double,2> w; double leftWidth, rightWidth; // clipped to contour
                         std::array<double,6> forces; };                        // Nn Nt Ntn Mn Mt Mtn per unit width
}
```

### 11.2 Algoritmo

1. **Malla**: triangulación restringida de cada lámina (contorno 2D en el plano con huecos) con lado máximo `maxTriangleSideM`; nudos de la obra conservados (tolerancia 0,001 m); nudos de arista compartidos con láminas/barras vecinas; aristas con barra dividen la barra en tramos; zonas rígidas (E×100) cuando se pidan. Preferencia por el triángulo de 6 nudos (mismos nudos de esquina + puntos medios) y opción de 3 nudos (`CYOS_USE_TRI3N_SHELLS`).
2. **Elemento**: matriz de membrana `D_m = E h/(1−ν²) [[1,ν,0],[ν,1,0],[0,0,(1−ν)/2]]`, de flexión `D_b = E h³/(12(1−ν²))` (misma forma) y de cortante `D_s = κ G h` (κ = 5/6) **multiplicadas por los factores**: `Dm_ij·F_ij`, `Db_ij·M_ij`, `Ds·V` [N: asignación exacta]. Flexión con elemento tipo DKT/DKGT (sin bloqueo por cortante), `drilling` con rigidez artificial pequeña (`1e-3·G·h·A` por vértice) [N]. Transformación a ejes globales con (ex, ey, ez).
3. **Cargas**: presión → fuerzas nodales equivalentes por integración (esquinas 0 y medios A/3 en el de 6 nudos, A/3 en el de 3); térmica uniforme `N = E h α ΔT/(1−ν) · [1,1,0]` (isótropa); retracción igual con ε; peso propio `γ·h`.
4. **Resolución**: ensamblar con el modelo de barras de T-P8-01 (`fem::LinearModel` ampliado con `shells`, `meshNodes`) y resolver con el mismo LDLT; unir con barras por nudos comunes de malla.
5. **Recuperación**: esfuerzos por elemento (en puntos de Gauss/nudos, ejes de la lámina) → suavizar (promedio nodal) → bandas.
6. **Banda**: muestrear `points` puntos equiespaciados a lo largo del eje; en cada punto calcular `leftWidth`/`rightWidth` recortados al contorno (extremos desplazados `min(0,01·dist, 0,01 m)` hacia el interior), integrar `∫ s dw` sobre el rectángulo y rotarlo al sistema de la banda (`T(θ)`), obtener 6 esfuerzos.
7. **Tensiones en caras** y **Von Mises** con las fórmulas exactas de §6; tensiones principales por elemento con el tensor 2D de la cara.

### 11.3 API

```cpp
std::vector<cc::fem::ShellMesh> meshShells(const cc::core::Project&, const MeshOptions&);
void addShells(cc::fem::LinearModel&, std::span<const ShellMesh>, std::span<const ShellSectionData>);
ShellResults solveShells(...);                 // same solve as bars
std::vector<BandPointResult> integrateBand(const ShellResults&, const BandSpec&, std::size_t loadCase);
struct FaceStress { double sx, sy, txy, txz, tyz; };
FaceStress topFaceStress(const ShellForces&, double h);     // N/h + 6M/h^2 ; V/h
FaceStress bottomFaceStress(const ShellForces&, double h);  // N/h - 6M/h^2 ; V/h
double vonMises(double sx, double sy, double txy, double txz, double tyz);
```

Tests (T-P8-04 extendido): placa Navier §8.1 con 1 % de tolerancia, viga pared §8.2, patch tests, equilibrio de reacciones, fórmulas de tensiones en caras y Von Mises (valores cerrados), bandas con recorte contra el contorno y huecos.

## 12. Tickets sugeridos

- **TK-NEW-E (T-P8-02)**: medir en el original las magnitudes de §9 (bloqueado hasta que termine la medición del solver).
- **TK-NEW-F (T-P8-02)**: leer `cm3dmalla_genera_mallado_de_elementos_modelo3d` y las funciones de suavizado de `calcmef` (`FUN_18001bff0`, `FUN_18001b980`, `FUN_18001abf0`) para fijar mallado y suavizado.
- **TK-NEW-G**: leer la clase `ShellCype`/`ElasticMembranePlateSectionCype` del ejecutable OpenSees extraído (TK-007: solo análisis local) para fijar la rigidez de drilling y el efecto de los 8 factores.
