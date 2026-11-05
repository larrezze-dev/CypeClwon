# Grupo Importar (pestaña Geometría) - hallazgos REA estáticos

Fuente: REA/Ghidra sobre `ed3dlib.dll`, textos de `imporstr.dll`/`dxflib.dll` en `data/strings_merged_latin.tsv`. No se lanzó ninguna aplicación. Etiquetas: [REA] código leído; [CAD] cadena; [PROB]; [NV].

## 1. Fichero de texto (#392582, ayuda #64153-64159, #399481)
Formato [REA+CAD]. La ayuda se genera en `FUN_180076c30` (ed3ayuda.c) y confirma las palabras clave: **NUDOS / JOINTS, BARRAS / FRAMES, LAMINAS / SHELLS, PAÑOS / PANELS** (UTF-16 en 0x180a366e0..; cada sección admite el nombre es-ES o el inglés; `FUN_1800a4ed0` devuelve el par de nombres según idioma).
- Estructura:
  1. Primera línea de sección: `NUDOS` (o `JOINTS`). Luego **una línea por nudo**: `<nº nudo> X=<valor> Y=<valor> Z=<valor>` (nº, espacio, letra X, «=», valor; idem Y, Z) (#64155).
  2. `BARRAS` (o `FRAMES`): una línea por barra: `<nº barra> <nudo inicial> <nudo final>` separados por un espacio (#64156).
  3. `LAMINAS` (o `SHELLS`): una línea por lámina: `<nº lámina> <nudo> <nudo> ...` (n.º de lámina y los nudos que la componen separados por espacios) (#64159).
  4. `PAÑOS` (o `PANELS`): paños de carga, igual que láminas: `<nº paño> <nudos...>` (#399481).
- Ejemplo literal de la ayuda (pórtico a dos aguas, `FUN_180076c30`):
```
NUDOS
1 X=0 Y=0 Z=0
2 X=0 Y=0 Z=3
3 X=5 Y=0 Z=5
4 X=10 Y=0 Z=3
5 X=10 Y=0 Z=0

BARRAS
1 1 2
2 2 3
3 3 4
4 4 5
```
- Unidades: metros (las coordenadas de la obra; la ayuda no menciona otras) [PROB]; separador decimal, sensibilidad a mayúsculas/minúsculas y tolerancia a líneas en blanco/espacios múltiples [NV].
- Validaciones del importador (imporstr.dll, texto es-ES [CAD]; ids 106854-106890, 399535-399541):
  - Nudos: n.º no positivo (#106854) / > 999999 (#106855) / ya existente (#106856); coordenadas iguales a un nudo existente (#106857); «Sintaxis irreconocible» (#106859); «numeración de usuario negativa» (#106860).
  - Barras: n.º no positivo (#106862) / > 999999 (#106863); nudo de barra no positivo (#106864) / > 999999 (#106865); n.º repetido (#106866); nudo inexistente (#106867 «barra: %lu nudo: %lu»); conectividad ya existente (#106868 «Barra con conectividades ya existentes (%lu = %lu)»); sintaxis (#106869); nudo inicial = final (#106861).
  - Láminas: sintaxis (#106884), n.º no positivo (#106885) / > 999999 (#106886), nudos repetidos (#106887), n.º repetido (#106888), nudo inexistente (#106889 «lámina: %lu, nudo: %lu»), conectividad ya existente (#106890).
  - Paños: #399536-399541 (mismas reglas).
  - Globales: «Fichero vacío» (#106870), «No se encontraron todos los sectores de datos» (#106873), «No se ha leído ninguna barra ni ninguna lámina» (#106877), «Leídas %lu líneas.» (#106876), «Total: %lu nudos, %lu barras» (#106879), «Lectura finalizada correctamente. Desea importar los datos leídos...» (#106880), «Proceso finalizado con error» (#106881); fases «Lectura sector nudos/barras/láminas/paños de carga» (#106871/#106872/#106883/#399535), «Lectura datos fichero texto» #106874, «Cargando fichero» #106875.
  - Máximo de número = 999999. #64278: importar en una obra que no tiene ningún nudo con vinculación exterior no es posible (esto es la importación de «estructura 3D integrada» de CYPECAD, no el fichero de texto).
- Funciones de `ed3dlib.dll` que reciben los datos: `ed3import_append_nudo` (0x18032d090), `ed3import_se_anyade_barra` (0x18032d330), `ed3import_empotext_crea|crea_empotrado|destruye` (0x18032d190/230/2c0) y `ed3import_vincextnu_crea|destruye` (0x18032d5a0/710) (crear nudo/barra/empotramiento exterior/vinculación exterior de nudo) [REA nombres; cuerpos no leídos]. El lector de texto está en `imporstr.dll` (nombres `impsap_*` y `impdxf_*`, p. ej. `impdxf_se_lee_estructura_dxf`, `impsap_num_laminas`, `impsap_get_coords_lamina`; importados por ed3dlib, ver imports DelayLoad 0x180941887/0x18094193c/0x180941948) [REA].
- Una importación anterior ASCII CYPE (otro formato): #106791 «...ficheros 'NUDOS.asc', 'REAC.asc' y 'BARRAS.asc'», #106792 «No se pueden importar las reacciones.» (flujo heredado; probablemente no accesible desde el ribbon) [PROB].

## 2. DXF o DWG (#392583; ayuda #64736, #64767-64769)
- Función: «generar la geometría de la obra automáticamente a partir de un fichero DXF o DWG. Puede desactivar las capas de los elementos que no se quieren contemplar. **Es imprescindible que la unidad de dibujo sea el metro**» (#64736). Flujo: diálogo de búsqueda de fichero (`dxflib`: «Selección de ficheros DXF-DWG» #60976 / «...a leer» #61041; filtros «Ficheros DXF» #61042 y «Ficheros DWG» #61043) -> «Leyendo DXF-DWG...» (#60977) -> ventana «Importar geometría» (#65114; ayuda #64767) donde se **visualizan y seleccionan elementos por capas, tipos de línea, color o tipo de entidad** (#64769); botones/opciones: «Importar en la posición definida en el fichero» (#65116) o «Seleccionar el punto de referencia para la importación» (#65117); casilla «Importar polígonos como paños de carga» (#399490). «Visibilidad de capas» #64295; columnas «Capa/Capas» #372886/#372887, «Nombre de la capa» #64495.
- Recomendaciones de la ayuda (#64768-64769): las líneas que representan barras deben cortarse realmente (si no, se crean dos nudos muy próximos sin conexión); texto/símbolos del dibujo pueden importarse como barras si se seleccionan; lo ideal es una sola capa exclusiva de barras.
- Entidades admitidas: línea/polilínea/3DFACE: **[NV]**. La ayuda habla de «líneas que representan las barras» y de «polígonos» (importables como paños de carga #399490, y `impsap_get_coords_lamina` sugiere láminas); no consta cadena «3DFACE»/«LWPOLYLINE» en `ed3dlib.dll` (búsqueda negativa). Habría que analizar `dxflib.dll`/`imporstr.dll` (no abiertas en esta sesión).
- Mensajes de error/avisos [CAD]: #65115 «Algunas barras no se han podido importar. Revise la geometría... barras de longitud excesiva (más de mil metros), barras solapadas entre sí, etc.»; #64993 «Las capas seleccionadas no tienen elementos asociados.»; #60948 «No se encontraron <entidades> en el fichero. El fichero es erróneo.»; #61039 «Este dibujo no tiene capas definidas.»; #61019 «Es necesario haber seleccionado, al menos, una capa.»; #60974/#60975 «El dibujo es muy grande/pequeño. Tiene que reducir la escala/el divisor de escala»; #247130 «El número de capas de este dibujo es superior al máximo permitido (%d)» (máx. [NV]); #71697 «No existe el fichero %s»; #60991 «No existe ningún fichero en la lista.»; #139119 «No puede haber nombres de capa repetidos.»; #138036 «la plantilla debe tener como unidad de dibujo el metro» (opciones Escalado X/Y); #138037 situar cerca del origen (Desplazamiento X/Y); transformaciones #61045-61054 (desplazamiento X/Y, escalado X/Y, ángulo de rotación, origen).
- Tolerancias de importación (distancia para unir nudos, longitud máxima 1000 m salvo la citada): solo consta el máximo de 1000 m (#65115); la tolerancia de fusión de nudos [NV] (ver Reparar en `herramientas.md`).
- Funciones: `ed3obra_append_barras_segmentos_dxf_para_test` (0x180444e00, función de test que añade barras desde segmentos), `impdxf_se_lee_estructura_dxf` (imporstr.dll), `ubcpdxf_append_capas_geometria[_para_listado]` (undxf.dll) [REA nombres].

## 3. Ampliación REA (sesión de análisis de imporstr.dll / dxflib.dll / ed3dlib.dll)
Solo análisis estático (Ghidra vía REA); no se lanzó ninguna aplicación. Direcciones = imagen base 0x180000000 de cada DLL. Etiquetas: [REA] pseudocódigo leído; [CAD] cadena; [NV] no verificado; [INF] inferencia señalada.

### 3.1 Lector DWG/DXF: librería de terceros ODA [REA/CAD]
- `cydwgdxf.dll` (33,9 MB, carpeta de CYPE 3D) es el lector/escritor DWG/DXF. Cadenas: «Open Design Alliance», «Teigha(R)», «DWGdirect», «OdaTeigha», `OdDwgInController`, **«ODA SDK 25.8.0.0»**, «Copyright 2002-2024, Open Design Alliance». `dxflib.dll` lo carga por nombre (cadena `cydwgdxf.dll` en 0x180120aa0) y expone `dxfdatos_lee_formato_autocad[_basico|_ex|_sin_progreso]_u`, `dxf_lee_formato_autocad[_ex|_sin_eliminar_capas_vacias]` [REA nombres].
- Es decir: no hay conversión externa (no se llama a AutoCAD/ODAFileConverter); DWG y DXF se leen con el mismo motor ODA. Versiones: la tabla de cadenas de versión de `cydwgdxf.dll` contiene AC1.40/AC1.50, AC1001-AC1015, AC1018, AC1021, AC1024, **AC1027, AC1032** (y AC1500). Es la tabla estándar del SDK; que un dwg AC1032 (AutoCAD 2018-2025) se abra está respaldado por la presencia de la cadena, pero no se ha ejecutado [CAD; lectura efectiva NV]. Mensaje `eUnsupportedEarlyDwgVersion` presente (versiones antiguas no soportadas por el SDK) [CAD].
- En `dxflib` hay una pila de bloques activos (`pila_de_bloques_activos`, `__dxfdatos_datos_bloq_%zu`, `cpladxf_inserta_bloque_con_plano_definido`): los INSERT se resuelven al leer el fichero; el filtro de importación nunca ve un tipo «INSERT» (ver 3.2) [REA/CAD, expansión exacta de bloques anidados/escala/rotación NV].
- Filtros del selector de fichero (`dxflib`): «Ficheros DXF» #61042 y «Ficheros DWG» #61043 (patrón `*.dwg` en 0x1800fce08; `*.dxf` análogo), título «Selección de ficheros DXF-DWG a leer» #61041 / «Selección de ficheros DXF-DWG» #60976; barra de progreso «Leyendo DXF-DWG...» #60977; mensaje «Debe introducir, al menos, un fichero DXF o DWG» #60978 / «Debe seleccionar, al menos, un fichero 'DXF' o 'DWG'» #61038. Nota: la cadena `*.dxf;*.dwg;*.dwf;*.jpeg;*.jpg;*.bmp;*.wmf;*.emf;*.pcx` (0x18010bc60) pertenece a la gestión de *plantillas*, no a la importación de geometría [CAD].

### 3.2 Entidades admitidas y conversión [REA]
Ruta: `impdxf_se_lee_estructura_dxf` (imporstr 0x180004ab0) -> `dxfselec_nousar_selecciona_entidades_importacion` (dxflib 0x18009b460) -> `FUN_18009e930` (lanza el diálogo `seleccion_dxf` y, si se acepta, `FUN_18009cde0`) -> por capa marcada `FUN_18009d470/18009d570` (conversión por tipo de entidad). El enumerado interno del tipo (`entidad+0x26`) es:

| tipo | tratamiento en importación | resultado |
|---|---|---|
| 6 (línea) | `FUN_18009e0c0` | 1 segmento (barra) |
| 7 (polilínea) | segmentos consecutivos vértice i -> i+1; si el indicador de cerrada (`+0x18`) = 1 se añade el segmento último -> primero | N-1 (o N) segmentos (barras) |
| 0 (3DFACE, `cara_3d`, 4 vértices) | con flag de «caras como líneas» = 1: 4 segmentos (v0-v1, v1-v3, v3-v2, v2-v0, es decir el contorno de las 4 aristas); con flag != 1 (caso de la importación de CYPE 3D, `ed3rcimport1.c`: se llama con 0): un **polígono** (se eliminan vértices consecutivos coincidentes, dist <= 1e-5; si quedan < 3 puntos se descarta) | polígono de 3 ó 4 puntos |
| 0x12 (malla de polícaras / PolyFaceMesh, `dxfepolyfacemesh.c`) | cada cara de la malla -> un polígono (`FUN_180069c60`) | N polígonos |
| 0x13 (malla poligonal / PolygonMesh M×N, `dxfepoligonmesh.c`) | se genera la rejilla 3D (`cypemesh_genera_grid3d`) y cada celda -> polígono (`FUN_1800693e0`) | N polígonos |
| 1-5, 8-0x11 | **no se importan** (en `dxfdatos_dame_coordenadas_entidades_linea_y_polylinea` activan el indicador «hay entidades que no son líneas ni polilíneas»); su nombre exacto (CIRCLE, ARC, TEXT, POINT, SPLINE, ELLIPSE, HATCH, 3DSOLID...) no se ha podido ligar a cada número [NV] | ignoradas |
| otro | `error_interno_default_error` | - |

- No consta ninguna conversión de CIRCLE/ARC/TEXT/INSERT en el camino de importación: el texto y los símbolos de la ayuda (#64769) «pueden importarse como barras» porque las letras suelen dibujarse con líneas/polilíneas. LWPOLYLINE: el reconocimiento lo hace ODA y la entidad llega al tipo 7; no hay prueba textual de que LWPOLYLINE se mapee a 7 (no consta cadena), por lo que queda [NV] aunque es lo esperable [INF].
- Filtro de longitud nula: `FUN_18009e0c0` solo añade un segmento si `mt_distancia_3D(p0,p1) > 1e-5` (en unidades del dibujo = metros); `FUN_18009e030` aplica el mismo umbral a los vértices consecutivos de un polígono [REA, 0x18009e0c0 / 0x18009e030].
- Unidades: no se aplica ningún escalado ni desplazamiento en este camino; se leen las coordenadas X, Y, Z tal cual (3D, incluida Z). Por eso la ayuda exige «unidad de dibujo = metro» (#64736) [REA por ausencia de transformación; coincide con [CAD]]. Las «Transformaciones» (#60979: desplazamiento/escala/rotación #61045-61049) pertenecen a las *plantillas* DXF/DWG, no a esta importación.
- Capas: el resultado se devuelve agrupado por capa: `nombres_capas`, `colores_capas` (color AutoCAD ajustado a la paleta, `idcolor_crea_color_ajustando_paleta_autocad`), `inds_capa_barras`, `barras_segmentos`, `inds_capa_laminas_ef`, `poligonos_laminas_ef`, `inds_capa_panyos_carga`, `poligonos_panyos_carga` (estructura `datos_importacion_dxf`, ed3dlib 0x180c143e8-0x180c145e0). Solo se procesan las capas con la casilla de la primera columna marcada (`fila.tipo == 1`) [REA 0x18009cde0]. Una capa sin elementos asociados -> aviso #64993 [CAD].
- Destino de los polígonos [REA 0x18055ee00 `FUN_18055ee00`]: cada polígono se envía a **láminas de elementos finitos** (`laminas_ef`) por defecto; si su capa tiene marcada la columna booleana «Importar polígonos como paños de carga» (#399490), va a **paños de carga** (`panyos_carga`). Los segmentos van siempre a barras. Mapa final: línea/polilínea -> barras; 3DFACE/malla -> lámina EF o paño de carga según la casilla de la capa; nudos se crean en los extremos y vértices (no hay entidad «nudo»: POINT no se importa).
- Partición de segmentos: `FUN_1802f3700` (ed3gener.c) trabaja con `piezas_con_puntos_intermedios`/`inds_capa_segmentos`: los segmentos se parten en los puntos intermedios (nudos) que caen sobre ellos, mostrando una barra de progreso con comentarios («Procesando barra»); la tolerancia exacta de coincidencia de nudos [NV] (no leída; está en `FUN_18030a5f0`/`FUN_1803053a0`, sin analizar). Límite de 1000 m y barras solapadas: aviso #65115 mostrado cuando el indicador de salida (`FUN_18055faf0` out param 6) vale 1 [REA+CAD; el cálculo del indicador está en `FUN_1802e8b60`, no desglosado].
- Requisito previo: la generación exige que no haya planos agrupados (`i_hay_planos_agrupados == FALSO`, ed3datob.c 0x26ff). Por eso, antes del diálogo de selección (`FUN_1800011a0`), si hay planos agrupados se pregunta con dos textos unidos: «Se van a desagrupar todos los planos que intervienen en la generación.» (#63970) + «¿Desea continuar?» (#322563) y, si se responde Sí, se desagrupan (`FUN_18021cf50`); si se responde No, se cancela la importación [REA 0x1800011a0 + CAD].

### 3.3 Diálogo de selección de entidades (importación DXF/DWG) [REA]
Descriptor: `FUN_18009f550` (dxflib 0x18009f550), diálogo aceptar/cancelar de nombre interno `seleccion_dxf` (`dlgokcan_lanzaDialogoAceptado_u`), título **«Selección de entidades»** (#60990; el rótulo «Importar geometría» #65114 es el nombre de la ventana que citan las ayudas #64767/#64769 y se usa en ed3dlib; la vinculación exacta título<->#65114 no consta en el código leído [NV]). Contenido: caja horizontal `capas_y_vistas` con dos paneles:
- **Izquierda (40 % del ancho, proporción 0,4)**: lista «edicion_capas» (`lista_nousar_crea`, 150×200 px por defecto en el descriptor: parámetros 0x96/200) con una fila por capa del dibujo:
  1. columna casilla (cabecera vacía) = capa incluida; el valor inicial lo fija el selector (`fila.tipo`: 1 = seleccionada);
  2. columna texto «Capa» (#372887; es-ES «Capa»), nombre de la capa (no puede haber nombres repetidos: #139119);
  3. columna color RGB «Color» (#183364), color AutoCAD de la capa;
  4. columnas booleanas adicionales aportadas por el llamador (una por cada elemento del array `param_9`): para la importación de geometría **una sola columna, «Importar polígonos como\npaños de carga»** (#399490, creada con `dxfselec_crea_columna_bool`, valor por defecto FALSO) [REA 0x180560990].
- **Derecha (60 %, proporción 0,6)**: vista 3D del dibujo (`pndibscr_nousar_panel_vista3D`, `FUN_18009eb30`) en un panel móvil, para ver qué hay en cada capa (las capas desmarcadas se ocultan).
- Botones: Aceptar / Cancelar (estándar `dlgokcan`). Si se cancela, no se importa nada (`FUN_18009e730` libera).
- Selección por tipo de línea, color o tipo de entidad (ayuda #64769): no se localizó en el código del descriptor una columna/filtro para ello (solo capa + color + la columna de polígonos); la ayuda se refiere a una estructura de «valores de columna» por fila (`dxfselec_destruye_valor_columna`) que aquí no se usa; queda [NV] si el original ofrece filtros adicionales (verificable solo con captura).
- Errores/avisos de selección: «Es necesario haber seleccionado, al menos, una capa.» (#61019), «Este dibujo no tiene capas definidas.» (#61039), «Las capas seleccionadas no tienen elementos asociados.» (#64993), «No se encontraron <entidades> en el fichero. El fichero es erróneo.» (#60948), máximo de capas (#247130; valor no leído) [CAD].

### 3.4 Después de aceptar: punto de inserción [REA]
Si la obra ya contiene datos (`FUN_180231730(obra) == 1`; si no, se usa el origen del fichero sin preguntar, `*tipo = 0`), se abre un diálogo **«Importar»** (#29651, `FUN_1808a0870`; panel con título a la izquierda) con un grupo de radio `modo_insercion` (tipo `i_tipo_origen_coords_t`), en vertical con panel de capas a la derecha (`radiogru_panel_vertical_con_panel_capas_derecha`):
- opción 0 (por defecto, valor 0): **«Importar en la posición definida en el fichero»** (#65116);
- opción 1: **«Seleccionar el punto de referencia para la importación»** (#65117).
Tras importar con éxito, si el indicador de aviso es 1 se muestra el aviso #65115. La importación se envuelve en deshacer/rehacer (`ebarecep_inicio_undo_redo`/`fin`) y reloj de espera [REA 0x18055faf0].
- Estructura del receptor (`ed3rcimport1.c`): campo tipo = 0 (ASCII/texto) o 1 (DXF); el mismo receptor sirve para el fichero de texto (`datos_importacion_ascii`) y DXF (`datos_importacion_dxf`) [REA].

### 3.5 Fichero de texto: flujo y gramática [REA]
- Selector (`FUN_1805601e0`, ed3dlib 0x1805601e0): panel «nombre de fichero de entrada» (`pnselfic_panel_nombre_fichero_entrada_u`) con filtro **`*.txt`** (0x180c14038), título a la izquierda **«Nombre del fichero»** (#59338), carpeta inicial = la del último fichero (`cdir_ruta_acceso_directorio_fichero_UNICODE`); el botón de aceptar se habilita con la función `FUN_18055d1a0` (comprobación de fichero válido; texto exacto del criterio [NV], probablemente existencia, error #71697 «No existe el fichero %s» [INF]). El texto del ribbon es «Fichero de texto» (#392582). Sin vista previa del contenido del fichero en el selector (solo ruta) [REA por ausencia].
- Lectura (`impsap_lee_estructura_texto`, imporstr 0x180017c60) con **ventana de progreso con registro** (`prgs_abre_progreso_u`, líneas `prgs_printf_definitiva_u`/`temporal`):
  1. «-> Lectura datos fichero texto "<ruta>"» (#106874 + ruta); «   Cargando fichero...» (#106875);
  2. «   Leídas %lu líneas.» (#106876);
  3. al detectar cada sección: «-> Lectura sector nudos» (#106871), «-> Lectura sector barras» (#106872), «-> Lectura sector láminas» (#106883), «-> Lectura sector paños de carga» (#399535) (el orden exacto de las funciones de mensajes 0x180007d50/82a0/f710/fc60 coincide con ese orden de ids [INF]);
  4. progreso temporal «   <msg> %lu...» con el n.º de línea actual;
  5. fin: «-> Fin lectura datos» (#106878), «Total: %lu nudos, %lu barras» (#106879); si no hay ni barras ni láminas: «!! Error: No se ha leído ninguna barra ni ninguna lámina.» (#106877);
  6. pregunta modal Sí/No (`dlgavis_preguntar_u`): «Lectura finalizada correctamente. Desea importar los datos leídos...» (#106880), salvo que el llamador pida importación silenciosa (parámetro `param_3 != 0`); No -> descarta; error de sintaxis -> el aviso de error «Proceso finalizado con error» (#106881) y la línea «   <msg> <n.º de línea>...» (la ventana de progreso queda abierta con el registro del error; error de la línea concreta (#106854-106890) impreso con prefijo «!!»).
  (Los números de #106874..#106881 siguen el orden de offsets de las funciones de mensaje 0x180009270, 0x1800097d0, 0x180009d30, 0x18000a290, 0x18000a7d0, 0x18000ad10, 0x18000b260, 0x18000b7a0 [INF por orden].)
- Gramática del lector (`FUN_180014ca0`, imporstr 0x180014ca0):
  - El fichero se lee con `lc_LeeFicheroTxt_u` (ANSI/ASCII por `char*`; codificación UTF-8/UTF-16 [NV]); **menos de 3 líneas** -> «Error: Fichero vacío» (#106870);
  - **Comentarios**: una línea que empieza por `:` se ignora; en cualquier línea se descarta todo desde el primer `:` (`FUN_180004d40`); líneas en blanco ignoradas;
  - La línea se separa por uno o más espacios (`cad_lista_de_subcadenas_separadas_por_n_espacios`); el primer elemento se compara **sin distinguir mayúsculas/minúsculas** (`cad_cadenas_iguales_may_o_min`) con las palabras clave: «nudos»/«joints» (el primer sector **debe** ser NUDOS), después, en cualquier orden y sin repetición inmediata, «barras»/«frames», «laminas»/«shells», «paños»/«panels» (máquina de estados 0->1->{2,3,4}; los saltos 2<->3<->4 están permitidos; no se puede volver a NUDOS) [REA 0x180016050 (nudos/joints) + 0x180015fa0/15ff0/160a0 (resto, no desensambladas una a una)];
  - Si al final el estado no es 2, 3 ó 4 -> «Error: No se encontraron todos los sectores de datos» (#106873);
  - Primer error de sintaxis o de validación aborta toda la lectura (no se acumulan errores).
  - Los datos de cada línea y sus validaciones (n.º ≤ 999999, nudo existente, coordenadas duplicadas, etc.) son los ya listados en la sección 1; separador decimal y mayúsculas de `X=`/`Y=`/`Z=` dentro de la línea de nudo [NV: `FUN_180014ba0` y los lectores de barras/láminas/paños 0x1800176e0/1800178a0/180017a80 no se desensamblaron].
- Funciones de datos expuestas: `impsap_num_barras/laminas/panyos_carga`, `impsap_get_coords_barra/lamina/panyo_cargas` (coordenadas de cada pieza), `impsap_destruir_estructura`; `impdxf_test_estructura_dxf` (versión de prueba) [REA nombres].

## Pendiente (no verificado)
- Nombre de cada número de tipo de entidad de dxflib (1-5, 8-0x11) y mapeo explícito de LWPOLYLINE/CIRCLE/ARC/TEXT/POINT; escala/rotación de INSERT.
- Tolerancia de fusión de nudos y cálculo del indicador «longitud > 1000 m / solapadas» (`FUN_1802e8b60`, `FUN_18030a5f0`, `FUN_1803053a0`, `FUN_1802f15b0`).
- Máximo de capas (#247130) y valor del límite; criterio de habilitación del selector `*.txt` (`FUN_18055d1a0`).
- Disposición geométrica exacta (tamaños, márgenes) del diálogo de selección: solo se leyeron proporciones 40/60 y 150×200 de la lista; requiere captura (con autorización de lanzamiento).
- Lectores línea a línea de barras/láminas/paños del fichero de texto (separadores decimales, mayúsculas de `X=`): `FUN_180014ba0`, `FUN_1800176e0`, `FUN_1800178a0`, `FUN_180017a80`.
- Vinculación título «Importar geometría» (#65114) con el diálogo `seleccion_dxf`.

## 4. GUI del clon (T-P5-11, sesión 2026-11-05)
Código: `src/app/ImportTool.*`, `src/app/ImportDialogs.*`; núcleo ajustado a la sección 3 (REA). Capturas: `docs/screenshots/imp_dxf_selection.png`, `imp_dxf_result.png`, `imp_text_log.png`, `imp_text_result.png`, `clone_geometria_importar_ribbon.png` (comparar con `orig_geometria_ribbon.png`: dos botones pequeños con texto apilados, «DXF o DWG» y «Fichero de texto»).
- Parser de texto ajustado a [REA 3.5]: comentario desde el primer `:`; menos de 3 líneas (todas, incluidas vacías y comentarios) = «Fichero vacío»; el primer sector debe ser NUDOS y no se vuelve a él; sin ningún sector tras NUDOS = «No se encontraron todos los sectores de datos»; solo paños = «No se ha leído ninguna barra ni ninguna lámina». Palabras clave sin distinguir mayúsculas; también Latin-1 (Á/Ñ de un fichero ANSI) [PROV]. «Leídas N líneas» cuenta todas las líneas.
- Lector DXF según [REA 3.2]: LINE -> barra; polilínea -> barras (cerrada: segmento de cierre); 3DFACE, polyface mesh (POLYLINE flag 64) y polygon mesh M x N (flag 16) -> polígonos (vértices consecutivos coincidentes <= 1e-5 se eliminan, < 3 se descarta) que van a láminas o, en las capas marcadas con «Importar polígonos como paños de carga», a paños; barras solo si longitud > 1e-5. Las polilíneas cerradas ya no generan paños (el original solo les da segmentos). Color de capa: LAYER de TABLES (código 62; negativo = capa apagada, se usa el valor absoluto); índice ACI -> RGB: 1-9 fijos, 10-249 rampa HSV, 250-255 grises (aproximación) [PROV].
- Diálogo «Selección de entidades»: tabla 40 % (casilla, Capa, Color, columna booleana «Importar polígonos como\npaños de carga», falsa por defecto) + vista previa 60 % propia (proyección ortogonal girable con el ratón; capas desmarcadas ocultas). Aceptar sin capa marcada: #61019 y no cierra. Todas las capas empiezan marcadas [PROV: el original las marca según el selector].
- «Importar» (#29651): solo si la obra ya tiene nudos; opciones #65116 (por defecto) y #65117. **Decisión**: el punto de referencia se teclea (X, Y, Z) y desplaza la geometría importada (el origen del fichero va a ese punto); el original lo designa en el viewport (ticket TK-079).
- Planos agrupados: pregunta «Se van a desagrupar todos los planos que intervienen en la generación.» + «¿Desea continuar?» antes del diálogo de capas; No cancela; Sí desagrupa y el desagrupado + la importación forman un solo paso de deshacer («Importar geometría»).
- DWG: error «Formato no soportado...» inmediato (sin ODA). Fichero de texto: ventana «Lectura datos fichero texto» con el registro (#106874-#106879) y la pregunta #106880 (Sí/No); con error, líneas `!! <mensaje>   Línea N` y «Proceso finalizado con error» (#106881), un solo botón.
- Paños de carga: el modelo no los tiene; se ignoran con un aviso y los nudos que solo les pertenecen no se crean.
