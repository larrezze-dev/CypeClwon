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

## 9. Pendiente de medir en el original (otro agente lo tiene en uso)

No lanzar el original desde este análisis. Mediciones a anotar cuando se pueda (obra de prueba en `docs/sample_project/`):
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
