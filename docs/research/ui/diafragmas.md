# Diafragmas rígidos (T-P6-03) - investigación REA + medición

Fuente: `ed3dlib.dll` (REA/Ghidra) y CYPE 3D v2027.a ejecutado el 2026-11-06 (obra vacía + `docs/sample_project/lig_test.txt`; obra no guardada; proceso cerrado). Leyenda: [REA] código, [CAP] captura (`docs/screenshots/orig_diaf_*.png`), [NV] no verificado. Direcciones = VA de ed3dlib.dll.

## 1. Ribbon Propiedades > Diafragmas [CAP]
Tres botones (orig_diaf_01): **Editar** (40,69), **Diafragma** (50,98), **Generar por niveles** (75,127; #375654). Siempre habilitados (también en obra vacía). Barra de estado al activar: «Diafragmas - Editar» / «Diafragmas - Diafragma | - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.» / «Diafragmas - Generar por niveles».

## 2. Estructura de datos [REA]
Ficheros: `ed3diafragma.c`, `ed3ardiafragma.c`, `ed3cardiafragma.c`, `ed3lstdiafrag.c`, `ed3pndiafragma.c`, `ed3pncardiafrag.c`, `ed3rcdiafrag1.c`; colecciones `datos_obra->diafragmas` y `diafragmas_calculo`.
`ed3diafragma_t` (ctor `FUN_1807eb990`): `{ uint32 id (cypeid_nuevo_id); widestr referencia (+4); uint32 color (+0xC); ArrULong ids_nudos (+0x10); ArrULong ids_laminas_ef (+0x18); array de estructuras cargas_por_hipotesis (+0x20) }`. Duplicar (`FUN_1807e8f50`) = id nuevo, misma referencia y color y cargas, SIN nudos ni láminas. Un diafragma «nuevo» (`FUN_1807e8df0`): referencia = cadena vacía (DAT_1809687d0), color = `idcolor_colorRGB(0xd)` (magenta [CAP]), sin nudos/láminas/cargas.
Carga por hipótesis (`ed3cardiafragma_t`, lista `lista_cargas_diafragma`): hipótesis + Fx, Fy (kN) + coord. x, coord. y (m) del punto de aplicación [CAP, ver §3].
Pertenencia: el nudo guarda `datos_generados_nudo.pertenece_a_diafragma` (campo +0xE8); lámina `datos_generados.pertenece_a_diafragma`. Un diafragma puede contener **nudos y láminas (EF)** (`ids_laminas_ef`), y se exigen aristas de láminas (`indices_nudos_ini/fin_aristas_laminas_diafragma`).
Cota (`FUN_1807eb6a0`): cota Z del diafragma = **media de las Z de sus nudos** (nudos del diafragma + nudos de láminas, `FUN_1807ebca0`); «todos en la misma cota» = |Zi - Zi-1| <= 0.001 m entre nudos consecutivos. Si no (en el paso de generación de cálculo, `FUN_1807e8430` / `FUN_1807e8640`: assert `todos_los_elementos_en_misma_cota_z == CIERTO`) -> mensaje #375642 «No todos los elementos del diafragma están contenidos en el mismo plano horizontal.» (se emite como error de elemento `ebaerror_error_elem_ref_u` con las coordenadas de un nudo, `FUN_1807e9170`). **[CAP] La asignación de nudos a distintas cotas NO se rechaza al asignar**: 6 nudos de Z=0 y Z=4 aceptados en D1 (`orig_diaf_15`); el aviso es de comprobación/cálculo [NV: cuándo exactamente].
Búsqueda de diafragma inferior/superior (`FUN_1807eba40`): dada una Z y la lista de cotas de diafragmas, devuelve el más cercano por debajo (Zd + 1e-5 < Z) y por encima (Z < Zd - 1e-5) (uso: relaciones entre diafragmas, `Calcular_act_desact_relaciones_diafragma`, sismo) [REA].
Exportación a cálculo: `cyosdiafrigid_crea` / `esport3d_diafragma_crea` (diafragma rígido por cota, nudos maestros, cargas) [REA, sin decodificar].
Regeneración: `regen_diafragmas_modificados`, `regen_forzar_compatibilizar_cargas_en_diafragmas`, variable de entorno `CYPE3D_RECOMPATIBILIZAR_CARGAS_DIAFRAGMAS`; categorías `diafrags`, `gen_diafrags`, `mod_ed_diafragma_nudo`, `opc_diafragmas`, capa de dibujo `capa_diafragma`, sección `cargas__diafragmas` [REA].

## 3. Botón «Editar» - diálogo [CAP]
Ventana **«Editar»** (697x520, maximizable; título rojo; botones **Aceptar** (abajo izq., por defecto) / **Cancelar**). `orig_diaf_01/02/08`.
- Izquierda: barra de herramientas **+ (Añadir: «Añadir nuevo elemento a la lista»), duplicar, borrar (aspa roja)** y tabla de dos columnas **Referencia | Color** (`lista_diafragmas`, #191065 «Diafragmas»). Vacía al abrir en una obra nueva.
- Al pulsar + aparece una fila con referencia vacía y color **magenta** y el panel derecho: campo **Referencia** (texto) y grupo **«Cargas introducidas»** (#375657) con su propia barra + / duplicar / borrar y tabla **Hipótesis | Fx (kN) | Fy (kN) | Coord. x (m) | Coord. y (m)** (#375655 «Coord. x», #375656 «Coord. y»). Nueva carga: hipótesis = primera hipótesis de la obra («Peso propio», combo editable en la celda), Fx=Fy=x=y= **0.000** (3 decimales).
- **Validación**: con referencia vacía, añadir otro diafragma muestra el aviso «Aviso» (icono triángulo amarillo, botón Aceptar) «**La referencia no puede quedar vacía.**» (`orig_diaf_03`; id no localizado en el TSV [NV]). Referencia repetida: #375648 «La referencia '%s' está repetida.» [REA, no medido]. #375658 «No ha introducido la referencia de todos los diafragmas introducidos.» [REA: aviso al Aceptar con alguna referencia vacía; no medido].
- Los textos «Cota absoluta» (#375643), «Excentricidad X/Y» (#375649/50) y «Genéricas/Todas genéricas» (#375652/53) **no aparecen en este diálogo** [CAP]; son campos de otro contexto (listado de diafragmas/cálculo, tooltip o resultados de sismo) [NV]. #380075 «Las excentricidades no se consideran en el cálculo» [REA: aviso, contexto NV]. El color no se probó a editar (celda de color, [NV] selector).

## 4. Botón «Diafragma» - herramienta de asignación [REA + CAP]
Receptor `ed3rcdiafrag1` (datos `{int tipo_accion; uint32 id_diafragma}`; tipos 0 = asignar, 1 = desasignar; `FUN_1807f20a0`, `FUN_1807f2350`).
- Sin diafragmas definidos: diálogo «Información» (icono azul «i») «**No se ha definido ningún diafragma.**» (#184611) y no entra en la herramienta (`FUN_1807f2250`, `ebarecep_dlgavis_informacion_u`) [CAP orig_diaf_10]; hay que pulsar Aceptar (modal).
- Con diafragmas: panel flotante **«Propiedades - Asignar»** con radios **Asignar** (por defecto, negrita) / **Eliminar asignación** y, bajo ellos, combo **Diafragma** con las referencias definidas (por defecto el primero, D1) [CAP orig_diaf_11].
- Selección de nudos (y láminas) con ventana de dos clics; RMB actúa (`FUN_18020e830` nudos, `FUN_18020e620` láminas -> asigna al diafragma elegido, o a -1 (ninguno) con Eliminar) [REA]. Si no se selecciona nada, no hace nada.
- Resultado [CAP orig_diaf_14/15]: el rótulo del nudo pasa de «N4» a «**N4: D1**» (texto verde) y el cuadrado del nudo se pinta con el color del diafragma (magenta). Nudos de cotas distintas se aceptan (ver §2).
- Validaciones de asignación [REA, mensajes]: #375640 «No es posible asignar un nudo con vinculación exterior a un diafragma.», #375641 (igual para aristas de lámina), #404575 «No es posible asignar a un diafragma un nudo con ligaduras en alguno de sus grados de libertad.», y simétrica #375639 en Ligaduras. No se midió si el nudo con vinculación se rechaza al capturar (como en Ligaduras) o al RMB [NV].

## 5. «Generar por niveles» [REA + CAP]
Mensajes [REA]: #375635 «Se borrarán los diafragmas previamente definidos. ¿Desea continuar?» (pregunta, si ya había diafragmas), #375636 «No se ha generado ningún diafragma», #375637 «Se ha generado un diafragma», #375638 «Se han generado %lu diafragmas».
[CAP orig_diaf_12]: con la obra sin niveles definidos (y con un diafragma D1 existente) aparece directamente el aviso «Aviso» «**No se ha definido ningún nivel**» (sin pregunta previa de borrado). El algoritmo agrupa por niveles de la obra (cotas Z definidas en Obra), un diafragma por nivel; referencias, color y nudos asignados [NV: no medido porque requiere niveles; ver tickets]. Qué nudos entran (todos los del nivel, sin vinculación exterior ni ligaduras) y nombre de referencia generado: [NV].

## 6. Borrado/fusión y dibujo [REA/NV]
Borrar nudo o fusionar nudos coincidentes debe sacar el nudo de `ids_nudos` (regeneración `mod_ed_diafragma_nudo`) [REA parcial; NV exacto]. Dibujo 3D: no hay malla de diafragma visible en la herramienta, solo color de nudo y rótulo [CAP]; categoría de visibilidad `diafrags` / `capa_diafragma` [REA].

## 7. Pendiente de medir (para el ORCHESTRATOR)
1. Niveles de la obra (Obra > Niveles?) y Generar por niveles real (referencias, orden, pregunta #375635, mensajes #375636-38).
2. Dónde se editan cota/excentricidad X/Y (#375643/49/50/380075) y aviso #375642 en cálculo.
3. #375640/41 y #404575 al asignar (captura vs RMB), referencia repetida #375648 y #375658 al Aceptar, selector de color, Cancelar con cambios.
4. Tooltip de nudo con diafragma, Eliminar asignación, borrar/fusionar nudo, persistencia en .ed3.

## 8. Mediciones adicionales 2026-11-06 (2.ª ronda) [CAP]
- **Referencia repetida (#375648)**: al pulsar Aceptar con dos diafragmas «D1» -> «Aviso» (triángulo amarillo): «La referencia 'D1' está repetida.» (`orig_diaf_20`). El aviso es al Aceptar, no al teclear. Con referencia vacía el aviso «La referencia no puede quedar vacía.» sale al añadir otro.
- **Color editable**: clic en la celda Color abre el diálogo **«Selección del color»** (paleta de grises, 16 colores básicos, rejilla de matices 24x9 con el actual resaltado, valores «210» (tono) con vista previa, botón «Más colores...», Aceptar/Cancelar) (`orig_diaf_21`). Magenta = tono 210 en su paleta.
- **Tooltip de nudo asignado**: título «N1 (0, 0, 0)» y debajo «Diafragma : D1» (`orig_diaf_23`). El rótulo «N1: D1» se pinta en teal si el nudo está bajo el cursor/hover como en Ligaduras.
- **Eliminar asignación**: el radio oculta el combo (solo queda Asignar/Eliminar asignación); ventana sobre N1 + RMB -> N1 pierde «: D1» y su color; los demás se mantienen (`orig_diaf_25`).
- **Ligar un nudo de diafragma (#375639)**: con Ligaduras (Asignar, X) sobre nudos de diafragma -> diálogo «**Error**» (icono rojo) «No es posible asignar ligaduras a nudos que pertenecen a diafragmas» (`orig_diaf_24`); los nudos se pintan en naranja (selección).
- Siguen sin medir: #404575 (asignar a diafragma un nudo ya ligado; la selección con ligaduras no se probó por orden de pasos), #375640 (nudo con vinculación exterior; requiere el diálogo de Vinculación exterior), borrar/fusionar nudo, #375642 (no alcanzable por GUI sin cálculo: Calcular no se ejecutó), ubicación de Cota absoluta / Excentricidad X/Y, y Generar por niveles real (depende de los niveles de la obra, no localizados en esta pasada; ticket con el módulo Planos/Niveles).
