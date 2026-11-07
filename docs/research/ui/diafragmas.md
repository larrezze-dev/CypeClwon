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


## 9. Resolución por REA (TK-088, 2026-11-07, solo código; sin lanzar el original) [REA]
Fuente: `ed3dlib.dll` (Ghidra 12.1.4 vía MCP rea), direcciones VA. Sustituye a las hipótesis [NV] de §3-§7 donde se indica.

**9.1 Ribbon (constructor del menú `diafragmas`, `FUN_1803da6d0`, llamada en 0x1803da884)**. Orden y ids de botón: (1) `opc_diafragmas` «Editar» (#74834, `entorno.msg`; icono `diafrags`; diálogo de obra `FUN_180411310`); (2) `mod_ed_diafragma_nudo` «Diafragma» (#184614, `ed3texto.msg`; icono `anudiaf`; segundo texto #55238 «Asignar» = título del panel «Propiedades - Asignar»); (3) `gen_diafrags` «Generar por niveles» (#375654, `ed3mnuob.msg`; icono `dgdiafs`; manejador `FUN_180004070`). En los tres, los argumentos de «título de tooltip» y «cuerpo de tooltip» pasados al constructor son 0: **no existe texto de tooltip para Editar, Diafragma ni Generar por niveles** (explica los 5 intentos sin tooltip de la medición). Contraste: Empotramiento sí pasa título (#64367) y cuerpo (#64631+#64632). Existe además un botón de depuración «[Dbg] Usar relaciones» (`Calcular_act_desact_relaciones_diafragma`) solo con la opción de entorno de depuración de cálculo (`ed3env_con_opciones_debug_calculo`): no clonar. Los botones de duplicar/borrar del diálogo Editar son controles genéricos de lista (barra +, duplicar, borrar): sin texto propio en este módulo.

**9.2 «Generar por niveles» - algoritmo real** (`FUN_180004070` -> `FUN_18022ee80` -> `FUN_180465c90` -> `FUN_1801db330` -> `FUN_1801748b0` -> `FUN_18017e240`):
1. Si la obra no tiene niveles (`ed3obra_hay_niveles` falso): aviso (`dlgavis_aviso`) #25932 «No se ha definido ningún nivel»; fin (no se borra nada). Coincide con CAP orig_diaf_12.
2. Si ya hay diafragmas (`FUN_180231090`): pregunta con **«No» por defecto** (`dlgavis_preguntar_no_por_defecto`) #375635. Si responde No, fin.
3. Generación: copia la lista de niveles y la **ordena por cota ascendente** (comparador `FUN_18078f3a0`: `cypemath_compara_doubles` sobre la cota, offset +0xC del nivel, tolerancia 0.001 m). Recorre `i = 1 .. n-1` tomando el elemento de índice `n-i`: **un diafragma por cada nivel salvo el más bajo (índice 0), empezando por el más alto**. Con un solo nivel no se genera ninguno.
4. **Referencia** del diafragma = referencia (nombre) del nivel (+4). Si el nombre de nivel se repite, el primero conserva el nombre y los siguientes se llaman `"%s_%lu"` (nombre_1, nombre_2, ...; contador por nombre).
5. **Nudos**: todos los nudos de la obra cuya cota Z coincide con la del nivel con tolerancia 0.001 (`SON_IGUALES_PREC`, `FUN_18078ff30`). **Láminas EF**: las láminas cuyos nudos están TODOS en ese nivel (`FUN_18033be60`; exige > 0 nudos). No hay filtro por vinculación exterior ni ligaduras en la generación (esos casos solo se señalan en la comprobación, 9.4).
6. **Color**: índice `i` (1 = nivel más alto): `idcolor_colorRGB(base + (i mod 24)*10)` con `base = 12` si `i < 24`, `11` si no (`FUN_1802a2570`). RGB real de `idcolor_colorRGB`: no decodificado (ver 9.7).
7. La colección generada **sustituye** a la anterior (se destruye y se reemplaza; cargas por hipótesis vacías). Después se regeneran datos (`FUN_180591b40`/`FUN_180593460`) y se refresca la vista (`FUN_180868fa0`).
8. Resultado (`dlgavis_informacion`): 0 -> #375636 «No se ha generado ningún diafragma», 1 -> #375637 «Se ha generado un diafragma», n -> #375638 «Se han generado %lu diafragmas».

**9.3 Cota absoluta / Excentricidad X / Excentricidad Y / Cargas en diafragmas / Genéricas / Todas genéricas (#375643, #375649, #375650, #375651, #375652, #375653)**: pertenecen a la tabla `ed3lstdiafrag.msg` (módulo `ed3lstdiafrag.c`, `FUN_1807ecec0` y vecinas): son **columnas/títulos del listado de diafragmas** (listados de la obra, «Cargas en diafragmas»), **no** del diálogo Editar. #380075 «Las excentricidades no se consideran en el cálculo» es una nota asociada a ese listado [contexto exacto NV, ver 9.7]. No hay campos cota/excentricidad editables en el diálogo Editar (coincide con CAP).

**9.4 Avisos #404575 / #375640 / #375641 / #375642 / #375639 / #375648 / #375658** (tablas `ed3diafragma.msg`, `ed3pndiafragma.msg`):
- #404575 (ligaduras), #375640/#375641 (vinculación exterior de nudo/arista) y #375642 (cotas distintas) **no son diálogos de asignación**: se emiten como *errores de elemento* (`ebaerror_error_elem_ref_u`, cabecera `"%s, %s:"` con referencia del nudo y del diafragma, más coordenadas del nudo; p. ej. `FUN_1807eb460` emite #404575 para cada nudo del diafragma presente en la lista de nudos con ligaduras) dentro de la **comprobación/cálculo** (lista de errores). No alcanzables sin Calcular.
- Asignar (RMB) `FUN_1801bdd20(datos, ids_nudos, id_diafragma|-1, obra)`: (a) quita los nudos de TODOS los diafragmas (`FUN_1807e7380`); (b) si id != -1, los añade al diafragma (`FUN_1807e8940`) y **los elimina de las tres colecciones de grupos de nudos de datos+0x48/+0x50/+0x58**, que por orden de campos son `ligaduras_dx`, `ligaduras_dy`, `ligaduras_dz` [identidad inferida del orden de las cadenas `datos_obra->ligaduras_dx/dy/dz`; verificar midiendo]. Es decir, **asignar un nudo a un diafragma le quita automáticamente sus ligaduras**, sin aviso. «Eliminar asignación» (id = -1) solo lo quita de los diafragmas. Sobre vinculación exterior no hay acción: el nudo queda asignado y solo se avisa en la comprobación.
- #375639 (Ligaduras sobre nudo de diafragma) sí es diálogo «Error» en la herramienta Ligaduras (medido).
- #375648 / #375658: validación del diálogo Editar (medido).

**9.5 Borrar/fusionar nudo de un diafragma**: el único llamador de `FUN_1807e7380` (quitar ids de todos los diafragmas) es la asignación (`FUN_1801bdd20`); el saneamiento al borrar o fusionar nudos se hace por otra ruta (regeneración `regen_diafragmas_modificados`) no localizada [NV].

**9.6 Color**: diafragma nuevo manual = `idcolor_colorRGB(0xd)` (sec. 2); generados = fórmula 9.2.6.

## 9.7 Lo que REA no responde (medir en el original)
1. RGB real de la paleta `idcolor_colorRGB(12 + k*10)` usada por Generar por niveles (k = 1, 2, 3...): crear 3 niveles distintos (más el más bajo), Generar por niveles y leer el color de la celda Color en Editar (o decodificar `idcolor_colorRGB`).
2. Dónde se define la lista de niveles (Obra > Niveles; ver planos.md) y comprobar que el nivel más bajo se omite y la ordenación por cota (crear niveles fuera de orden y con nombre repetido para ver `nombre_1`).
3. Verificar que asignar un nudo ligado a un diafragma le quita las ligaduras (Ligaduras X sobre N1, Diafragma > Asignar sobre N1, volver a Ligaduras: ¿desaparece el rótulo de ligadura?).
4. Contexto visible de «Cota absoluta/Excentricidad X/Y/Cargas en diafragmas/Genéricas»: abrir los listados de la obra con un diafragma definido (Obra > Listados).
5. #375640, #375642 y #404575: requieren Calcular/Comprobar con nudo de diafragma con vinculación exterior / cotas distintas; capturar la lista de errores (formato «N1, D1:»).
6. Borrar un nudo asignado y fusionar nudos coincidentes: ver qué ocurre con el diafragma (¿queda vacío?, ¿se mantiene la referencia?).
