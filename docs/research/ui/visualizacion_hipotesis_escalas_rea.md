# Visualización de hipótesis y escalas (T-P7-07) — parte REA

Sesión 2026-11-11. Vía: [REA] sobre `ed3dlib.dll` (decompilación Ghidra, funciones sin nombre; direcciones 0x18...) y cadenas UTF-16 con id (`tools/extract_dll_strings.py`). Marcas: **[V]** verificado (código, o medido en el original en documentos previos citados), **[N]** no verificado.

NOTA DE COORDINACIÓN: durante esta sesión otro proceso del RE-ANALYST (ficheros `vh_*`, `tools/vh_lib.sh`, `tools/vh_open.ps1`, capturas `vh_*.png`) estaba midiendo en el original la misma tarea con las obras de ejemplo. Para no pelear por el escritorio este documento contiene SOLO la parte REA y la revisión del clon; no se lanzó ninguna medición en el original (el proceso que lancé se cerró; no queda ninguno mío). Este fichero NO sustituye a `visualizacion_hipotesis_escalas.md` (que escribirá la otra sesión): hay que fusionarlos.

## 1. Estructura de escalas de cargas (obra) [REA, ed3esccar.c]

La obra guarda `datos_obra->escalas_cargas` (cadena de aserción 0x180a6f918) [V]. Función de validación FUN_1802dc780 (`ed3esccar.c`, líneas 0x5f-0x6a): estructura **empaquetada** (sin alineación), todos los campos double (8 bytes) salvo el primero [V]:

| Offset | Campo | Valor por defecto medido |
|---|---|---|
| +0x00 | (2 bytes, no aserto; casi seguro el modo) [N] | Fixed |
| +0x04 | `dimension_max_representacion_carga` (> 0.0001) | 0.500 m |
| +0x0c | `valor_escala_carga_uniforme` (> 0.0001) | 1.000 |
| +0x14 | `valor_escala_carga_puntual` | 1.000 |
| +0x1c | `valor_escala_carga_faja` | 1.000 |
| +0x24 | `valor_escala_carga_triangular` | 1.000 |
| +0x2c | `valor_escala_carga_trapezoidal` | 1.000 |
| +0x34 | `valor_escala_momento_puntual` | 1.000 |
| +0x3c | `valor_escala_variacion_temperatura` | 1.000 |
| +0x44 | `valor_escala_carga_superficial` | 1.000 |
| +0x4c | `valor_escala_carga_retraccion` | 100.000 |
| +0x54 | `escalas_hipotesis` (puntero a lista de `ed3eschip_t`, no nulo) | Peso propio 1.000 |

Los valores por defecto son los medidos en `cargas_nudos.md` §7 / `cargas_barras.md` §12.5 [V medido]; la tabla de offsets es [V REA]. Nótese que en memoria **temperatura (+0x3c) va antes que superficial (+0x44)**, mientras que en el diálogo Superficial va antes que Temperatura.

### 1.1 Regla de tamaño de dibujo (FUN_1802dd290, `ed3esccar.c` líneas 0x2c2-0x2db) [V REA]
Entradas: modo (0 = fijo, 1 = proporcional), `dimension_max_representacion_carga` (>0), escala del tipo (>0), hipótesis. Salida: registro de 18 bytes `{short tipo; double dimMax; double factor}` (FUN_1803003c0):
- Modo **fijo** (0): `tipo = 1`, `factor = 0`: la carga se dibuja con **tamaño máximo = dimMax**; el valor mayor de la carga alcanza dimMax y los demás son proporcionales a él (concuerda con lo medido: todas las bandas 0.5 m; cuña 1/3..1).
- Modo **proporcional** (1): `tipo = 0`, `factor = escala_tipo x escala_hipótesis` (la escala de la hipótesis se busca en la lista con `hipocal_son_iguales`); el consumidor aplica el factor al valor (medido: 0.1 m por unidad x factor). El 0.1 m/unidad no está en estas funciones (vive en el consumidor, no localizado) [N en código; V medido].
- Cualquier otro modo: `error_interno_default_error` (ed3esccar.c 0x2db): solo existen dos modos [V].

### 1.2 Diálogo «Escalas» (construcción) [V REA]
- Botón de ribbon `Carga_escalas` (#64337 «Escalas», icono `dimens2`) y `Carga_hipotesis_vista` (#65380) cuelgan del menú `cargas_opcs_visualiz` (FUN_1803d89f0, `ed3mnucar.c`); ambos son «botones de diálogo de obra» sobre `ed3datos_t`/`ed3datvi_t` (datos de obra y de vista). Callbacks: `Carga_escalas` -> FUN_1803d9f40; `Carga_hipotesis_vista` -> FUN_1803d9fc0 -> constructor FUN_1802415c0 (exige `hipotesis_usuario`, `categorias_usos`, `norma_hormigon`, `norma_laminados`, `norma_conformados`; la lista de hipótesis depende por tanto de las normas) [V].
- Panel `escalas_cargas_hipotesis` (FUN_1802dd000): caja vertical = lista `lista_cargas_por_hipotesis` (arriba) + panel `cargas_por_tipos` (abajo, margen vertical 5) [V].
- **Tabla Hipótesis | Escala** (FUN_1802dc9e0): estructura `ed3eschip_t`; columna 1 calculada (texto de la hipótesis, offset 0xe), columna 2 numérica editable (offset 8) con **mínimo 0.001, máximo 1000.0** (constantes 0x3f50624dd2f1a9fc y 0x408f400000000000), **no ordenable**, tamaño 0xfa x 0x82 [V].
- **Campos por tipo** (FUN_1802dcd10 + FUN_1802dd110): rejilla 9x3, título en negrita «Escalas de cargas por tipos» (#64478); nombres internos y orden de filas: `uniforme, puntual, faja, triangular, trapezoidal, momento_puntual, superficial, temperatura, retraccion`; todos con **mín 0.001, máx 1000.0, paso 0.001**, 6 caracteres de ancho, formato 3 decimales [V]. Retracción (por defecto 100.000) tiene el mismo rango: **1000.000 es el máximo** [V].
- Cadenas: #388085 «Dibujar las cargas con relleno»; #388086 «Proporcionales a un valor fijo»; #388087 «Tamaño máximo para representación»; #388088 «Proporcionales al valor de la carga»; #64652 tooltip «Permite indicar la escala de dibujo para las cargas introducidas.» [V].
- Rango del campo «Tamaño máximo para representación»: no localizado en el código (dialogo de modo no decompilado) [N]; la aserción exige > 0.0001 (a nivel de datos) [V]. El clon usa `InvalidScale` (no finito o no positivo): coherente con la aserción.

### 1.3 Hipótesis vista [REA]
- `obra->configuracion->hipotesis_vista` (ed3config.c 0x50, aserción 0x180a65590/0x180ba39d0): la hipótesis vista es **un puntero a hipótesis dentro de la configuración de la obra** (vive con la obra, no con la ventana) [V]. Los diálogos se abren con «hipótesis vista» de ahí.
- Setter (FUN_1801941f0, ed3config.c 0x2de): si la hipótesis vista actual **ya no está definida** para estructuras 3D (`hipocal_esta_definida_para_estructuras_3D`: p. ej. borrada o no admitida al cambiar de normas), se destruye y se sustituye por la hipótesis por defecto devuelta por FUN_18032c890 (Peso propio; la identidad de ese valor es [N] pero coincide con lo medido en `cargas_panos.md` §12.3: «la vista queda en el resto (se vio el color de Peso propio)») [V parcial].
- «Ver todas» (#38947 «Ver todas», ya presente en el cuadro Hipótesis vista del original): **no se encontró** un campo con nombre ni cadena de aserción propia para ese indicador en `ed3dlib.dll`; dónde se guarda (obra o sesión) y si se persiste en el `.ed3` es [N].
- Textos: #65380 «Hipótesis vista» (en: Visible loadcase); #64651 tooltip (ya en hipotesis_carga.md); #64128 (cargas superficiales: «sólo se muestran … las cargas superficiales pertenecientes a la 'hipótesis vista'… ésta pasará a estar oculta si no se encuentra en la 'hipótesis vista' actual»); #64679 y #64681 (Editar y Borrar superficiales actúan sobre la hipótesis vista) [V].

## 2. Resultados: escalas y textos (dependen del solver, T-P8-06) [REA]
- Barrido de los 682 binarios de la carpeta del original con 12 frases UTF-16 («Escala de la deformada», «Escala de los esfuerzos», «Escala de las reacciones», «Escala de dibujo», «Escala de las leyes», «Escala de visualiza», «Escala de desplaz», «Escala del diagrama», «Escala de esfuerzos», «Amplificaci», «Escala de las gr», «Tamaño de los textos»): **ninguna** coincidencia con escala de deformada, diagramas o reacciones; «Tamaño de los textos» aparece solo en `ed3dlib.dll`, `dibento.dll`, `enplanos.dll` (#74878, panel Opciones de visualización y planos). Es decir, los diálogos de resultados no tienen un literal «Escala de …» localizable; si la escala de deformada/diagramas es un control con otro texto o un simple deslizador, es [N] y exige medir con una obra calculada en el original [N].
- Cadena #64458 (texto de ayuda de un ejemplo/tutorial): «Después de calcular, observe la deformada de la barra. Aumente, si es necesario, la escala de visualización para ver claramente su forma.» => existe una «escala de visualización» de la deformada ajustable por el usuario; su control y rango son [N].
- #64076: los esfuerzos y la flecha se dibujan en **otros colores, independientemente de su valor**, para distinguir las gráficas activas a la vez [V]. #64075: la envolvente de tensión se dibuja como gráfica (rojo en los tramos que incumplen, verde en el resto) o con isovalores [V].
- Ayudas de los diálogos de resultados: #64160 (Esfuerzos: leyes, deformada y flechas o envolventes de esfuerzos, flecha y tensión), #64842 (ribbon «Esfuerzos»), #64841 «Deformada e isovalores de la ventana activa», #64072/#64073 (Reacciones: hipótesis simple, combinación o envolvente; clic en el nudo muestra un cuadro con los valores, un segundo clic lo oculta), #64754, #65493/#65495/#65497 (envolventes de reacciones de equilibrio y de tensiones sobre el terreno), #65496 (valores de envolvente no concomitantes), #64522-#64531/#64547/#64548 (títulos de listados por hipótesis, por combinación y envolventes) [V como textos].
- #64250 «Mostrar textos sobre los elementos» / #64251 (activa/desactiva el dibujo de textos sobre los elementos) y #74878 «Tamaño de los textos» (Opciones de visualización): ya cubiertos en `viewport_3d.md`/`etiquetas_panel.md`.
- Color de hipótesis: no existe literal ni nombre de campo en el código (`color.*hipot` sin resultados); la paleta se obtiene del dibujo. Medido: Peso propio amarillo (255,255,0), 1.ª adicional cian (0,191,255), 2.ª verde (0,255,0), 3.ª magenta (255,0,255), 4.ª marrón (204,102,0) por ORDEN DE CREACIÓN (`cargas_panos.md` §14.2) [V medido para paños/láminas]. 5.ª y siguientes, y si nudos/barras usan la misma paleta: [N] (en el clon `Viewport3D::drawNodalLoads` y `drawBarLoads` pintan siempre amarillo, ver §3).

## 3. Revisión del clon (qué existe, qué falta)
Existe: `core/nodal_load.h` `LoadScales {mode, fixedSizeM, perCase, perType[9]}`, `loadScaleOfCase`, `setLoadScales` (undo), rangos `kMinTypeScale=0.001`/`kMaxTypeScale=1000` (coinciden con REA), fórmulas de dibujo en `nodalLoadDrawLengthM`, `barLoadDrawSizeM`, `shellLoadDrawSizeM`, `panelLoadDrawSizeM`; `LoadScalesDialog` (aviso de rango con texto supuesto), «Ver todas» en `NodalLoadDialogs.cpp:632`, `Viewport3D::m_loadViewCase` (nullopt = ver todas), colores por orden de creación solo en paños (`panelLoadColourSlot`) y láminas.
Huecos detectados [V en el clon]:
1. `DisplayOptions::fillLoads` («Dibujar las cargas con relleno», por defecto true) se guarda y se edita en el panel pero **ningún código de dibujo lo lee** (`grep fillLoads` solo en DisplayOptionsPanel y display_options): las bandas de barras/láminas/paños siempre se dibujan con relleno.
2. Las cargas de **nudos y barras** se pintan siempre en amarillo (`kLoadYellow`) con independencia de la hipótesis; los paños/superficiales/láminas usan la paleta por hipótesis. Hay que decidir con la medición de la otra sesión si el original usa la misma paleta en nudos y barras.
3. La tabla Hipótesis|Escala del clon debe validar 0.001-1000 también por fila (REA); comprobar `LoadScalesDialog::accept`.

## 4. Especificación para backend (src/core) y frontend (GUI)

### 4.1 Backend [V salvo [N]]
1. `LoadScales` se mantiene: modo {Fixed=0, Proportional=1}, `fixedSizeM` (defecto 0.5, > 0.0001, finito), `perType[9]` en el orden Uniforme, Puntual, Faja, Triangular, Trapecial, Momento puntual, Superficial, Temperatura, Retracción (defectos 1,1,1,1,1,1,1,1,100), `perCase` (defecto 1.0, el valor 1.0 no se guarda). Validación común: **cada valor de tipo y de hipótesis en [0.001, 1000.0]** (REA); `fixedSizeM` > 0.0001 [N rango máximo].
2. Función única de resolución de tamaño (equivalente a FUN_1802dd290): `LoadDrawSize resolveLoadDrawSize(const LoadScales&, LoadScaleType, int loadCaseId)` que devuelve `{Fixed, maxM}` o `{Proportional, factor = perType x perCase}`; los cuatro módulos de cargas (nudo, barra, lámina, paño) deben usarla en lugar de duplicar la lógica (0.1 m por unidad x factor en proporcional; el valor máximo de la carga = `fixedSizeM` en fijo).
3. Persistencia con la obra (JSON «scales» ya en `nodal_loads`): el original guarda las escalas en la obra (`datos_obra->escalas_cargas`) [V]; la hipótesis vista vive en la configuración de la obra (`configuracion->hipotesis_vista`) [V]. Guardar en la obra `viewedCaseId` (por defecto Peso propio) y, **si se decide** [N] que «Ver todas» es de obra, `viewAllCases` (por defecto false, medido).
4. Regla de caída: al borrar una hipótesis (o dejar de ser válida por cambio de normas) la hipótesis vista pasa a Peso propio (FUN_1801941f0), y su escala se elimina de `perCase` (ya hecho en `load_case_edit`).
5. Paleta de hipótesis: función central `loadCaseColourSlot(cases, id)` (hoy `panelLoadColourSlot`) movida a `load_case.h` y reutilizada por nudos, barras, láminas y paños; ranuras 0..4 medidas; 5 y siguientes [N] (el clon cicla desde 1: suposición).
6. Resultados (T-P8-06, no ahora): reservar `ResultScales {double deformedScale; double lawsScale; double reactionsScale}` solo cuando se midan [N]; hoy no hay ninguna cadena ni campo conocido.

### 4.2 Frontend [V salvo [N]]
1. Ribbon Carga > Visualización: botones «Hipótesis vista» (#65380, abre cuadro con combo «Hipótesis» + casilla «Ver todas», Aceptar/Cancelar, termina el modo activo) y «Escalas» (#64337, cuadro modal, termina el modo); tooltips #64651 y #64652; barra de estado «Visualización - Hipótesis vista. …» / «Visualización - Escalas. …» (ya hechos).
2. Cuadro Escalas: radios «Proporcionales a un valor fijo» (campo «Tamaño máximo para representación» m, 0.500) y «Proporcionales al valor de la carga» (tabla Hipótesis|Escala, celda editable 3 decimales, no ordenable, + grupo en negrita «Escalas de cargas por tipos» con 9 campos 3 decimales); solo se muestra el bloque del radio activo; los campos de tipo con tooltip «Debe estar entre 0.001  y 1000.000 .» (doble espacio); Aceptar en modo fijo no guarda la tabla (medido). Falta: aplicar el rango 0.001-1000 también a las celdas de la tabla de hipótesis (REA).
3. Dibujo: cablear `DisplayOptions::fillLoads` (false = solo contorno/líneas sin relleno translúcido en barras, láminas y paños) [comportamiento sin relleno del original: N, hay que verlo en el original desmarcando la casilla]; pintar nudos y barras con la paleta por hipótesis cuando la otra sesión lo confirme.
4. Filtrado: con «Ver todas» desmarcada se dibujan solo las cargas de la hipótesis vista (y el peso propio solo con la vista en Peso propio); marcada, todas con su color sin atenuar y apiladas por orden de creación (medido en láminas y paños; nudos/barras: N).
5. Tras borrar la hipótesis vista el combo/vista vuelve a Peso propio.

## 5. Pendiente de medir en el original (para la otra sesión o la siguiente)
- Colores de nudos/barras por hipótesis, 5.ª+ hipótesis, naturalezas distintas de CM y V (Q, T, R, S, N, H, A).
- Comportamiento visual con «Dibujar las cargas con relleno» desmarcada.
- Dónde se guarda «Ver todas» (cerrar/guardar/reabrir la obra) y si la hipótesis vista se conserva en el `.ed3`.
- Rango y aviso del campo «Tamaño máximo para representación» y texto del aviso de rango en los campos de tipo.
- Escala/amplificación de la deformada, diagramas y reacciones: requiere obra calculada (T-P8).
