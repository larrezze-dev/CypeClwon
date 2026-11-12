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

## 13. Pasada de medición 2026-11-12 (T-P8-02, cierre de TK-109) [V]

Obra de prueba: la placa 4x4 de §9.0 reconstruida desde `m6_lam.txt` (barras borradas, E = 30000 MPa, ν = 0,3, h = 200 mm, aristas «Articulación» (icono fila 2, 2.º del diálogo «Vinculación exterior»), carga Q = 10000 kN/m² local +z, peso propio activo; Q neto = 9995,094). Muro (viga pared): `docs/sample_project/lam_wall.txt` (lámina vertical 4 x 2 m en el plano XZ, h = 200 mm, E = 30000 MPa, ν = 0,3, arista x = 0 «Empotramiento», carga uniforme global −Z 1000 kN/m² sobre la superficie de la lámina). Capturas en `docs/screenshots/`: `band*`, `fx0_legs`, `fxm05_legs`, `fxy05_legs`, `fv01_legs`, `h400_legs`, `wall1_legs`, `wfxx05_legs`, `wfxy05_legs`, `sw*`, `mem0_d` (los `*_legs.png` apilan los extremos de las leyendas de Isovalores). Scripts de conducción: `tools/lam_lib.sh` (`lam_fact`, `lam_factm`, `lam_legs`, `lam_legsm`, `bq`) y `tools/lam_mont.py`.

### 13.1 Bandas de integración: cómo se crean y valores medidos

**Flujo de «Nueva» (aclara §9.0)**: botón «Nueva» (628,312) -> diálogo «Nueva» («Anchura» 15,0 cm) -> **Aceptar primero** -> **dos clics** (primer y segundo punto) y la banda se crea al segundo clic (los clics dados con el diálogo abierto se pierden; ése era el fallo de la pasada anterior). Banda centrada en y = 2 m, de x = 0 a 4 m, ancho 15 cm: el globo la llama «BI 1 (L1, A.B.: 15.0 cm)».

**Consulta**: Cálculo > Esfuerzos, «Sólo las barras seleccionadas» + «Consultar valores», seleccionar la banda (clic) y botón derecho: globo «BI 1 (L1, A.B.: 15.0 cm) / X: … m / N, Vy, Vz (kN), Mt, My, Mz (kN·m)». **Una banda es una barra con resultantes totales (kN, kN·m), no por metro**: My = Mx(placa)·ancho, Vz = Qx·ancho, N = Vy = Mz = 0 (placa sin membrana). Con el diálogo «Integración de esfuerzos en bandas» (icono 575,312 del diálogo de bandas) se elige el método.

Banda a lo largo de x en y = 2 m, malla 1,0 m (la de §9.0), Q neto 9995,094, h = 0,2 m; referencia = serie de Navier (Kirchhoff, a = 4, D = 21978) × 0,15 (My) y Qx × 0,15 (Vz):

| x (m) | Navier: My / Vz | **Esfuerzos internos, alisados (defecto)** | Internos sin alisar | **Fuerzas nodales** |
|---|---|---|---|---|
| 0,501 | 597,7 / 1350,9 | My −609,99; Vz 1434,295 | My −639,43; Vz 1389,030; Mt 98,78 | My −599,12; Vz 1152,630; Mt 348,18 |
| 0,994 | 930,4 / 823,5 | My −916,14; Vz 855,010; Mt −1,43 | My −927,77; Vz 867,111; Mt 12,52 | My −841,96; Vz 833,013; Mt −140,23 |
| 1,487 | 1096,6 / 393,8 | My −1090,62; Vz 399,314; Mt 5,81 | - | - |
| 1,995 | 1148,7 / 3,7 | My −1160,51; Vz −0,462; Mt −10,39 | My −1138,24; Vz −6,672; Mt 26,68 | **My −752,20**; Vz −70,922; Mt 293,94 |
| 2,991 | 937,5 / −809,3 | My −943,38; Vz −829,493; Mt −3,13 | - | - |

Lecturas: (1) el método por defecto reproduce el campo de momentos de la placa a ±2 % y el cortante a ±6 % (el extremo más cargado): el valor central de la banda (1160,5/0,15 = 7737 kN·m/m) cae entre Navier (7658) y el máximo nodal alisado de la leyenda (7857). (2) «Sin alisar» añade ±3 % de ruido. (3) **El método de fuerzas nodales es mucho peor con malla de 1 m y banda de 15 cm** (My central −752 frente a −1149 teóricos, Mt hasta 348): es una propiedad del método del original (equilibrio de una porción aislada) y el clon debe reproducirla, no corregirla. (4) Signo: **My de banda = −Mx de la placa** (con carga +z y Mx > 0 el listado de barras da My negativo); Vz = +Qx·ancho (positivo cerca de x = 0). (5) Mt de la banda ≠ 0 por el ruido de Mxy de la malla no estructurada (±10 kN·m con el método por defecto).

### 13.2 Causa del +4 % en w y +2,7 % en M: apoyo simple «blando» (RESUELTO) [V]

**No es un error del elemento ni de la carga.** La referencia correcta no es Navier-Mindlin con apoyo «duro», sino Reissner-Mindlin con apoyo simple **«blando»**: en la «Articulación» de arista (Dx, Dy, Dz fijos, giros libres) queda libre el giro tangencial del borde. Comprobado con un Q4 de Mindlin propio (32x32, cortante reducido, κ = 5/6, G = E/2(1+ν), misma obra):

| h (m) | FE blando (w central; Mx central) | FE duro (= Navier-Mindlin) | **Original** |
|---|---|---|---|
| 0,2 | 499,28 mm; 7907 | 478,94; 7658 | w máx 498,09 (centro 496,44); Mx máx 7886,9 (centro 7857,5) |
| 0,4 (q = 9990,19) | 67,15; 8146 | 62,14; 7654 | w máx **67,19**; Mx máx **8111** (My 8112) |

El sobrante crece linealmente con h/a (+4 % en w a h = 0,2; +8 % en w y +6 % en M a h = 0,4), como el apoyo blando, y el FE blando reproduce el original al 0,2 % en w. Para el clon: el elemento de Mindlin con 3 gdl por nudo y giros libres en los apoyos de arista ya da el valor del original; los tests de validación contra Navier deben usar el FE blando o una tolerancia de ≥ 6 % en w y 3,5 % en M a h/a = 0,05, no 1 %. El número de triángulos de la malla de 1,0 m: 16 en el muro de 4x2 m (visual, `w_i.png`), del orden de 40–50 en la placa de 4x4; no se contó con exactitud en la placa.

### 13.3 Factores de rigidez (Propiedades > Láminas > «Factores de rigidez») [V]

Orden de los campos: **Fxx, Fxy, Fyy, Mxx, Mxy, Myy, Vxz, Vyz** (rango 0..100, 3 decimales, defecto 1,000). Placa: malla 1 m, h = 0,2, ν = 0,3; referencia w máx = 501,16, Mx 7887,5, My 7881,1. Muro: referencia Dz punta −19,9, Dx 5,34.

| Factor | Valor | Resultado |
|---|---|---|
| **Mxx** | 0,5 | w máx **604,36** (×1,206); Mx máx 5143, My máx 8762, Mxy −6109 / 5193 |
| **Mxx** | 0 | **calcula sin mensaje**; **Mx ≡ 0** («Valor constante: 0 kN·m/m»); w máx **879,97** (×1,756); My máx 9549; Mxy −13347 / 12930 |
| **Mxy** | 0,5 | w máx **603,12** (×1,203); Mx 9551, My 9652, Mxy −2983 / 2530 (escala la torsión D66) |
| **Vxz = Vyz** | 0,1 | w máx **595,35** (+18,8 %); Mx −2317 / 8387; Cortante X −17186 / 15194 |
| **Fxx = Fxy = Fyy** | 0 | **calcula sin mensaje**; la placa (sin membrana) no cambia: w 501,16 |
| **Fxx** (muro) | 0,5 | Dz punta −35,77 (×1,797), Dx ±10,65 (×1,994) |
| **Fxy** (muro, 2.º campo) | 0,5 | Dz −23,94 (×1,203), Dx 5,40 (×1,011): escala el cortante en el plano (D33) |

**Ley deducida y comprobada [V por FE propio]**: `D' = S·D·S` con `S = diag(√Fxx, √Fyy)` en membrana (`√Mxx`, `√Myy` en flexión) y factor directo en el término de cortante/torsión (Fxy → D33; Mxy → D66): `D11' = Fxx·D11`, `D22' = Fyy·D22`, **`D12' = √(Fxx·Fyy)·D12`**, `D33' = Fxy·D33`. Un Q4 de tensión plana propio con esa ley da en el muro Dz ×1,797 y Dx ×1,993 con Fxx = 0,5 (original 1,797 y 1,994); las alternativas (D12 sin escalar: ×1,953 / ×2,196; D12·F: ×1,730 / ×1,906) quedan descartadas. En flexión con Mxx = 0,5 la ley da w ×1,199, Mx ×0,641, My ×1,123 (original ×1,206; 0,652; 1,112). Con Mxx = 0 la fórmula de series da w ×1,61 y el original ×1,756 (malla de 1 m con rigidez de flexión nula en una dirección: no se afinó). **No hay mensaje de error con factor 0** en estos casos (la lámina sigue estable por los demás términos y, sin membrana, los gdl de membrana ya estaban sin rigidez en la placa: el solver no avisa). El mensaje por mecanismo local debido a un factor 0 no se logró provocar con una placa apoyada en todo el contorno y queda abierto (ver TK-109).

### 13.4 Viga pared (muro en voladizo 4 x 2 m) [V]

Carga uniforme global −Z de 1000 kN/m² sobre la **superficie de la lámina** (no por proyección) + peso propio 4,906 kN/m² = línea de 2009,8 kN/m. Original (malla 1 m, 16 triángulos): Dz punta **−19,90 mm**, Dx ±5,34 (esquinas del extremo libre), Nx −25789 / +29526 kN/m (borde empotrado), Ny −4169 / 8858, Nxy −6532 / 180, cortante transversal ≡ 0. Referencias: viga con cortante (κ = 5/6) 16,08 + 4,18 = 20,26 mm; Q4 de tensión plana propio 64x32 con incompatibles: **−20,02 mm y Dx 5,36** (acuerdo del 0,6 %).

### 13.5 Peso propio de lámina: «Desactivar» sí funciona [V]

Carga > Láminas > «Peso propio» abre «Generación de cargas» (Activar / Desactivar); tras Aceptar hay que **seleccionar la lámina con una ventana de dos clics (esquina 1, esquina 2) que la envuelva y pulsar el botón derecho**; un clic en el interior de la lámina no la selecciona con esta herramienta (por eso fallaba antes). Al aplicarlo **desaparece el volumen amarillo del peso propio** y el cálculo lo excluye (muro: Dz −23,94 -> −23,82 = −0,50 % = 4,906/1004,906). **Corrige §9.0 y `solver_medidas_original.md` §9.5**: la herramienta «Peso propio» actúa por selección.

## 14. Implementación en el clon (T-P8-02, 2026-11-12)

Código: `src/core/fem/shell_model.h` (datos y convenios), `shell_analysis.h` (malla, bandas, API pública), `shell_mesh.cpp`, `shell_element.h/.cpp` (elemento), `shell_bands.cpp` (bandas, tensiones en caras), `linear_solver.cpp` (ensamblaje con las barras), `frame_builder.cpp` (adaptador desde la obra). Tests: `tests/fem_shell_tests.cpp`. Prototipo previo en Python: `docs/research/data/t6_mindlin_proto.py`.

Decisiones (marcas como en el resto del documento):
- **Malla [N]** (**sustituida en §17** por Delaunay + bisección de §15.1): triangulación estructurada en triángulos rectos de 6 nudos; cada arista recibe `n = ceil(L / lado máximo)` divisiones (la mayor exigencia de las láminas que la comparten) y se igualan las aristas opuestas del cuadrilátero y las tres del triángulo, propagando entre láminas vecinas (mallas conformes). Diagonales alternas. Reproduce el único recuento exacto medido (16 triángulos en el muro 4 x 2 con 1 m); la placa 4 x 4 da 32 triángulos (el original, no estructurado, ~40–50 a ojo). Barras sobre una arista: se dividen internamente en los nudos de malla de la arista (resultados por barra). Nudos de la obra dentro de una lámina que no son vértices: se informan en `FrameBuild::unsupported` (no conectados).
- **Elemento [N, validado]**: membrana de tensión plana cuadrática + placa de Reissner-Mindlin cuadrática con deformaciones de cortante supuestas tipo MITC6a (dos puntos de atadura por lado + centroide), integración 3 puntos (membrana/flexión) y 6 puntos (cortante, *drilling*, balasto, cargas). *Drilling*: penalización `1e-3·G·h·Fxy·∫(θz − ω)²` (0 sin rigidez de membrana a cortante: los gdl quedan sin rigidez y se bloquean, como en el original «sin mensaje»). Balasto: muelles de Winkler `K ∫ N Nᵀ` en w local.
- **Factores [V ley §13.3]**: `D' = S·D·S` en membrana y flexión, Fxy → D33, Mxy → D66, Vxz/Vyz escalan κGh.
- **Suavizado [N]**: recuperación por parches (Zienkiewicz-Zhu) desde los 3 puntos de Gauss de los triángulos que rodean el nudo (cuadrática con ≥ 4 triángulos, lineal si no). El promedio nodal simple da +12 % en Mx en el centro de la placa con malla de 1 m.
- **Bandas [V signo; N método de fuerzas nodales]**: resultantes totales como en una barra (`applySignConvention`): ejes de banda x = eje de la banda, y en el plano, z = z de la lámina; `My = −∫M_tt`, `Vz = +∫Q_t`, `N = ∫N_tt`, `Vy = ∫N_tn`, `Mt = ∫(η Q_t − M_tn)`, `Mz = −∫η N_tt` (integración Gauss compuesta en el ancho recortado por el contorno). Punto extremo en el contorno con ambos anchos < 1 mm: se desplaza `min(1 % L, 1 cm)` [V]. Método «fuerzas nodales»: fuerzas nodales de elemento de los triángulos con centroide antes del corte, en los nudos compartidos con los de después que caen dentro del ancho de banda.
- **Cargas**: presión por m² de superficie (ejes globales o locales, campo afín para las variables), temperatura uniforme `α·T` de membrana, retracción `−ε` de membrana ([N] signo), peso propio `γ·h` en −Z global salvo «Desactivar» [V].

Validación (valores reales del C++, impresos por `tests/fem_shell_tests.cpp` como «[value]»; malla 1 m salvo indicación):

| Caso | Clon | Referencia | Desvío |
|---|---|---|---|
| Placa h 0,2: w centro (= máx) | 507,28 mm | original 501,16 / Mindlin blando 499,28 | +1,22 % / +1,60 % |
| Placa h 0,2: Mx centro (= máx, suavizado) | 8074,5 | original máx 7887,5 | +2,37 % (frente a Navier 7658: +5,4 %, fuera del ±3,5 % de la tarea con malla 1 m) |
| Placa malla 0,5 m: w / Mx centro | 499,07 / 7921,1 | Navier 472,95 / 7658,1 | +5,52 % / +3,43 % (dentro de ±6 % / ±3,5 %) |
| Placa malla 0,25 m: w / Mx centro | 499,87 / 7916,9 | original 498,09 / 7857,5 | +0,36 % / +0,76 % |
| Placa h 0,4: w máx / Mx máx | 68,51 / 8317,8 | original 67,19 / 8111 | +1,96 % / +2,55 % |
| Peso propio solo: Dz mín | −0,249 mm | original −0,24 (2 decimales) | +3,8 % |
| Mxx 0,5: razones w / Mx / My | 1,199 / 0,647 / 1,125 | 1,206 / 0,652 / 1,112 | −0,6 / −0,8 / +1,2 % |
| Mxx 0: Mx ≡ 0 sin mensaje; razón w | 1,612 | 1,756 | −8,2 % (fórmula de series ×1,61; no afinado) |
| Mxy 0,5 / Vxz = Vyz 0,1: razón w | 1,199 / 1,205 | 1,203 / 1,188 | −0,4 / +1,4 % |
| Fxx = Fxy = Fyy = 0 en placa | w igual, sin mensaje | igual [V] | 0 |
| Muro: Dz punta / Dx máx | −19,75 / 5,345 mm | original −19,90 / 5,34; Q4 propio −20,02 | −0,75 % / +0,1 %; −1,35 % |
| Muro Fxx 0,5: razones Dz / Dx; Fxy 0,5: Dz | 1,801 / 1,992; 1,200 | 1,797 / 1,994; 1,203 | +0,2 / −0,1; −0,3 % |
| Peso propio desactivado (muro) | razón 0,9951 | original 23,82/23,94 = 0,9950 | +0,01 % |
| Balasto K = 1e6: w máx | 11,08 mm | original 10,96 | +1,06 % |
| Banda y = 2, My alisado en x = 0,994 / 1,487 / 1,995 / 2,991 | −942,8 / −1074,6 / −1205,2 / −949,6 | −916,1 / −1090,6 / −1160,5 / −943,4 | +2,9 / −1,5 / +3,9 / +0,7 % (centro frente a Navier −1148,7: +5,0 %) |
| Banda, Vz alisado en 0,994 / 1,487 / 2,991 | 935,0 / 339,7 / −918,2 | 855,0 / 399,3 / −829,5 | +9,4 / −14,9 / +10,7 % (cortante: el campo más ruidoso con malla basta) |
| Banda sin alisar, My en 0,994 | −977,5 | −927,8 | +5,4 % |
| Banda, fuerzas nodales | solo se comprueba el signo (My < 0) | −841,96 (0,994), −752,20 (centro) | cifras no reproducidas (dependen de la malla del original) |
| Banda de 1 m, malla 0,25: fuerzas nodales frente a tensiones | −7342,7 / −6626,5 | coherencia interna | +10,8 % |
| Patch tests de temperatura (libre / coaccionada), equilibrio de reacciones, viga de borde con losa (flecha 5qL⁴/384EI) | exactos / −1,2 % | analítico | |

Abierto: algoritmo de malla y suavizado exactos del original (TK-NEW-F, no leído), cifras del método de fuerzas nodales (dependen de la malla del original), mensaje por mecanismo local con factor 0 (TK-109), excentricidad del plano central, balasto «Sólo compresión», vinculaciones de arista elásticas/oblicuas/interiores (T-P8-03), modal/pandeo de láminas (T-P8-08/09).


## 15. Segunda pasada REA + original (2026-11-12, cierre de lo medible de T-P8-02) [V salvo marca]

Vías: REA sobre `calcmallad3d.dll`, `modelo3d.dll`, `geom2d.dll`, `calcmef.dll` y `cyopensees.dll` y, solo para contar la malla real y probar el factor 0, el original (obra `m6_lam.txt`: barras borradas, lámina 4 x 4 m, aristas «Articulación», lámina por defecto h = 100 mm y C25/30; capturas `docs/screenshots/mesh_v.png`, `fz_view.png`, `fz_dlg.png`, `fz_calc.png`).

### 15.1 Mallado: de dónde sale cada número

Cadena leída: `cm3dmalla_genera_mallado_de_elementos_modelo3d` (calcmallad3d) crea un «plano» por cada plano de láminas, inserta contornos, huecos, barras elásticas y puntos de carga y llama a `modelo3d_compatibiliza_conexiones_y_mejora_mallado_ex(modelo, num_max_iteraciones = 5·max(num_laminas, 100), progreso)` (modelo3d). La triangulación no está en calcmallad3d: la hace `geom2d.dll` (`triang3d_*`), una malla de Delaunay de nudos con refinado por bisección de aristas.

- **[V]** Lado máximo `s` (campo «Tamaño máximo de los lados de un triángulo»): se guarda por lámina/área; el plano toma el **máximo** de sus áreas (`FUN_18000bf80` de modelo3d: máximo de `area[2]`) y una arista compartida entre planos (charnela) toma el **mínimo** de los tamaños de los planos que concurren (`FUN_180002d00`). Límites: bucle de mejora `numIteraciones < 50000` (con `num_max_iteraciones = 5·max(n,100)` como tope de pasadas de compatibilización) y bucle de compatibilidad `< 1000`.
- **[V]** Puntos intermedios de una arista de contorno de longitud `L` (`FUN_180067290`, usada por `triang3d_CreaPuntosIntermediosEnAristasConId`): `0` si `L <= 1,01·s`; si no `floor((L − 0,001·s)/s)` (4 m con s = 1: 3 puntos intermedios; 1,5 m: 1 punto). Las barras elásticas y de pandeo usan otra función (`FUN_180012740`: `n = max(2, ceil(L/Lmax))`, tramo mínimo 0,05 m); no confundir.
- **[V]** `triang3d_mejora_aspecto_mallado` = tres pasadas: `FUN_18005b960`, `FUN_18005b850` (no leídas: preparación y volteo de aristas tipo Delaunay) y `FUN_18004b500` (refinado). El refinado itera hasta que no se marca ningún punto nuevo o hasta que la razón «puntos nuevos / vértices» deja de mejorar (historial de 10 valores, `FUN_18004dce0` y `FUN_18006afe0`).
- **[V]** Regla de bisección de una **arista interior** `p1p2` (triángulos adyacentes con vértices opuestos `p3`, `p4`), `FUN_18005c3e0` (1 = partir por el punto medio, redondeado a la rejilla discreta del plano), para un tamaño `s`:

```
L = |p1p2|
if |dx| > 1.01 s or |dy| > 1.01 s or L > 1.01 s: split          # arista más larga que el tamaño
elif number_of_divisions(L) != 0: split                          # FUN_180067290 (arista con curva)
else:                                                            # arista corta: criterio de forma
    k = 2.0 ; m = 0.35 s                                         # FUN_18005e5a0 = 2.0 ; FUN_1800703d0(s) = 0.35 s
    if k*1.01*m < L:                                             # L > 0.707 s
        d6=|p1p3| d7=|p1p4| d8=|p2p3| d9=|p2p4| ; f = 2.02
        split if (f*d6 < L and f*d7 < L) or (f*d8 < L and f*d9 < L)
             or (f*d8 < L and f*d8 < d6 and f*m < d6) or (f*d6 < L and f*d6 < d8 and f*m < d8)
             or (f*d9 < L and f*d9 < d7 and f*m < d7) or (f*d7 < L and f*d7 < d9 and f*m < d9)
```

  Solo se parte si el punto medio no cae sobre una línea de borde (`indLinborde == ULONG_MAX`) y los cuatro triángulos resultantes pasan `FUN_18009e0e0` (validez/orientación, no leída). **Consecuencia para el clon**: con s = 1 m las hipotenusas de 1,41 m de una malla estructurada se parten (1,41 > 1,01), así que la malla del original **no** es la estructurada del clon (32 triángulos en la placa).
- **[V original, conteo propio]** Malla real de la placa 4 x 4 m con s = 1,00 m (visor «Deformada, escala 0» con «Discretización», `mesh_v.png`, nudos leídos a mano): **57 nudos (25 de contorno y 32 interiores) y 87 triángulos** (Euler `T = 2V − B − 2`), tramos de contorno de 0,6–0,8 m (5–6 nudos intermedios por lado, no 3), aristas de 0,33 a 1,7 m, mediana 0,67 m (±5 % en el recuento de nudos por lectura visual). Es la **misma malla** en todos los cálculos de una obra (los cálculos con distintos factores de rigidez dibujan los mismos nudos, `fz_view.png`). La cifra «40–50 triángulos» de §9.0 y «16 en el muro» de §13.2 eran estimaciones visuales erróneas.
- **[N] simulación** `docs/research/data/mesh_sim.py` (Delaunay del contorno y bisección de aristas interiores con la regla de arriba, sin `FUN_18005b960/b850`, sin el filtro `FUN_18009e0e0` y sin partir el contorno): 49 nudos y 80 triángulos en la placa (29 y 44 en el muro 4 x 2): orden de magnitud correcto (80 frente a 87) pero **no validada nudo a nudo** (el contorno del original tiene más nudos). `mesh_count.py` es el conteo de la captura; su comprobación de aristas por color no es fiable en el contorno (líneas violeta poco saturadas) y el 87 sale de la fórmula de Euler con los nudos leídos. **Para un desarrollador**: Delaunay restringido + la bisección anterior reproduce la densidad (≈ 5,4 triángulos/m² a s = 1 m); la identidad exacta de nudos exige leer `FUN_18005b960`, `FUN_18005b850`, `FUN_18009e0e0`, `FUN_18004a260` y `FUN_180062fd0/68b70` (abierto, TK-109).

### 15.2 «Fuerzas nodales» de bandas (calcmef `cefbandint`) [V algoritmo; sin cifras reproducibles]

Leídas: `FUN_18001abf0` (suma por nudos), `FUN_18001b980` y `FUN_18001db70` (selección de nudos a cada lado del corte), `FUN_18001f290` (por triángulo cruzado), `FUN_180022e70` (normalización) y `FUN_18001cd40` (aporte de un nudo):

```
cut = segment of the band centre line (the band widths left/right only bound the segment, they are not a strip)
for each lamina: triangles = those crossed by the cut segment                (spatial query FUN_180022410)
  for each crossed triangle (6 nodes: vertices + mid-sides):
     if the cut touches the triangle only at a point/vertex ('interseccion_en_vertice'): factor = 1 (provisional),
         the centroid decides the side
     else: factor = length(cut inside the triangle) / length(infinite cut line inside the triangle), clamped to [0,1];
         each node goes to the NEGATIVE or the POSITIVE list by its side of the cut line;
         a node ON the line (|signed distance| <= 1e-6) goes to BOTH lists
  normalisation (FUN_180022e70): triangles flagged 'interseccion_en_vertice' get
         factor = (sum of the factors of the NON-flagged triangles) / (number of NON-flagged triangles)
resultant on one side (sign = -1 when the side flag is 1, else +1):
  for each (triangle, node, factor w):
     F, M = element nodal force (Fx,Fy,Fz) and nodal moment (Mx,My,Mz) of that node in the lamina frame
            (element callback func[9]: K_e u_e minus the equivalent nodal loads of the element)
     r    = node position minus the reference point of the band section (2D, z = 0)
     (Fx,Fy,Fz) += sign * w * F                  # rotated to the band frame with the lamina->band rotation
     (Mx,My,Mz) += sign * w * (M + r x F)        # idem
```

Los nudos son los de los triángulos que cruza el **segmento** de la banda (no una franja de 15 cm): por eso con malla de 1 m el resultado es «mucho peor» que con tensiones (§13.1): es el equilibrio de las fuerzas nodales de los triángulos cruzados, ponderadas por la fracción de cada triángulo que atraviesa el corte. Las cifras −841,96 y −752,20 no se pueden reproducir sin la malla exacta (la fracción `w` depende de cómo corta el segmento cada triángulo). **Falta en el clon**: sustituir la regla actual («fuerzas nodales de los triángulos con centroide antes del corte») por la anterior (triángulo cruzado, factor de longitud, nudo sobre la línea en ambas listas, normalización por vértice, `M + r × F`).

### 15.3 Alisado de esfuerzos y Vz de banda [N: no localizado]

Ni calcmef ni calcmallad3d contienen cadenas o procedimientos «alisado/promedio/smooth» (búsqueda literal y regex vacía): `smoothedStresses` solo entra como bandera de `cefbandint_integra_esfs_todas_hipos_en_punto_banda`. El alisado está, pues, en la extracción de resultados por nudo (`cyopensees.dll`, estructuras `cyosresulbas_resultados_triangulo6n/3n`) o en el visor; no se leyó. La diferencia de Vz de banda del clon (+9,4 / −14,9 / +10,7 %) es del orden del ruido de cortante con malla basta: la malla real tiene 87 triángulos y el clon 32, de modo que **el primer paso es adoptar la regla de §15.1** y después volver a comparar.

### 15.4 Factor de rigidez 0: no hay mensaje, se regulariza [V original]

- Con **Mxx = Mxy = Myy = Vxz = Vyz = 0** (Fxx = Fxy = Fyy = 1) en la placa articulada (ninguna rigidez fuera del plano) el original **calcula sin ningún aviso** y da un desplazamiento total máximo de **9,5967e8 mm** (`fz_view.png`; con rigidez normal la misma placa da 0,96 mm): razón ≈ 1,0e9, es decir la rigidez nula se sustituye por una **rigidez mínima ≈ 1e-9 veces la nominal** (valor medido, no leído del código). Por tanto «factor 0» **nunca produce el aviso de mecanismo** (confirma §13.3: Fxx = Fxy = Fyy = 0 y Mxx = 0 calculan). El clon debe sustituir `F = 0` por `F = 1e-9` en `D' = S·D·S`, `S = diag(sqrt(F))`, en lugar de bloquear gdl sin rigidez.
- El aviso de mecanismo aparece cuando **falta apoyo**: lámina 4 x 4 m (barras borradas) sin vinculación exterior -> diálogo «Aviso»: «Esta estructura es un mecanismo. Revise los datos introducidos. Desplazamiento según el eje Z. El error se encuentra en el punto de coordenadas: (x = 4,000000, y = 4,000000, z = 0,000000). Alguna de las barras de la estructura no está vinculada correctamente en sus extremos, es decir, no es estable.», con la traza «gstif(27,27)=609…» en la barra de progreso (92 %) y el informe final «Esta estructura es un mecanismo. Revise los datos introducidos.». El gdl nombrado es **Z** (traslación) del nudo (4, 4, 0) (el tercer vértice de la obra) y no un giro; el texto habla de «barras» aunque solo hay láminas.
- Con las barras de contorno sin describir el cálculo se detiene antes: error «Existen barras sin describir.» (no es mecanismo).
- Voladizo con Fyy = 0 o borde empotrado: no se probó (innecesario: la regularización 1e-9 impide el aviso por factor).
- Lámina nueva por defecto: espesor 100 mm, material C25/30 (tooltip: «Espesor: 100,00 mm; Material: C25/30», factores en orden Fxx, Fyy, Fxy, Mxx, Myy, Mxy, Vxz, Vyz, y «Análisis modal: Se considera en el análisis», «Análisis de pandeo: No se considera»).

### 15.5 Mxx = 0: razón de w ×1,756 (original) frente a ×1,612 (clon) [N]

No se investigó por separado: con rigidez de flexión mínima en una dirección la flecha depende de la topología de la malla (87 triángulos del original frente a 32 del clon; el clon sustituye además el factor 0 por «sin rigidez y gdl bloqueados»). Adoptar `F_min = 1e-9` (§15.4) y la malla de §15.1 y repetir; hasta entonces tolerancia ±10 % en este caso.

## 16. Alisado de esfuerzos de lámina localizado (3.ª pasada REA, 2026-11-12) [V salvo marca]

Vía: solo REA (sin lanzar el original). Binarios: `cyopensees.dll` (alisado; los símbolos `cyosresul*` y los mensajes `error_interno` con nombres de variables dan los nombres reales) y `geom2d.dll` (malla). El alisado **no** está en calcmef, calcmallad3d, modelo3d ni en el visor: está en `cyopensees.dll`, fichero fuente `cyosresul.c`, y es un **promedio simple por nudo**.

### 16.1 Origen de los esfuerzos por triángulo [V]

- Los triángulos de lámina son elementos OpenSees de 6 nudos (`6node_independent_triangle_elems`, grupo Tcl `__independent_6_node_tgAll`, prefijo `i6ntg`; existen además `triang3n` y `cuad` con `ShellDKGT`/`ShellDKGQ`, `ShellNLDKGT`, `ShellCype`). Los esfuerzos se piden al motor con un registrador Tcl (cadena UTF-16 de `cyopensees.dll`, función `FUN_1800d5510`): `recorder Element ... -time -ele {*}$__independent_6_node_tgAll stressesNodes;` (esfuerzos **ya dados en los nudos del elemento**, no en puntos de Gauss) y, si hay bandas/fuerzas nodales, `... globalForce;` (fuerzas nodales del elemento, §15.2). La extrapolación Gauss -> nudos, si existe, ocurre **dentro del motor OpenSees** (`stressesNodes` no es un comando estándar de OpenSees: extensión de CYPE).
- **Límite de la vía estática**: el motor OpenSees de CYPE está en `cyopenseesbas.dll` (52 MB: sección `.data` de 52 136 448 bytes con entropía 8,00 y `.text` de solo 10 752 bytes, es decir empaquetado/cifrado; ninguna cadena `Shell`, `stress` o `Element` ni funciones recuperables, tampoco en ASCII ni UTF-16 en ninguna otra DLL/EXE de la carpeta). No se puede leer estáticamente cómo calcula `stressesNodes` (extrapolación, ley de la lámina, ejes locales). Solo sería accesible volcando la memoria del proceso en ejecución; no se hizo.
- Por vértice de triángulo y por caso de carga/escalón se guarda un registro `cyosresulbas_esf_punto_lamina_t` de **8 doubles** (`[0..7]`, esfuerzos de lámina), más 3 bloques de «tensiones en cara» (`tensiones_cara`, 5 doubles cada uno) y 3 bloques de «deformaciones» (5 doubles + valor equivalente). El significado y orden de los 8 doubles (lo coherente con §13 sería Nxx, Nyy, Nxy, Mxx, Myy, Mxy, Vxz, Vyz) **no está verificado** en el código.

### 16.2 Alisado por nudo = media aritmética sin pesos [V]

`FUN_180042560` (triángulos de 6 nudos; `FUN_18004c410` es la variante análoga con tipo cuadrilátero) y subrutinas `FUN_1800473a0`, `FUN_180047140`, `FUN_18003f1d0`, `FUN_180040f20`, `FUN_180046d80`, `FUN_180045500`, `FUN_180041f60`, `FUN_180040190`, `FUN_180040330`:

```
smooth_triangles6(triangles, results):                       # FUN_180042560
  groups = distinct triangle.group_id (offset +0x44), ignoring -1      # FUN_180048900
  for each group g:                       # g = lamina/plane id: NO smoothing across groups
     nodes = ordered set keyed by model node id
     for each triangle t with t.group_id == g:                          # FUN_180040f20
        for v in (vertex1, vertex2, vertex3):   # only the 3 CORNER nodes (offsets +0, +8, +0x10); mid-side nodes are not smoothed
            acc = nodes[node_id(v)]  (created at zero on first use)     # FUN_18003f1d0
            for every load case c, step s:
                acc.sum[c][s][0..7]            += unsmoothed[v][c][s][0..7]
                acc.sum[c][s].face_stresses[3] += ... ; acc.sum[c][s].strains[3] += ...
            acc.num_uses += 1 ; acc.triangles.add(t)
     for each node acc:                                                 # FUN_180046d80 / FUN_180045500
        if acc.num_uses > 1: every accumulated value /= acc.num_uses    # plain arithmetic mean
        # the derived equivalent value is RECOMPUTED from the averaged components, not averaged:
        #   FUN_1800458c0 (5 components, with flag) or FUN_180045aa0 (3 components)
        for each triangle t in acc.triangles:                           # FUN_180047140 -> FUN_180041f60
            copy acc into t.smoothed[vertex slot whose model node id == this node]
```

- **Sin pesos**: ni por área del triángulo, ni por ángulo, ni por distancia: `valor_alisado(nudo) = (1/n) · Σ valor_sin_alisar(triángulo i, nudo)`, con n = número de triángulos del **mismo grupo** (lámina/plano) que comparten el nudo. Con n = 1 no se divide.
- El resultado «sin alisar» sigue guardado (`solucion_sin_alisar_nudo1/2/3_por_ccarga_por_escalon`, bloques `+0x18/+0x20/+0x28` del resultado del triángulo) y el alisado en `+0x00/+0x08/+0x10`. De ahí las dos variantes «internos alisados / sin alisar» de la banda (§13.1).
- Un nudo compartido por láminas de **grupos distintos** (aristas con charnela, muro-forjado) se promedia por separado en cada grupo: no hay continuidad de esfuerzos alisados entre planos.
- Qué es exactamente `group_id` (+0x44 de `cyosresulbas_datos_triangulo6n_t`: lámina o plano de láminas) no se ha fijado; el valor `-1` queda fuera.

### 16.3 Puntos de lámina (isovalores y banda) [V estructura; N integración de la banda]

- Un «punto de lámina» (`cyosresulbas_datos_punto_lamina_t` / `resultados_punto_lamina_t`) **es un nudo de la malla**, no un punto arbitrario: `FUN_18003fa60` toma para cada vértice de cada triángulo su índice de punto de lámina (`-1` = no registrado), `FUN_18003fb50` **suma** los 8 esfuerzos sin alisar de ese vértice y cuenta usos, y `FUN_180052b90` divide cada suma por `num_usos_puntos_lamina` (misma media simple que 16.2). No hay interpolación baricéntrica ni funciones de forma en cyopensees: **si se interpola en un punto cualquiera es el consumidor** (isovalores del visor, calcmef) quien lo hace; el módulo solo entrega valores en nudos.
- En la ruta de barras la validación equivalente (`FUN_18005f1c0`) exige que el nudo de malla pertenezca al segmento con `ebamath_punto_pertenece_a_segmento_3D_con_precision(..., CYOSTIPOS_PRECISION_PUNTO_LAMINA)` y distancia mínima entre nudos iguales `1e-6`; es coherente con que los puntos de una banda sean los nudos de malla sobre el eje de la banda («5 puntos» de §13.1 con nudos cada 0,6–0,8 m, §15.1). Esa asociación banda-nudos es una **inferencia** (no se leyó el registro de puntos de banda de `cefbandint`).
- Valor de banda con esfuerzos alisados (inferido): nudos de malla sobre el eje, cada uno con la media simple de 16.2, y luego la regla de integración de calcmef (`cefbandint_integra_esfs_todas_hipos_en_punto_banda`, bandera `smoothedStresses`), no leída.

### 16.4 Malla: `FUN_18005b960` y `FUN_18005b850` (geom2d.dll) [V estructura; N criterios]

- `triang3d_mejora_aspecto_mallado` = tres pasadas; las dos primeras son **barridos de volteo de aristas** repetidos hasta que no cambia nada:
  - `FUN_18005b960`: `do { changed = FUN_18005a830(malla, triangulo_raiz, ...); FUN_180082080(...); } while (changed)`; exige `malla->xmax > xmin` y `ymax > ymin`; con `param_4 != 0` llama a `triang3d_compruebaMalla` (depuración).
  - `FUN_18005a830` (recorrido recursivo por vecinos `tr1/tr2/tr3`, marca `visitado`): para cada una de las 3 aristas, si el id de región del triángulo está en la lista de regiones activas y la arista no es de borde (`borde1b/2b/3b == FALSO`), consulta `FUN_18005d9f0` -> `FUN_18005dc30` (¿arista restringida?, compartida entre regiones/contorno). Si **no** es restringida: `FUN_180062b10` = `FUN_18008c3b0(..., 1)` (test de volteo) y, si procede, `FUN_180085e50` (voltea la arista). Si **es** restringida: `FUN_1800598b0` (tratamiento de arista restringida; si devuelve 1 aborta y reinicia el barrido).
  - `FUN_18005b850`: mismo bucle con `FUN_18005aff0` (segunda pasada). **Sin leer**: `FUN_18008c3b0` (criterio de volteo), `FUN_18005dc30`, `FUN_1800598b0`, `FUN_18005aff0`, `FUN_180085e50`, `FUN_18009e0e0`, `FUN_18004a260`, `FUN_180062fd0/68b70`.
- Conclusión: el mallado es Delaunay restringido + volteo iterativo + refinado por bisección (§15.1). Que el volteo sea el criterio de círculo circunscrito es lo habitual pero **no está verificado**. **No se fijaron las coordenadas exactas de los nudos de la placa 4 x 4 m con s = 1 m.**

### 16.5 Consecuencias para el clon (T-P8-02)

1. Calcular esfuerzos por triángulo en los nudos del elemento; no hay extrapolación del motor del original que se pueda replicar a nivel de código (cifrada).
2. Alisado = media aritmética de los valores sin alisar de los triángulos de la misma lámina/plano que comparten el nudo (solo vértices esquina), sin pesos; guardar también el valor sin alisar.
3. Puntos de banda = nudos de malla sobre el segmento de la banda; valor alisado = ese promedio. Para un punto arbitrario, interpolación lineal entre los nudos del triángulo que lo contiene es una decisión de diseño del clon, no verificada.
4. Selección de triángulo para el valor «sin alisar» de un nudo compartido: no leída (calcmef).

### 16.6 Abierto

- Cálculo de `stressesNodes` en el motor cifrado y orden/significado de los 8 doubles (verificar en el original comparando una obra de prueba).
- Regla de integración de la banda entre puntos y selección de triángulo en «sin alisar» (calcmef).
- Criterio de volteo `FUN_18008c3b0` y coordenadas exactas de nudos de la placa 4 x 4 m.

## 17. Implementación de §15 en el clon (cierre de T-P8-02, 2026-11-12)

Código: `src/core/fem/shell_mesh.cpp` (reescrito), `shell_element.{h,cpp}` (factor mínimo), `shell_bands.cpp` (fuerzas nodales), `linear_solver.cpp` (aviso de giros excesivos solo en nudos de barras). Tests: `tests/fem_shell_tests.cpp` (valores impresos como «[value]»). El alisado de esfuerzos (§16) no se tocó en este paso (recuperación por parches de §14); **sustituido en §18** por la media por nudo.

### 17.1 Malla (regla de §15.1)

- **[V]** Tamaño de un plano = máximo de sus láminas; arista entre planos distintos = mínimo. **[N]** cómo se forma un «plano» (el clon agrupa todas las láminas coplanarias de la obra, aunque estén separadas) y que el refinado interior de cada lámina use el tamaño del plano y no el suyo: no verificado (consecuencia: una losa con 0,25 m en la misma cota que otra con 1 m se malla con 1 m). [V] puntos intermedios del contorno `0` si `L <= 1,01 s`, si no `floor((L − 0,001 s)/s)`; Delaunay de los puntos del plano (Bowyer-Watson incremental con adyacencias) y pasadas de bisección por el punto medio de las aristas interiores marcadas por la regla de `FUN_18005c3e0` (longitud > 1,01 s, o criterio de forma k = 2,0, m = 0,35 s) hasta que no se marca ninguna. Un punto medio sobre una línea de contorno no se inserta: las cadenas de contorno son fijas, de modo que láminas vecinas y barras sobre aristas comparten los nudos sin iteración de compatibilización.
- **[N]** Empates de puntos cocirculares: por orden de inserción (determinista); no se reproducen `FUN_18005b960/b850` (volteo), `FUN_18009e0e0` (filtro), el redondeo a la rejilla del plano ni la parada por historial de la razón puntos nuevos/vértices. Contorno no convexo: si se pierde un tramo de contorno se informa («no se pudo mallar el contorno»).
- Resultado: placa 4 x 4 m, s = 1 m: **49 vértices y 80 triángulos** (original 57 / 87, −14 % / −8 %); muro 4 x 2: 29 / 44; placa con s = 0,5: 196 / 358. Igual que `mesh_sim.py`. Por Euler la diferencia está **toda en el contorno**: interior 33 (original 32), contorno 16 (original 25). Se probó aplicar el criterio de forma también a las aristas de contorno (con un solo vértice opuesto, o con p4 = p3): no parte ninguna; la regla que da 5–6 nudos intermedios por lado en el original no está leída (abierto, TK-109).

### 17.2 Factor de rigidez 0

`F = 0` se sustituye por `F_min = 1e-9` en `D' = S·D·S` (membrana, flexión, cortante y la penalización de *drilling*), y los esfuerzos se recuperan con los factores nominales (un factor 0 da exactamente 0, como lista el original). Placa con Mxx = Mxy = Myy = Vxz = Vyz = 0: calcula sin mensaje, w ×1,0000e9 respecto a la placa normal (original ×1e9: 0,96 -> 9,5967e8 mm) y Mx = My = Mxy = Qx = Qy = 0. Para que no salte el aviso «desplazamientos demasiado grandes» (TK-105, giro > π/2) en ese caso, el control de giros se limita a los nudos de barras **[N]** (en el original la obra sin barras no avisa con 9,6e8 mm).

### 17.3 Fuerzas nodales de bandas (§15.2)

Implementado el algoritmo leído: corte = segmento transversal en s (de −izquierda a +derecha), triángulos que cruza, factor `w` = longitud del corte dentro / longitud de la recta dentro (recortado a [0, 1]), triángulos tocados en un punto con la media de los demás, nudo sobre la línea en ambas listas, `M + r × F` respecto al punto de la banda. **[N] lado y signo, evidencia débil**: los nudos «después» del corte con signo +1. Con malla fina y banda de 1 m todas las variantes probadas («antes» con −1, «después» con +1, la semidiferencia) coinciden con las tensiones (+0,7 / +0,3 / +0,5 %), así que esa prueba no discrimina; se eligió «después» por ser la más próxima al original con la malla de 1 m, cuyas cifras no se pueden reproducir. Además hay una discrepancia cualitativa: el original **subestima** respecto a las tensiones (−752 frente a ~−1149 en el centro) y el clon **sobrestima** (×1,8 respecto a sus tensiones). Banda y = 2 m, 15 cm, malla 1 m:

| x (m) | Clon My / Vz / Mt | Original My / Vz / Mt |
|---|---|---|
| 0,994 | −1757,5 / 1540,7 / −0,8 | −841,96 / 833,01 / −140,23 |
| 1,995 | −2107,3 / −36,5 / −1,9 | −752,20 / −70,92 / 293,94 |

**No se reproducen** (×2,1 y ×2,8 en My): con una banda de 15 cm y triángulos de ~1 m el resultado depende de cómo corta el segmento cada triángulo de la malla exacta (y el Mt del original, ±300, es ruido de esa malla). Con el lado «antes» (signo −1) salen −2631,5 / −4638,2 y Vz 5314 en el centro (peor). Banda de 1 m en malla de 0,25 m: −6653,9 frente a −6635,2 con tensiones (+0,28 %).

### 17.4 Revalidación con la nueva malla (malla 1 m salvo indicación)

| Caso | Clon §17 | (clon §14) | Referencia | Desvío | Tolerancia del test |
|---|---|---|---|---|---|
| w máx placa h 0,2 | 502,79 mm | 507,28 | original 501,16 | +0,33 % | 1 % (antes 3 %) |
| Mx centro (= máx) | 7973,5 | 8074,5 | original 7887,5 | +1,09 % | 2 % (antes 3 %) |
| My máx | 7990,8 | — | original 7881,1 | +1,39 % | 2 % |
| Mx centro frente a Navier | 7973,5 | 8074,5 | 7658,1 | **+4,12 %** | 4,5 %: **fuera del ±3,5 % pedido** (el propio original da +3,0 % máx / +2,6 % centro; el apoyo blando suma ~3 %) |
| Malla 0,5: w / Mx / My frente a Navier | 500,07 / 7918,3 / 7924,1 | 499,07 / 7921,1 | 472,95 / 7658,1 | +5,73 / +3,40 / +3,47 % | 6 / 3,5 / 3,5 % |
| Malla 0,25: w / Mx centro | 499,98 / 7914,9 | 499,87 / 7916,9 | original 498,09 / 7857,5 | +0,38 / +0,73 % | 1 / 2 % |
| h 0,4: w / Mx máx | 67,92 / 8244,3 | 68,51 / 8317,8 | 67,19 / 8111 | +1,09 / +1,64 % | 2 / 2,5 % |
| Peso propio: Dz mín | −0,2468 | −0,249 | −0,24 | +2,8 % | 5 % (dato de 2 decimales) |
| Mxx 0,5: w / Mx / My | 1,204 / 0,644 / 1,127 | 1,199 / 0,647 / 1,125 | 1,206 / 0,652 / 1,112 | −0,2 / −1,2 / +1,3 % | 3 / 2 / 3 % |
| **Mxx 0: razón w** | **1,641** | 1,612 | 1,756 | **−6,5 %** | 8 % (antes 10 %): sigue fuera de un 3 % |
| Mxy 0,5 / Vxz = Vyz 0,1: razón w | 1,191 / 1,201 | 1,199 / 1,205 | 1,203 / 1,188 | −1,0 / +1,1 % | 3 / 4 % |
| Muro: Dz punta / Dx | −19,869 / 5,343 | −19,75 / 5,345 | −19,90 / 5,34 | −0,16 / +0,05 % | 2 / 3 % |
| Muro Fxx 0,5 (Dz, Dx) / Fxy 0,5 (Dz) | 1,799 / 1,992 / 1,202 | | 1,797 / 1,994 / 1,203 | +0,1 / −0,1 / −0,05 % | 2 % |
| Balasto K = 1e6: w máx | 11,11 | 11,08 | 10,96 | +1,38 % | 2 % (antes 5 %) |
| Banda alisada My en 0,994 / 1,487 / 1,995 / 2,991 | −960,7 / −1092,6 / −1189,4 / −992,1 | −942,8 / −1074,6 / −1205,2 / −949,6 | −916,1 / −1090,6 / −1160,5 / −943,4 | +4,9 / +0,2 / +2,5 / +5,2 % | 6 % (antes 4 %: **empeora** en los extremos por la asimetría de la malla de Delaunay) |
| **Banda alisada Vz** en 0,994 / 1,487 / 2,991 | 797,1 / 400,9 / −778,6 | 935,0 / 339,7 / −918,2 | 855,0 / 399,3 / −829,5 | −6,8 / +0,4 / −6,1 % | **8 % (antes 16 %)** |
| Banda centro frente a Navier | −1190,0 | −1205,2 | −1148,7 | +3,6 % | 6 % |
| Banda sin alisar My en 0,994 | −1061,1 | −977,5 | −927,8 | **+14,4 %** | 16 % (antes 6 %): el valor de un triángulo depende de la malla exacta |

Abierto (TK-109): regla de nudos de contorno del original (16 frente a 25), identidad nudo a nudo, cifras de fuerzas nodales, Mxx = 0 (−6,5 %), banda sin alisar; alisado del original (§16, otro agente).

## 18. Cuarta pasada REA (geom2d.dll) y alisado de §16 en el clon (2026-11-12) [V salvo marca]

Vía: solo REA sobre `geom2d.dll` (sin lanzar el original). Funciones leídas: `triang3d_mejora_aspecto_mallado`, `FUN_18005b960`, `FUN_18005a830`, `FUN_18005b850`, `FUN_18005aff0`, `FUN_1800598b0`, `FUN_1800599d0`, `FUN_18005a430`, `FUN_1800703f0`/`FUN_180070570`, `FUN_180067290`, `FUN_180057050`, `FUN_18005d9f0`/`FUN_18005dc30`/`FUN_180057710`, `FUN_18005f900`/`FUN_18005f6e0`, `FUN_180062b10`, `FUN_18008c3b0`/`FUN_18008c590`/`FUN_18008bb20`, `FUN_180096be0`/`FUN_1800949c0`, `FUN_18004b500`, `FUN_18004ada0`, `FUN_18004a960`, `FUN_180062fd0`/`FUN_180062bc0`, `FUN_180068b70`, `FUN_18004c870`, `FUN_180068a10`, `FUN_180068d30`, `FUN_180059780`.

### 18.1 Malla: volteo y puntos de contorno

- **Orden** [V]: `mejora_aspecto` = pasada 1 (`FUN_18005b960`: barrido de volteos de aristas no restringidas; en las restringidas, `FUN_1800598b0` = regla del suelo `floor((L − 0,001 s')/s')` con `s' = min(tamaño de la región, tamaño de la región vecina)`, partición recursiva por mitades `FUN_180057050`), pasada 2 (`FUN_18005b850`: volteos + **partición de aristas de contorno**), refinado interior (`FUN_18004b500`). Cada barrido se repite hasta que no cambia nada; una inserción reinicia el barrido.
- **Arista restringida** [V, `FUN_18005dc30`]: región distinta a cada lado, o índice de línea de borde ≠ −1, o datos de arista, o datos de triángulo distintos.
- **Criterio de volteo** [V, `FUN_18008bb20` con la opción de forma que usan los barridos]: el cuadrilátero debe seguir convexo (áreas nuevas ≥ 1e-20; filtro `FUN_18009e0e0` no leído) y se voltea si `max(f_antes) ≥ 1,00001·max(f_después)`, con el **factor de forma** `f(p1,p2,p3) = Σ_vértices |p_i − ápice del equilátero construido sobre el lado opuesto|² / área` (`FUN_180096be0`, `FUN_1800949c0`; 0 = equilátero). **No es el criterio de Delaunay** (círculo circunscrito).
- **Partición de una arista de contorno p1p2 con vértice opuesto p3** [V, `FUN_1800599d0`], solo si p3 es un punto de borde (`FUN_18005f900`: alguna arista restringida en su abanico). Con `L = |p1p2|`, `t` = abscisa de la proyección de p3 desde p1, `d` = distancia de p3 a la recta, `k = 2,0`, `h = max(0,35 s, k·d)`:
  - proyección en un extremo (`|t| < 0,001 d` o `|L − t| < 0,001 d`): con `l = min(s, h)`, si `L > 1,01 l` se inserta a distancia `l` de ese extremo (o en el punto medio si `L < 2 l`);
  - si `L < h` o `t < 0,75 h` o `L − t < 0,75 h`: se inserta la proyección de p3 solo si `0,3 L < t < 0,7 L` y (`L > 1,01 s`, o los vértices opuestos de los dos triángulos vecinos son de borde, distan de la recta `≤ 1,01 d` y se proyectan en `(0,15 L, 0,85 L)`);
  - en otro caso (arista larga frente a h y p3 lejos de los extremos) se inserta la proyección de p3.
- **Triángulo de esquina con dos aristas de contorno** [V, `FUN_18005a430`]: se parte por la mitad la que supera `max(0,35 s, 2 × la otra)`.
- **Refinado** [V]: `FUN_18004b500` = barrido de volteos (`FUN_180062fd0`), recolocación de los puntos interiores libres (`FUN_180068b70` -> `FUN_180068a10`: hasta 5 pasadas de `troptimo_coordenadas_optimas` con radio 5 × incremento de la rejilla; solo mueve el punto si el desplazamiento supera el incremento) y bucle de bisección (`FUN_18004ada0` -> `FUN_18004a960` = regla de §15.1 + cuatro comprobaciones `FUN_18009e0e0` + `FUN_18004a260`, ambas sin leer), con recolocación tras cada tanda. **En el refinado no hay partición de contorno.**
- **Clon** (`shell_mesh.cpp`): implementados el criterio de volteo (barridos y volteos locales tras cada inserción), la pasada 2 completa (con propagación de los puntos nuevos a las láminas que comparten la arista) y el refinado con volteos. **[N]**: rutina de inserción `FUN_180063ec0` (el clon parte el triángulo o la arista y voltea localmente), orden de recorrido (el original recorre vecinos desde un triángulo raíz; el clon, por índice), filtro `FUN_18009e0e0`, recolocación `troptimo`, redondeo a la rejilla y parada por historial.
- **Resultado: la placa 4 x 4 m con s = 1 m sigue en 49 vértices / 80 triángulos** (original 57 / 87). Aplicadas una vez sobre la triangulación de los 16 puntos de contorno, las reglas leídas no parten ninguna arista (lados de 1 m; los vértices opuestos de borde distan ≥ 1 m, `h ≥ 2 > L`, y ninguno se proyecta en el tercio central). `mesh_v.png` muestra 5–6 puntos intermedios por lado **con separación irregular**. **Las reglas leídas no lo explican**: llamar otra vez a `mejora_aspecto` sobre la malla refinada tampoco añadiría puntos, porque `FUN_1800599d0` exige que el vértice opuesto sea de borde y junto al contorno casi siempre es interior (y `FUN_18005a430` no parte lados de 1 m). Lecturas que lo decidirían (TK-109): qué tamaño ve el contorno (5–6 puntos por lado equivalen a un `s'` de 0,57–0,8 en la regla del suelo, no 1,0: leer qué lista de tamaños construyen calcmallad3d/modelo3d, si la región exterior a la placa está en ella y si el contorno entra en `mejora` solo con sus esquinas); si `modelo3d_compatibiliza_conexiones_y_mejora_mallado_ex` llama a `triang3d_CreaPuntosIntermediosEnAristasConId` o a `mejora` dentro de su bucle; y la separación irregular, para la que hay un candidato en lo leído: los puntos de borde pueden entrar en la lista de recolocación por `FUN_180053790` -> `FUN_180053000` dentro de `FUN_18004c870` (explicaría la irregularidad, no el número). **Objetivo 57/87 no alcanzado.** La malla de 0,5 m pasa de 196/358 a 197/360 por el criterio de volteo.

### 18.2 Alisado de §16 en el clon

- Implementado [V]: media aritmética sin pesos, por lámina, en los **vértices** de los triángulos; los nudos intermedios no se alisan ni se listan (`smoothedShellForces` devuelve solo vértices; hoy solo lo usan los tests). Sin alisar = valor del triángulo en sus vértices.
- Valor por triángulo y vértice **[N]**: el original lo recibe del motor cifrado (`stressesNodes`). Se probaron tres candidatos sobre la misma malla: campo del elemento en el nudo (Mx centro +5,9 % frente al original, Vz de banda +32 a +50 %), valor del centroide (Mx −7,5 %, My de banda hasta −12 %) y **valor del punto de Gauss más próximo al vértice** ((1/6, 1/6) -> vértice 0, (2/3, 1/6) -> 1, (1/6, 2/3) -> 2; práctica habitual en códigos de EF). Se adopta el último porque es el único que encaja con **todas** las cifras medidas a la vez (tabla); sigue siendo una hipótesis no leída. Nota: los momentos del elemento son lineales en el triángulo, así que «sin alisar» da lo mismo interpolando con 3 o con 6 nudos; el cambio de −1061 a −936 se debe al valor por vértice, no a la interpolación.
- Punto arbitrario **[N]**: interpolación lineal entre los tres vértices del triángulo que lo contiene (en un nudo devuelve exactamente su valor, coherente con «puntos de banda = nudos de malla» de §16.3). Sin alisar en una arista compartida: el primer triángulo encontrado (no leído).

| Caso (malla 1 m salvo indicación) | Clon §18 | Clon §17 (parches) | Original | Desvío §18 |
|---|---|---|---|---|
| Mx centro (= máx) | 7827,8 | 7973,5 | 7887,5 | −0,76 % |
| My máx | 7849,4 | 7990,8 | 7881,1 | −0,40 % |
| Mx centro frente a Navier 7658,1 | 7827,8 | 7973,5 | (original +3,0 %) | **+2,22 %** (dentro del ±3,5 %) |
| Malla 0,5: Mx / My frente a Navier | 7886,1 / 7891,5 | 7918,3 / 7924,1 | | +2,98 / +3,05 % |
| Malla 0,25: Mx centro | 7907,5 | 7914,9 | 7857,5 | +0,64 % |
| h 0,4: Mx máx | 8108,6 | 8244,3 | 8111 | −0,03 % |
| Mxx 0,5: razones w / Mx / My | 1,204 / 0,645 / 1,127 | 1,204 / 0,644 / 1,127 | 1,206 / 0,652 / 1,112 | −0,2 / −1,1 / +1,4 % |
| Banda alisada My en 0,994 / 1,487 / 1,995 / 2,991 | −969,6 / −1072,8 / −1163,3 / −937,5 | −960,7 / −1092,6 / −1189,4 / −992,1 | −916,1 / −1090,6 / −1160,5 / −943,4 | +5,8 / −1,6 / +0,2 / −0,6 % |
| Banda alisada Vz en 0,994 / 1,487 / 2,991 | 749,0 / 402,8 / −832,7 | 797,1 / 400,9 / −778,6 | 855,0 / 399,3 / −829,5 | **−12,4** / +0,9 / +0,4 % |
| Banda centro frente a Navier −1148,7 | −1163,3 | −1190,0 | | +1,3 % |
| Banda sin alisar My en 0,994 | −935,7 | −1061,1 | −927,8 | +0,85 % |
| Fuerzas nodales My en 0,994 / 1,995 | −1757,5 / −2107,3 | igual | −841,96 / −752,20 | no reproducido (×2,1 / ×2,8) |
| Mxx = 0: razón w | 1,641 | 1,641 | 1,756 | −6,5 % |

Tolerancias de los tests estrechadas: Mx/My frente al original 2 % -> 1,5 %; Mx frente a Navier (malla 1 m) 4,5 % -> 3,5 %; malla 0,25 2 % -> 1 %; h 0,4 Mx 2,5 % -> 1 %; banda centro frente a Navier 6 % -> 2 %; sin alisar 16 % -> 2 %; Vz de banda 8 % -> 3,5 % en 1,487 y 2,991 (el clon da +0,9 / +0,4 %; margen por ser el campo más ruidoso), pero **13 % en 0,994** (el punto más próximo al apoyo, donde más difiere la malla: única tolerancia que se afloja). My de banda sigue en 6 % (+5,8 % en 0,994).

### 18.3 Abierto (TK-109)

- Los 25 nudos de contorno no salen de las reglas leídas (ver 18.1 para las lecturas pendientes); sin leer además: recolocación `troptimo`, inserción `FUN_180063ec0`, `FUN_18009e0e0` (filtro que también condiciona cada volteo en `FUN_18008bb20`), `FUN_18004a260` y `FUN_180085e50`. Sin ellos no hay malla nudo a nudo.
- La elección del valor por vértice y todas las tolerancias de placa y banda se han calibrado sobre la malla de 49/80: hay que revalidarlas cuando la malla coincida con la del original.
- Cifras del método de fuerzas nodales (dependen de la malla exacta) y Vz de banda en x = 0,994 (−12,4 %).
- Mxx = 0 (×1,641 frente a ×1,756): es una flecha, no cambia con el alisado; depende de la malla.
- Valor por vértice de `stressesNodes` (hipótesis del punto de Gauss más próximo, no leída: motor cifrado).
