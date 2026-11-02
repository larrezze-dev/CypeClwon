# Geometría > Planos (rejilla y niveles): hallazgos de código (REA, ed3dlib.dll) para T-P5-01

Etiquetas: [código] = leído en pseudocódigo/bytes de `ed3dlib.dll` (imagen base 0x180000000, nombres de función strippeados: `FUN_*`);
[cadena] = texto de tabla de mensajes; [no resuelto] = no averiguado (ver el final). Nada de lo no marcado se ha deducido.

## 0. Hallazgo de método (importante)
- **No existe ningún RT_DIALOG** en ninguna de las 673 DLL ni en los EXE de la carpeta (escaneo propio de los árboles de recursos PE; `inspect_pe_resources` de ed3dlib.dll
  devuelve 0 recursos). Los diálogos NO tienen plantilla de recursos con tamaños en unidades de diálogo: se construyen en código con el framework de paneles
  (PANELES.DLL / COMPONEN.DLL / CYPEUD.DLL: `pnpanel_crea_en_vertical_edits_4`, `pnpanel_crea_en_horizontal_3`, `grupanel_panel_con_titulo_u`, `pnpanel_ed_ulong_u`,
  `pnpanel_nousar_ed_double_ud`, `pnpanel_ed_texto_u`, `radiogru_*`, `dlgokcan_lanzaDialogoAceptado_u`, `dlgavis_preguntar_no_por_defecto_u`). Los tamaños dependen del auto-layout;
  por tanto las coordenadas en píxeles solo se pueden medir en la app (ver §8).
- Los textos no se piden por id en el código (no hay inmediatos 65025 etc.): cada mensaje es una función generadora (`FUN_*`) que rellena una estructura
  `{fichero.msg, índice, 7 idiomas...}`; el código la pasa a `nousar_msglocal_original_u(FUN_x)`. El fichero `.msg` agrupa los ids de un panel. Ver §1.
- Fichero fuente de la lógica: `ed3pnlingridus.c` (panel tabla de líneas), `ed3gridplanun.c` (generador uniforme, 2 direcciones), `ed3lingridun.c` (una dirección), `ed3pnnivel.c` (panel niveles).

## 1. Ficheros de mensajes y ids por panel [código+cadena]
| Fichero .msg | ids (es) |
|---|---|
| ed3pnlingridus.msg (tabla de líneas de rejilla) | 25889 "Coordenada"; 65019 "Etiqueta"; 25890 "Posición de la etiqueta"; 65024 "Nueva línea"; 65025 "Generación de líneas de rejilla"; 65026 "Se borrarán las líneas de la rejilla previamente definida. ¿Desea continuar?"; 25891 "La etiqueta '%s' está repetida."; 25892 "Las líneas '%s' y '%s' están situadas en la misma coordenada."; 387397 "Ajustar los elementos del modelo por cambios en la posición de las líneas de la rejilla" |
| ed3gridplanun.msg (generador, 2 grupos) | 25887 "Líneas en la dirección X"; 25888 "Líneas en la dirección Y"; 64510, 64511 |
| ed3lingridun.msg (un grupo) | 64509 "Número de líneas"; 64510 "Separación entre líneas"; 64511 "Etiqueta inicial"; 64512 "Posición de la etiqueta" |
| ed3pnnivel.msg (niveles) | 33058 "Prefijo"; 65033 "Número de niveles"; 65034 "Cota inicial"; 49336 "Separación"; 47673 "Cota absoluta"; 65031 "Generar niveles"; 65038 "Nuevo nivel"; 65036 "Se borrarán los niveles previamente definidos. ¿Desea continuar?"; 65028 "Debe definir al menos dos niveles."; 65030 "Los niveles '%s' y '%s' tienen asignada la misma cota."; 65037 "Proceso constructivo para elementos no asociados a niveles" |
| ed3dlib.msg | 131655 "Nivel" (prefijo por defecto del generador de niveles) |
Otros (fuera de estos .msg; sin comprobar su uso): 65029 "La referencia '%s' está repetida."; 25876 "Etiqueta de la línea de la rejilla"; 25877 "Líneas de la rejilla"; 25913 "Rejilla"; 25927 "Niveles";
25886 "Opciones de visualización"; 25883 "Intervalo"; 25932 "No se ha definido ningún nivel"; 25935/25938 "No hay más niveles/líneas de rejilla definidas en esta dirección"; 25936/25937 "Subir/Bajar un nivel"; 25939/25940 "Avanzar/Retroceder una línea";
25941 "Alzados"; 25918/25919 "Vista 2D de un alzado paralelo a las líneas de rejilla X/Y"; 25929 "Alzado %s"; 64255 "Mostrar la rejilla". Título del botón "Planos": id 74892 (existe en ed3dlib, cfgplano, entomenu, entornop, panento, seccviga).
Ayudas: 64844 niveles = "planos horizontales paralelos al plano xy global situados en una cota absoluta dada"; 64845 "La asignación de los elementos a los niveles es automática. Los elementos situados entre dos niveles se asignan al nivel inferior"; 64846 "Los pilares deben comenzar y terminar en dos niveles diferentes… No es necesario definir niveles para introducir vigas"; 64847 rejilla = "líneas de referencia etiquetadas que sirven como soporte para la introducción de elementos".

## 2. Diálogo «Generación de líneas de rejilla» (id 65025) [código]
Se abre desde el botón "generacion" del panel de la tabla de líneas (`FUN_1807ced20`, `boton_crea_u("generacion", msg 65025...)`) con `dlgokcan_lanzaDialogoAceptado_u("grid_uniforme", ...)`: **Aceptar/Cancelar** (aceptado = 1). Título 65025.
Disposición (`FUN_1807c7a80`): `pnpanel_crea_en_horizontal_3`: [grupo con título "Líneas en la dirección X" (25887)] | línea vertical | [grupo con título "Líneas en la dirección Y" (25888)], margen 0x14 en cada grupo.
Cada grupo (`FUN_1807c88d0`, panel "lineas_grid_un", edits en vertical, 4 filas):
| Fila | Control | Nombre interno | Tipo / límites [código] |
|---|---|---|---|
| 1 | "Número de líneas" (64509) | `num_lineas` | entero sin signo, **mín 2, máx 1000** (`pnpanel_ed_ulong_u(...,2,1000,0)`) |
| 2 | "Separación entre líneas" (64510) | `espaciamiento` | double con unidades de longitud (`udusada "longitud"`, edición/texto/dígitos de CYPEUD) |
| 3 | "Etiqueta inicial" (64511) | (texto) | `pnpanel_ed_texto_u`, parámetro 4 (probable longitud máx. de 4 caracteres: **no confirmado**) |
| 4 | "Posición de la etiqueta" (64512) | `etiqueta` | popup `radiogru_ed_popup_u` con **2 opciones** (valores 0 y 1) del enum `ed3lingridus_posicion_etiqueta_t`, cada una con icono (`FUN_180613020` / `FUN_180612a60`); texto de las opciones: [no resuelto] |
Invariantes (`FUN_1807ca0c0`, aserciones): num_lineas ≥ 2; referencia base (etiqueta inicial) no nula.
Al aceptar (`FUN_1807cf120`): si ya hay líneas en X o en Y, pregunta 65026 (`dlgavis_preguntar_no_por_defecto_u` = el botón por defecto es **No**); si Sí o no había líneas, regenera **ambos** ejes (se borran las anteriores).
Generación (`FUN_1807c8620` por dirección): para i = 0..n-1: coordenada = i·separación (**parte de 0**, no hay cota inicial en el diálogo); etiqueta: i=0 -> etiqueta inicial; las demás =
`cyperef_nueva_referencia_u(anterior)` (autoincremento de referencia tipo A→B / 1→2; reglas exactas de `cyperef_*`: [no resuelto]); posición de etiqueta = valor del combo para todas.
Estructura de datos por dirección (`ed3lingridun`): +0 uint n; +4 double separación; +0xC puntero a etiqueta base; +0x14 enum posición. Valores por defecto de n / separación / etiqueta: [no resuelto] (no hallado el constructor).

## 3. Panel tabla de líneas de rejilla (`ed3pnlingridus`) [código+cadena]
- Datos por línea: coordenada (double), etiqueta (cadena), posición de etiqueta (enum `ed3lingridus_posicion_etiqueta_t`). Objeto `ed3gridplan` con dos arrays (X, Y) ("lineas_grid_x", "lineas_grid_y"; `datos_panel` con `navegacion` para listas ordenadas: `lista_navegacion_lineas_grid_usar_ordenadas`).
- Columnas (por los ids del .msg): Etiqueta (65019), Coordenada (25889), Posición de la etiqueta (25890). Botón "Nueva línea" (65024) añade una fila; el botón "Generación" (65025) abre el §2. El orden de columnas, anchuras y si hay una tabla por dirección o una pestaña: [no resuelto]. La opción 387397 (“Ajustar los elementos del modelo por cambios en la posición de las líneas de la rejilla”) es un check del panel (texto de ayuda de ese check): al mover una línea se desplazan los elementos asociados.
- Validación [cadena+código de id]: etiqueta repetida -> 25891; dos líneas en la misma coordenada -> 25892. Cuándo se dispara (al editar la celda o al Aceptar) y si bloquea el cierre: [no resuelto].
- Salida a otros módulos: BIMJSUNION.DLL exporta `bjsumodgeo_v1_append_linea_grid_x/y` (la rejilla se serializa a BIM JSON como líneas X/Y).

## 4. Diálogo «Generar niveles» (id 65031) [código: `FUN_180795b90`]
`dlgokcan_lanzaDialogoAceptado_u("generacion_niveles", ...)`, Aceptar/Cancelar. Campos (con valores por defecto del código):
| Campo | Id | Defecto | Límite |
|---|---|---|---|
| Prefijo | 33058 | "Nivel" (id 131655, localizado) | cadena |
| Número de niveles | 65033 | **2** | aserción `num_niveles >= 2` |
| Cota inicial | 65034 | **0,0** | double (unidades de longitud) |
| Separación | 49336 | **3,0** | double (sin validación vista) |
(El orden vertical Prefijo/Número/Cota inicial/Separación sigue el orden de los ids del .msg; el orden visual exacto: [no resuelto].)
Al aceptar: si la lista ya tiene filas pregunta 65036 (botón por defecto **No**). Si procede: borra todas las filas y crea n niveles, i = 0..n-1: **nombre = "%s %lu" (prefijo, i)**, p. ej. "Nivel 0", "Nivel 1"…
(el índice empieza en 0); **cota = cota inicial + i·separación**. Se insertan al final (tipo `ed3nivel_t`), se reorganizan columnas y se refresca (`FUN_180796270`).

## 5. Panel de niveles (`ed3pnnivel`) [código+cadena]
- Lista de objetos `ed3nivel_t` (lista con "Nuevo nivel" 65038 y "Generar niveles" 65031). Columnas por .msg: nombre/referencia (65029 habla de "referencia" repetida) y "Cota absoluta" (47673). Opción "Proceso constructivo para elementos no asociados a niveles" (65037): control del panel (tipo y ubicación: [no resuelto]).
- Validación [cadena]: <2 niveles -> 65028 "Debe definir al menos dos niveles."; dos niveles con la misma cota -> 65030; referencia repetida -> 65029.
- Orden: existe `niveles_ordenados` (identificador del modelo) y la asignación de un elemento al nivel inferior más cercano (64845). Si la lista se reordena por cota al aceptar: [no resuelto].
- Otros símbolos que sirven de pista (aserciones): `Obra_niveles`, `ed3obra_hay_niveles`, `intervalo_niveles`, `ids_niveles_ini/fin_intervalos`, `nombres_niveles`, `referencias_niveles`, `regen_niveles_editados`, `con_edicion_niveles`.

## 6. Agrupar / desagrupar planos (cadenas; sin código leído)
Botones "Agrupar planos" (64084) y "Desagrupar planos" (64085). Ayuda 64029-64035: solo planos perpendiculares a un eje global; se seleccionan pulsando una línea de referencia del plano (en 3D o 2D); el plano seleccionado dibuja sus líneas en **rojo**; pulsar de nuevo lo deselecciona; botón derecho -> pregunta 65150 "¿Desea agrupar los planos seleccionados?" / 65151 desagrupar; el programa da **el mismo número a todos los planos de un grupo y lo muestra en pantalla**; se puede elegir un grupo existente para añadir planos (65152); 65153 "Seleccione los planos que desea agrupar."; error 65155 (disposición/nº de puntos fijos incompatible); 65148 (planos no paralelos al acotar). 64742: los elementos y sus posiciones permanecen idénticos en todos los planos del grupo.
Generación de planos (menú Herramientas>Generar): 63971 "Generación de planos", 63972 "Nº de planos a generar", 63981 "Separación entre planos" (positiva = sentido positivo del eje global perpendicular), 63973 "Agrupar planos generados", 63980/63982 ayudas; 64775/64776 ayuda (en 3D, seleccionar dos líneas de referencia del plano a copiar).

## 7. Dibujo en el visor 3D y guardado en el .ed3
No investigado en esta pasada (no se leyó ningún código de dibujo ni de serialización). Pistas: 64255 "Mostrar la rejilla"; 25876 etiqueta de línea; los símbolos `info_nivel_linea`, `coord_nivel`, `grupos_niveles`.
Dónde buscar: (a) xrefs a las funciones `FUN_1807ca620` (crea una línea de rejilla con coordenada, etiqueta, posición y datos de obra) y `FUN_18078fa80` (crea un nivel); (b) clase `ed3gridplan`, `ed3nivel` en los *serializadores* de obra (`bimjson`/`.ed3`: pendiente).

## 8. No resuelto: qué medir en la app o seguir en código
1. Coordenadas/tamaños en píxeles de los diálogos (§2, §4) y de la pestaña/pantalla principal «Planos»: captura con el original (pedir OK al usuario).
2. Estructura del diálogo principal: ¿pestañas «Rejilla»/«Niveles» (25913/25927) y dónde están el check de 387397 y el de 65037?; columnas/anchos de ambas tablas; botones (Nueva línea/Nuevo nivel, borrar, generar).
3. Valores por defecto del generador de rejilla (n, separación, etiqueta inicial, posición) y límites de la separación (¿>0?); texto/iconos de las 2 posiciones de etiqueta.
4. Regla `cyperef_nueva_referencia_u` (A→Z→AA, 1→2→…, con sufijos), longitud máxima de etiqueta.
5. Cuándo se validan duplicados (en edición o al aceptar) y si la tabla se reordena por coordenada/cota; nombres por defecto de "Nueva línea"/"Nuevo nivel".
6. Dibujo 3D (color, estilo, posición de etiquetas, botones de navegación de líneas/niveles 25935-25940) y formato en el .ed3.
7. Agrupar/desagrupar: comprobación empírica de la numeración mostrada y del color de selección.
