# Solver lineal de pórtico 3D de CYPE 3D: investigación para T-P8-01

> Actualización 2026-11-12: con la licencia activada se midió el solver en el original; ver `solver_medidas_original.md` (Timoshenko confirmado, convenio de signos, empotramiento parcial, carga por longitud, temperatura). Las afirmaciones [N] de las secciones 4.1, 4.4, 6 y 7 sobre estos puntos quedan superadas por ese documento.

Fecha: 2026-11-12. Vía principal: REA (cyopensees.dll, cyopenseesbas.dll, ed3dlib.dll, calcmef.dll, calcmallad3d.dll). El original **no se lanzó**: Calcular y Dimensionar están bloqueados por la barrera de licencia «Permisos necesarios» (módulo M3D, ver `ui/t_re_03_cierre_huecos.md` §5) y no se elude. Por tanto no hay valores de referencia medidos en el original: los de la sección 8 son analíticos o de un solver independiente, y T-P8-05 (validación cruzada) queda bloqueada (ticket sugerido al final).

Marcas: **[V]** verificado en código o cadenas del original (REA); **[N]** no verificado (inferencia, hipótesis o hueco).

## 1. Hallazgo principal: el solver lineal es OpenSees embebido, no `calcmef`

- **[V]** El análisis estático lineal lo ejecuta `cyopensees.dll` (prefijo `cyos*`), llamado desde `ed3dlib.dll` (`cyosmodelo_append_nudo/barra/lamina/relacion_2_nodos/diafragma/arista_lamina` y `cyosmodelo_lanza_calculo_ex`). ed3dlib no importa ningún otro motor de cálculo de esfuerzos.
- **[V]** `calcmef.dll` (100 exports) NO resuelve el sistema: son bandas de integración de láminas (`cefbandint_*`), tensiones de Von Mises y principales, cuantías y comprobación de armado de láminas (`cefcal_*`, `cefpunto_*`), gestores normativos de losa/muro (`cefnorma_*`) y transformación de esfuerzos de lámina (`ceftransesf_*`). Es postproceso de láminas (T-P8-07 y fase de dimensionado), no el solver.
- **[V]** `calcmallad3d.dll` (51 exports `cm3dmalla_*`) es el generador de malla (nudos, barras, láminas, vinculaciones de aristas, zonas rígidas, tendones; `cm3dmalla_precision_nudos_iguales` = 0,001 m). `modelo3d.dll`/`modmallad.dll` son el modelo de láminas por planos y su malla triangular. `c3d.dll` es el núcleo geométrico (C3D), sin relación con el cálculo.
- **[V]** `cyopenseesbas.dll` (52 MB) solo contiene datos: dos paquetes comprimidos con ejecutables OpenSees versión **3.2.2** (por defecto) y **3.7.2** (se elige con la variable de entorno `CYOPENSESS_VERSION`), que `cyosbinst_instala_opensees_si_procede` descomprime (`comprlib_descomprimir_ficheros_instalacion`) en `<base>\<prefijo>.cyreview_<versión>`. También existe el modo MPI `OpenSeesSP` (`cyosbinst_ruta_mpi_openseesSP`). Es un OpenSees con elementos y utilidades propias de CYPE (`ElasticBeam3dCype`, `CypeFrBarr3D`, `ShellCype`, `ShellDKGQ`, `ShellDKGT*`, `CYPETransformation`, `CYPEMumpsSolver`, `CYPEConstraintsExpander`, `bucklingAnalysis`, `cyModalAnalysis`, `cyArpackSolver`, `cyPushoverAnalysis`).
- **Corrige** `.planning.md` línea 8 y `01_reverse_engineering_findings.md` línea 14, que citaban `calcmef`/`calcmallad3d` como solver lineal. El solver es OpenSees; `calcmef` es postproceso y `calcmallad3d` es la malla. La decisión D2 (OpenSees embebido) cubre pandeo, modal y no lineal; el original lo usa también para el lineal. La fase 8 puede seguir con Eigen (decisión del plan), reproduciendo lo que hacen el elemento y el script.

## 2. Flujo de cálculo del original [V]

1. ed3dlib vuelca el modelo a estructuras `cyos*` (nudos, barras con tramos y extremos, relaciones entre 2 nudos, diafragmas, láminas) y llama a `cyosmodelo_lanza_calculo_ex`.
2. `cyosgsmod` escribe un script Tcl por tarea (`model_task%lu.tcl`, progreso en `progress_task%lu.txt`; variable `CYOSGSMOD_NO_MULTITAREA` desactiva el reparto en tareas), reparte los casos de carga entre instancias de OpenSees (modos de uso de memoria / número máximo de instancias; textos #328074-#328079, #350250, #350251) y lanza `OpenSees`.
3. Los resultados se leen de ficheros `*.cyres` (`nodes.cyres`, `frames.cyres`, `rel2nodes.cyres`, `itri3n.cyres`, `itri6n.cyres`, `iquads.cyres`, `plastichinges*.cyres`, `multishells.cyres`; mensaje «Procesando los resultados para optimizar la velocidad de acceso desde el disco» #399701) y sus variantes `_asoc_po` (pushover).
4. Opción de depuración `ed3env_con_opciones_debug_calculo`; variables `CYOPENSEES_WRITE_EXPANDED_RELATIONS`, `CYOPENSEES_RESTART_ANALYSIS_ON_RETRY`, `CYOSGSMOD_FACTOR_ALWAYS`, `CYOS_USE_TRI3N_SHELLS`, `CYOSMODELO_RESULTADOS_EN_TEXTO`.

## 3. Receta Tcl del análisis estático lineal [V] (cadenas UTF-16 de cyopensees.dll)

```
wipe all;
model BasicBuilder -ndm 3 -ndf 6;           # 3 dimensiones, 6 gdl por nudo
constraints CYPETransformation;             # ligaduras por transformación/expansión de ecuaciones
numberer Plain;
integrator LoadControl 1;
analysis Static;
algorithm Linear -factorOnce;               # lineal: se factoriza K una sola vez y se reutiliza para todos los casos
# (no lineal: test EnergyIncr 1.e-8 10 0 + algorithm Newton; "CYOSGSMOD_FACTOR_ALWAYS" fuerza refactorizar)
#Static Linear Load Cases / #Static Non-Linear Load Cases
```

- **Sistema lineal: `CYPEMumpsSolver`** [V] (MUMPS, `dmumps`; comprueba «pfsfct(): The diagonal entry is zero», «Error -10 returned in substitution dmumps()»). Es un solver directo disperso con factorización LDLT/LU; el clon puede usar `Eigen::SimplicialLDLT` o `SparseLU` (equivalente numérico). Numeración `Plain` (sin renumeración RCM): irrelevante para el resultado.
- **Mensajes de OpenSees para mecanismo** [V] (en inglés, los traduce ed3dlib): «Displacement on null pivot. Structure is a Mechanishm», «Displacement too big. Structure is a Mechanishm», «Singular value_detected. Structure is a mechanism.». Detección: pivote nulo en la factorización y/o desplazamientos desmesurados (`ed3obra_se_han_calculado_desplazamientos_excesivos`). El umbral de «excesivo» no se ha leído [N].
- **Cada hipótesis/combinación = un caso de carga** `LC%lu` con su `pattern`; los resultados por caso se recogen con `recorder Element ... InnerStresses|InnerDeflections|axialForce|basicForce|globalForce|stresses|stressesNodes` y de nudos (desplazamientos y reacciones). Las combinaciones del cálculo se aplican antes como factores de carga (`cyosfactcarga_*`, términos `cyosccarga_crea_termino_ccarga`): el solver recibe una carga ya combinada por caso.

### Mensajes al usuario (es-ES, ids de `cyopensees.dll`) [V]

| id | Texto |
|---|---|
| 213999 | `Calculando el caso de carga '%s'` |
| 214000 | `Se ha producido un error calculando el caso de carga '%s'. Revise las condiciones del modelo.` |
| 214001 | `No es posible calcular el caso de carga '%s'. La estructura es un mecanismo. Revise la definición de cargas y las condiciones de vinculación del modelo.` |
| 380376 | `No es posible calcular el caso de carga '%s', ya que la estructura es inestable. Revise la definición de cargas y las condiciones de contorno introducidas.` |
| 328573 | `Con las condiciones impuestas, no se alcanza la convergencia para el caso de carga '%s'. Revise las condiciones del modelo.` (no lineal) |
| 320854 | `Reintentando el caso de carga '%s'` |
| 360344 | `Se ha cancelado el cálculo del caso de carga '%s'.` |

Texto de progreso en inglés del script (`puts`): `Processing model`, `Model processed`, `Processing loadcase %lu`, `Completed loadcase %lu`, `Error in loadcase %lu`, `Retrying loadcase %lu`, `Cancelled loadcase %lu`, `Convergence not achieved for loadcase %lu, but results of last committed iteration saved.` Avisos de ed3dlib relacionados (es-ES): #64010 «Se ha encontrado un nudo que no está conectado a ninguna barra ni a ninguna lámina.», #64192 «Este tramo de la barra se ha considerado indeformable.», #396568 «Esta barra se ha considerado indeformable en toda su longitud, por lo que no tiene resultados.», #64981/#64982 (longitud indeformable en el nudo ≥ longitud de la pieza/barra: no se tiene en cuenta), #64983 (cartela interfiere con extremo indeformable), #64886 (aristas de láminas con distinta vinculación exterior: el nudo se considera sin coacción).

## 4. Barra 3D [V código / N semántica de algún campo]

### 4.1 Elemento
- **[V]** 6 gdl por nudo (`-ndm 3 -ndf 6`), nudos `node %lu x y z`.
- **[V]** Línea del elemento: `element ElasticBeam3dCype tag nodoI nodoJ <props> transfTag flagPrincipal d1 d2 d3 d4 <vecXZ> <extra> <flagPandeo>;` Si `cyosbarra_se_usa_elasticbeam3dcype()` es falso se usa `element CypeFrBarr3D` con los mismos argumentos (elemento de barra con fibras/rótulas plásticas, pandeo, no lineal).
- **[V]** `<props>` (`FUN_1800738e0`) para `ElasticBeam3dCype` se compone, en este orden: dos escalares recibidos de la barra (`E` y `G` **[N: supuesto por analogía con OpenSees]**), seguidos de 6 propiedades promediadas **entre el inicio y el final del tramo** `(valor_ini + valor_fin) * 0,5` (campos 1, 4, 5, 6, 2, 3 de la estructura de tramo de 0xEA bytes creada por `cyosbarra_crea_tramo`: 8 propiedades × (ini, fin) + 1 corto + 8 doubles). La secuencia equivale por analogía a la de `elasticTimoshenkoBeam` de OpenSees: `E G A Jx Iy Iz Avy Avz` **[N: la asignación de cada campo a A/Jx/Iy/Iz/Avy/Avz no está nombrada en el binario; el orden concuerda con las aserciones `Ix, Iy, Iz > -1e-10`, `inercia_torsional + 1e-5 > 0` y con `Iy > 0`, `Iz > 0`]**. Las propiedades varían a lo largo de la barra por tramos (cartelas / canto variable): cada tramo es un elemento con la media de sus extremos.
- **[N]** Posible **deformación por cortante**: el original pasa dos propiedades adicionales (la pareja de campos 2 y 3 del tramo, medias ini/fin) tras `Iz` (código [V], significado [N]); el clon ya modela `Avy/Avz` (`CrossSection::shearAreaYM2/ZM2`, `SectionProperties::avy/avz`, «0 = ignorar»). **[N: no se ha podido contrastar numéricamente en el original si ElasticBeam3dCype aplica Timoshenko o Euler-Bernoulli (sin cálculo disponible por la licencia).]** Mientras no se verifique, la API de backend debe permitir ambos (`shearDeformation` activable) y el valor por defecto se decide con T-P8-04.
- **[V]** Torsión: St. Venant (`G·J/L`). El alabeo (`Iw`) NO se pasa al elemento lineal [V: la cadena de propiedades no contiene Iw]; el alabeo solo existe en `SectionProperties::iw` para comprobaciones.
- **[V]** Masa/peso propio: la carga de peso propio se genera como cargas de barra (`eleLoad -type -beamUniform` en el caso/pattern de peso propio, ver §6) y los mismos formatos con `eleMass` para el análisis modal. Peso específico del material = `densidadKnM3` (acero 77,01 kN/m³ en el clon).

### 4.2 Ejes locales
- **[V]** `geomTransf <tipo> tag vx vy vz [-jntOffset dxI dyI dzI dxJ dyJ dzJ];` con `<tipo>` = `Linear` o `PDelta` (cadenas `Linear` / `PDelta`); `vx vy vz` es el vector que define el plano local xz (vecxz de OpenSees). Se comprueba que los tres vectores locales (x, y, z) son unitarios (`fabs(producto_escalar - 1) < 1e-5`).
- **[N]** El cálculo concreto de vecxz y del ángulo de giro (`rollAngleDeg`, `xAxisInverted`) a partir de la barra lo hace ed3dlib antes de llamar a `cyosbarra_crea`; el clon ya tiene `Bar::rollAngleDeg` y `xAxisInverted` y el documento `ui/describir_barras.md`/`barras.md` sobre ejes locales (T-P5-02/03). El backend debe tomar los ejes locales ya resueltos (x, y, z unitarios en ejes globales) como entrada.

### 4.3 Excéntricas, brazos rígidos y «tamaño de nudo» [V]
- **[V]** Cada extremo de barra tiene una **longitud indeformable (rígida)** y un desfase (`-jntOffset` en `geomTransf`, 6 valores: offset en ejes globales del extremo I y del J). La función `FUN_1800610d0` devuelve por extremo: posición (x, y, z), desfase (dx, dy, dz) y longitud rígida. `longitud_rigida_barra_NO_elastica >= -1e-3` (aserción). La longitud del elemento elástico = distancia entre los puntos «nudo + desfase» de ambos extremos (`mt_distancia_3D`); la longitud rígida del extremo I se pasa al formateador de propiedades de posición y la del J se resta (`longitud_elastica = L_nudos_desplazados − rígida2`).
- **[V]** Las zonas rígidas se generan además como **nudos intermedios** (`id_nudo_original_si_es_generado_por_tramo_rigido`) unidos con `rigidLink beam|bar ni nj;` (cadenas `rigidLink beam %lu %lu` / `rigidLink bar %lu %lu`) y, en la opción `ed3env_introducir_barras_completamente_indeformables_en_modelo_calculo`, las barras totalmente indeformables entran como barras rígidas (si no, «Esta barra se ha considerado indeformable en toda su longitud, por lo que no tiene resultados» #396568, sin resultados).
- **[V]** «Tamaño de nudo» (#63989/#63990 de `ui/obra.md`, «Considerar la dimensión finita de los nudos»): al dimensionar uniones se **recalcula la obra introduciendo en el modelo las dimensiones de los nudos como longitudes indeformables en los extremos de las piezas** (#64785). Con la opción desactivada las barras van de eje a eje de nudo sin zona rígida. Tramos indeformables dibujados: #64273.
- **[V]** Para posiciones de esfuerzos se usa la barra «elástica» (sin zonas rígidas) frente a la «no elástica» (con zonas rígidas): `cyosresul_convierte_posicion_relativa_a_posicion_sobre_barra_elastica`, `cyosresul_reescala_posiciones_resultados_barra`; los esfuerzos se calculan en puntos de la barra elástica (`cyosbarra_set_posiciones_adicionales_esf_barra`, `posicion_puntos_esfuerzo_barra_elastica`) y se reescalan a la longitud de definición.

### 4.4 Articulaciones y liberaciones [V]
- **[V]** Cada extremo tiene coeficientes de empotramiento `Kx`, `Ky`, `Kz` (campos `Kx_coef_empotramiento`, `Ky_…`, `Kz_…` en 0x4C, 0x54, 0x5C del extremo) y los flags «flotante» de plano xy/xz. Con `ElasticBeam3dCype` el modelo exige **todos = 1,0** (aserción `|K - 1| < 1e-3`) y flotante falso: el elemento puro es de extremos empotrados; cualquier articulación o empotramiento parcial se modela fuera del elemento. En el modo `CypeFrBarr3D` los coeficientes y los planos flotantes sí se pasan al elemento. Los 4 doubles `d1..d4` de la línea del elemento proceden de los campos 0xCA..0xE2 del tramo y se escriben como 0 cuando un indicador de extremo vale 1 [V código; significado [N]: parecen datos de extremo, p. ej. longitudes/rigideces de zona de extremo].
- **[V]** Elementos de relación entre dos nudos (`cyosrel2nodos_*`): `relacion_rigida_6_gdl`, `relacion_rigida_3_gdl_desplazamiento`, `relacion_rigida_3_gdl_giro`, `relacion_rigida_1_gdl_dado`, `relacion_elastica`, `relacion_Dx/Dy/Dz/Gx/Gy/Gz`, y materiales `Elastic`, `ElasticPPGap`, `ENT` (solo compresión), `Hardening`, `Concrete01` compresión, `Multilinear`, `PySimple1` (pilotes). Se escriben como `equalDOF`, `rigidLink`, `element zeroLength ... -mat ... -dir ...` y `element twoNodeLink ... -mat ... -dir 1 2 3 4 5 6 -orient ...` (con vectores de orientación local de la relación).
- **[V]** Constantes de rigidez de las relaciones (`cyosrelac_cte_*`, leídas de los bytes): **ínfima = 1e-3**, **infinita = 1e10**, **infinita reducida = 1e8** (unidades de kN/m y kN·m/rad [N unidades]). Se usan para articulaciones (rigidez ínfima en el giro liberado), ligaduras rígidas y apoyos fijos modelados como muelles.
- **[N]** La condensación estática de una articulación (matriz del elemento con giro liberado) no existe como tal en el original: se aproxima con una relación elástica de rigidez ínfima/parcial en un nudo duplicado. Para el clon (Eigen) la condensación estática exacta es equivalente y más precisa; su diferencia con el original es del orden de 1e-3/K (despreciable). La rigidez de empotramiento parcial del clon (`EndPlaneFixity::stiffnessKnMPerRad`, coeficiente 0..1) se traduce a `K_giro = coef/(1−coef) · 4EI/L` [N: fórmula del original no verificada].

## 5. Ligaduras, apoyos y diafragmas [V]

- **Apoyos**: `fix %lu f1..f6;` para gdl bloqueados; `sp %s %12.8f;` para desplazamientos impuestos (asientos); los muelles de apoyo (constantes `Dx..Gz` elásticas, `Sentido` solo +/−) pasan por `cyosvincext_crea_por_gdl`, `cyosvincext_crea_coaccion` (por nudo/plano/recta: `crea_coaccion_desps_en_plano`, `crea_coaccion_desps_en_recta`: apoyos con vectores/ángulos no alineados con los ejes) y `cyosvincext_crea_libre`; el original los resuelve con `equalDOF` y relaciones (`CYPEConstraintsExpander::expandConstrainedNodesEquations` y `addNewEquationDoingExpansion` generan las ecuaciones de ligadura multi-punto y las eliminan por expansión: método de transformación, no penalización).
- **Apoyos con ángulos** [V]: restricciones en recta/plano oblicuos se resuelven con ecuaciones de relación (`dofRelation tag … nTérminos (nudo gdl coef)*`, `cyosrelgenln_*` relación lineal genérica con términos arbitrarios).
- **Ligaduras** (grupos de nudos con mismo Dx/Dy/Dz): `equalDOF maestro esclavo gdl…;`.
- **Diafragma rígido**: `rigidDiaphragm 3 nudoMaestro nudoEsclavo;` (perpendicular al eje 3 = Z), con un nudo maestro generado (`tag_opensees_nudo_diafragma_si_relacionado`) y cargas globales en el diafragma (`cyosdiafrigid_append_carga_global`: Fx, Fy en el punto).
- **Rigidez de relaciones**: `uniaxialMaterial Elastic tag K;` por muelle.

## 6. Cargas [V formatos / N detalles]

Formatos del script (de ed3dlib/cyosbarra a OpenSees):

| Carga | Comando Tcl |
|---|---|
| Nodal | `load nudo Fx Fy Fz Mx My Mz` (valores combinados; global) |
| Barra uniforme | `eleLoad -ele tag -type -beamUniform wy wz wx` (ejes locales de la barra) |
| Barra trapezoidal/triangular/tramo | `eleLoad -ele tag -type -beam3dTrapLoadDetPos` + 8 valores (posiciones inicial/final y cargas locales Y, Z, X en cada extremo: `cargax1/y1/z1`, `l1`, `cargax2/y2/z2`, `l2` de `cyosbarra_append_carga_trapezoidal`) |
| Barra puntual | `eleLoad -ele tag -type -beamPoint Py Pz x Px …` (carga y posición a lo largo de la barra) |
| Temperatura barra | `eleLoad -ele tag -type -beam3dTempLoad T_sup T_inf …` (variación en ejes locales: `cyosbarra_append_carga_temperatura_locales_barra`; dilatación = `thermalExpansion` del material) |
| Momento puntual/pretensado | tendón: `cm3dmalla_crea_carga_tendon_final/transferencia`, `cyosmodelo_append_tendon` (pretensado, fuera de esta tarea) |
| Lámina | `eleLoad -type -uniftrappressure`, `-tempinc`, `-OrthotropicTempInc`, `-RetractionInc` (T-P8-02) |

- **[V]** Las cargas globales de barra se transforman a locales antes de emitirse; las que caen sobre zona rígida se recortan a la barra elástica (aserciones `l1_no_elastica`, `l1_elastica < l2_elastica`, `fabs(l1_elastica_sobre_NO_elastica - pos_ini_barra_elastica) < 1e-4`; las cargas en zona rígida se trasladan al nudo, cadena `hay_carga_sobre_barra_elastica`).
- **[V]** Peso propio: carga repartida uniforme = `peso_específico · A` (kN/m, hacia −Z), generada por barra salvo desactivación individual (`BarLoads::selfWeightOff`, #64002); en láminas `peso_específico · espesor` (carga superficial). **[N: la fórmula exacta con áreas medias por tramo no está leída; razonablemente A_tramo·γ.]**
- **[V]** El cálculo de esfuerzos de cargas sobre una barra no divide la barra en elementos por defecto: `ElasticBeam3dCype` incluye las cargas de barra dentro del elemento (los esfuerzos internos se obtienen con `InnerStresses`/`InnerDeflections` en los puntos de esfuerzo, 'posiciones adicionales'). Opciones de discretización de barras (`con_discretizacion_barras_por_numero_tramos`, `num_tramos_discretizacion_barra`, `longitud_maxima_tramo_discretizacion_barra`) para el análisis de pandeo/no lineal.
- **[N]** Pretensado y retracción de barra: no localizados en el script de barra (solo lámina y tendón).

## 7. Convenio de signos y resultados [N / V parcial]

- **[V]** Resultados por barra en puntos de esfuerzo (`cyosresul_resultados_barra`, `cyosresulbas_esfs_pos_barra_*`): axil, cortantes Vy Vz, torsor, flectores My Mz en **ejes locales**, y por nudo de barra (`cyosresul_resultados_nudo_barra`: esfuerzos en cada extremo). Reacciones: fuerzas/momentos en nudos con apoyo (`cyosresul_resultados_nudo`: desplazamientos y reacciones en ejes globales), cargas de reacción reutilizables como cargas hiperestáticas (`cyosnudo_append_reacciones_nudo_como_cargas_para_calculo_hiperestatico`).
- **[N]** Convenio exacto (axil positivo = tracción, signos de cortante/flector respecto a los ejes locales): no leído; la tarea T-P8-06 debe medirlo con una captura de esfuerzos del original (bloqueado por licencia) o fijar el convenio estándar de OpenSees (`basicForce`: N tracción positiva) y documentarlo como supuesto.

## 8. Valores de referencia (analíticos / solver independiente) [N respecto al original]

No proceden del original. Datos comunes: E = 210000 MPa (210e6 kN/m²), ν = 0,3 (G = 80769,23 MPa), A = 0,01 m², Iy = 1e-4 m⁴, Iz = 5e-5 m⁴, J = 8e-5 m⁴, Avy = Avz = 0,004 m². Ejes locales de barras horizontales: x a lo largo, z hacia arriba; flexión alrededor de y.

Solver independiente de verificación: ensamblado directo con matriz de rigidez 12×12 de Euler-Bernoulli (con y sin deformación por cortante, φ = 12EI/(G·Av·L²)) y transformación de ejes; reproduce las fórmulas cerradas al 1e-9.

| Caso | Resultado | Valor |
|---|---|---|
| C1 voladizo L = 4 m, P = 10 kN en −Z en el extremo libre, nudo 0 empotrado | δz extremo, Euler-Bernoulli | −1,015873016e-2 m (= PL³/3EI) |
| | δz con cortante (Timoshenko) | −1,028253968e-2 m (+ PL/(G·Avz)) |
| | giro ry extremo | 3,809523810e-3 rad (= PL²/2EI) |
| | reacción Rz / My en el empotramiento | 10,000 kN / 40,000 kN·m (módulo) |
| C2 viga biapoyada L = 6 m, q = 10 kN/m (−Z), 12 elementos con cargas nodales equivalentes | δz centro, EB | −8,035714286e-3 m (= 5qL⁴/384EI) |
| | δz centro con cortante | −8,175000000e-3 m (+ qL²/8GAv) |
| | reacciones | 30,000 kN en cada apoyo; M máx = qL²/8 = 45 kN·m |
| C3 pórtico plano XZ: pilares 4 m, vano 6 m, bases empotradas, H = 20 kN en +X en el nudo 1 | ux nudo 1 / nudo 2, EB | 4,083155886e-3 / 4,054654597e-3 m |
| | giro ry nudo 1, EB | 7,686193445e-4 rad |
| | reacciones base 0: Fx, Fz, My | −10,0245 kN, −5,3286 kN, −24,0843 kN·m |
| | reacciones base 3: Fx, Fz, My | −9,9755 kN, +5,3286 kN, −23,9441 kN·m |
| | con cortante: ux nudo 1 / nudo 2 | 4,233218287e-3 / 4,204715537e-3 m |

(Equilibrio comprobable: ΣFx = −20, ΣMy de las bases + H·4 + Fz·6 = 0.)

La diferencia relativa entre EB y Timoshenko es del 1,2 % en C1 (L/h ≈ 20) y hasta 3,7 % en C3: **T-P8-04 debe fijar tolerancias teniendo en cuenta esta ambigüedad [N]**; para decidir cuál usa el original hace falta un cálculo del original (bloqueado) o la lectura del elemento `ElasticBeam3dCype` extrayendo el ejecutable OpenSees del paquete (no hecho: ver §10).

## 9. Estado del modelo del clon (`src/core`) y huecos

Ya existe (todo como datos, sin solver):
- **Geometría**: `Node{id, position}`, `Bar{startNode, endNode, materialId, sectionId, rollAngleDeg, xAxisInverted}` (`structure_model.h`), `Piece`.
- **Material**: `Material{E (MPa), ν, densidad kN/m³, dilatación}` con `shearModulusMPa()`. **Sección**: `CrossSection{A, Iy, Iz, J(torsion), Avy, Avz}` (0 = ignorar cortante) y `Section`/`SectionProperties` completos con `iw`, `propertiesAt(t)` para canto variable (`section_properties.h`).
- **Extremos**: `BarEnds` con `EndPlaneFixity{coeficiente 0..1 | rígido de giro kN·m/rad}` por plano (xy, xz, yz) y extremo, flag `pinned`.
- **Apoyos**: `NodeSupports{external, internal}` con `ExternalSupport` (tipos predefinidos, genérico, muelles por gdl con constante y sentido, desplazamientos impuestos por hipótesis, vectores de usuario) y `restraintOf()`, `InternalLink` (empotrado, parcial, articulado, genérico por fila de elemento).
- **Ligaduras / diafragmas**: `NodeLigatures{dirs[3]}`, `Diaphragms` (nudos, láminas, cargas Fx, Fy, x, y).
- **Cargas**: `BarLoad` (puntual, uniforme, tramo, triangular izq/dcha, trapezoidal, momento puntual, temperatura, retracción…) con ejes globales/locales, posiciones relativas o absolutas; `NodalLoad` (kN, 7 direcciones); `LoadCaseSet`/`LoadCase` (naturaleza), combinaciones y estados límite (`combinations`, `limit_states`).
- Láminas: `Shell`, `ShellMaterial`, propiedades (T-P8-02).

Falta para el solver:
1. **Ejes locales ya resueltos**: función que dé (ex, ey, ez) a partir de nudos, `rollAngleDeg` y `xAxisInverted` y el criterio de barra vertical (T-P5-03; comprobar que existe una función pública, si no, crearla en el backend).
2. **Excéntricas y longitudes rígidas por extremo** (tamaño de nudo, cartelas, brazos): no hay campo en `Bar`; falta modelo + mapeo a `-jntOffset` (TK-021).
3. **Propiedades por tramo con media ini/fin**: usar `propertiesAt(0)` y `propertiesAt(1)` y promediar.
4. **Material/sección mínimos (TK-021)**: `CrossSection` es el contenido que necesita el solver; falta el puente `Section → CrossSection` automático (área efectiva, inercias, J, Avy, Avz de `effectiveProperties`).
5. **Coeficientes de empotramiento parcial** por extremo del nudo/pieza → rigideces de giro (hay datos, falta la ley).
6. **Cargas**: no existe aún conversión carga → fuerzas nodales equivalentes ni al formato del elemento (uniforme, trapezoidal, puntual, temperatura, momentos), ni la hipótesis automática de peso propio.
7. **Hipótesis y combinaciones aplicadas** al solver: el original resuelve una combinación por caso (cargas combinadas); el clon debe elegir entre resolver por hipótesis y combinar resultados (más barato para análisis lineal con factorización única).
8. **Pretensado y retracción de barra, tendones**: sin modelo.
9. **Opciones de cálculo**: tamaño de nudo, barras indeformables, error por nudos redundantes (`ed3env_considerar_nudos_redundantes_como_error_impide_calcular`), discretización: sin modelo de opciones.

## 10. Vías no recorridas (para quien continúe)

- Extraer los ejecutables OpenSees 3.2.2/3.7.2 del bloque comprimido de `cyopenseesbas.dll` (formato `comprlib` de CYPE, 52 MB) permitiría decompilar `ElasticBeam3dCype` y `CYPETransformation` con REA y comparar con los valores de §8; se descartó en esta sesión por coste y por el aviso de licencia de OpenSees (TK-007: redistribución restringida a entidades educativas/sin ánimo de lucro; solo análisis local, nunca copiar el binario al repositorio).
- No se identificó el consumidor de `cyosbarra_crea_tramo` en ed3dlib (importación diferida sin referencias directas en Ghidra), por lo que no se pudo nombrar cada campo del tramo.

## 11. Especificación para backend (T-P8-01, C++20 + Eigen)

### 11.1 Estructuras de datos propuestas (`src/core/fem/`)

```cpp
namespace cc::fem {
struct Vec3 { double x, y, z; };
struct FemNode { int id; Vec3 pos; std::array<bool,6> fixed{}; std::array<double,6> spring{}; // N/m, N·m/rad; 0 = none
                 std::array<double,6> prescribed{}; };                                           // settlement per DOF (per load case in LoadCaseData)
struct BarSection { double area, iy, iz, j, avy, avz; };           // avy/avz == 0 => no shear deformation
struct FemBar { int id, nodeI, nodeJ; double e, g; BarSection sec; // sec = average of start/end (tramo)
                Vec3 ex, ey, ez;                                   // resolved local axes (global components)
                Vec3 offsetI, offsetJ;                             // rigid end offsets / zones (global), 0 by default
                std::array<double,3> releaseI{1,1,1}, releaseJ{1,1,1}; // Kx,Ky,Kz coefficients 1..0 ("coef. empotramiento")
                double selfWeightKnM = 0; };                       // gamma*A (0 if disabled)
struct BarLoadElem { int bar; enum Kind { Uniform, Trapezoid, Point, Temperature, MomentPoint } kind;
                     bool local; Vec3 q1, q2; double pos1, pos2; double t1, t2; };
struct LoadCaseData { std::string name; std::vector<NodalForce> nodal; std::vector<BarLoadElem> bars;
                      std::map<int,std::array<double,6>> settlements; };
struct RigidDiaphragm { int master; std::vector<int> slaves; };      // axis = Z
struct LinearModel { std::vector<FemNode> nodes; std::vector<FemBar> bars; std::vector<std::pair<int,int>> equalDof; /*+gdl mask*/
                     std::vector<RigidDiaphragm> diaphragms; };
struct LinearResult { std::vector<std::array<double,6>> disp; std::vector<std::array<double,6>> reactions;
                      /* per bar: forces at i/j (local) + sampled diagram points */ };
enum class SolveStatus { Ok, Mechanism, Unstable, Error };
}
```

### 11.2 Algoritmo (orden de ensamblado)

1. Numerar gdl: 6 por nudo, orden de ids; calcular nudos esclavos.
2. Matriz local 12×12 del elemento: EA/L; GJ/L; flexión en xz con (Iy, Avz) y en xy con (Iz, Avy) con φ = 12EI/(G·Av·L²) (φ = 0 sin cortante, es Euler-Bernoulli). Longitud L = distancia entre nudos desplazados (offset) menos zonas rígidas; barra totalmente rígida: no se genera elemento (nudos unidos por relación rígida y barra sin resultados, #396568).
3. Transformación a globales con R = [ex; ey; ez] por bloque 3×3; excéntricas con matriz de transferencia rígida 6×6 (brazos rígidos = `-jntOffset`).
4. Liberaciones: condensación estática de los gdl de giro con coeficiente 0 (artículado), y rigidez parcial como resorte en serie `K_eff` (decisión §4.4 [N]).
5. Cargas de barra → fuerzas de empotramiento perfecto (reacción del elemento biempotrado) y, con liberaciones, condensación del vector; peso propio como `Uniform` local o global −Z de valor γA; temperatura: `N = EAαΔTm`, `M = EIαΔTg/h`; guardar la carga para recuperar esfuerzos internos.
6. Ligaduras y diafragmas: método de transformación (elimina gdl esclavos expresando u = T·u_r) en vez de penalización; apoyos fijos eliminan gdl; muelles suman a la diagonal; apoyos oblicuos por rotación local de los gdl del nudo (o ecuación de ligadura n·u = 0).
7. Factorizar K una sola vez (`Eigen::SimplicialLDLT<SparseMatrix>`), resolver todos los casos (multi-RHS) y recuperar reacciones `R = K·u − F` en gdl fijos (incluye ligaduras).
8. Esfuerzos de barra en N+1 puntos: a partir de los esfuerzos de extremo (de `ke·ue − fe`) y equilibrio de la carga a lo largo de la barra.
9. Mecanismo: pivote ≤ ε relativo en la factorización, o `|u|max` desmesurado (el original: «Displacement on null pivot/too big. Structure is a Mechanishm»); devolver `SolveStatus::Mechanism` con el mensaje es-ES #214001 y `Unstable` con #380376 por caso.

### 11.3 API propuesta (`src/core/linear_solver.h`)

```cpp
cc::fem::LinearModel buildLinearModel(const cc::core::Project&, const SolverOptions&);      // adapter from core model
cc::fem::LinearResult solveLinear(const cc::fem::LinearModel&, std::span<const cc::fem::LoadCaseData>,
                                  cc::fem::SolveStatus& status, std::string& messageEs);
struct SolverOptions { bool shearDeformation = false /*decide in T-P8-04*/; bool finiteNodeSize = false; bool includeRigidBars = true;
                       double mechanismPivotTolerance = 1e-12; };
```

Tests (T-P8-04): C1, C2, C3 de §8 con tolerancia 1e-6 relativa (EB) y la variante con cortante; equilibrio global de reacciones; liberación (viga biapoyada con articulación intermedia); ligadura Dz; apoyo con muelle (δ = P/K); estructura mecanismo → `Mechanism`; excéntrica (brazo rígido) con solución cerrada.

## 12. Tickets sugeridos para el ORCHESTRATOR

- **TK-NEW-A (T-P8-05 / T-P8-04)**: verificar numéricamente Euler-Bernoulli vs Timoshenko del original. Opciones: (a) clave de licencia M3D (no disponible; no eludir), (b) análisis estático del ejecutable OpenSees extraído (TK-007 aplica), (c) tomar el valor por defecto «sin deformación por cortante» y revisitar. Hasta entonces `shearDeformation` configurable.
- **TK-NEW-B (T-P8-01)**: modelo de excéntricas / longitudes rígidas por extremo y opción «tamaño de nudo» (§9 puntos 2 y 9).
- **TK-NEW-C**: corregir `.planning.md` línea 8 y `01_reverse_engineering_findings.md` línea 14 (el solver lineal es OpenSees; `calcmef` es postproceso de láminas).
- **TK-NEW-D (T-P8-06)**: convenio de signos de esfuerzos y reacciones sin verificar (§7).
