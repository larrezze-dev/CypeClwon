# Cargas en barras (T-P7-03): Carga > Barras

Sesión RE 2026-11-09, solo análisis estático (sin lanzar el original). Leyenda: [REA] = binario `ed3dlib.dll` abierto con el MCP rea (Ghidra 12.1.4; hizo falta subir `ghidra.startup_timeout_ms` a 1 800 000 con `update_analysis_settings`, el defecto de 330 s vence con este DLL de 29 MB; ya abierto, las consultas son rápidas); [STR] = tabla de cadenas `docs/research/ui/data/strings_merged_latin.tsv` (ids de ed3dlib); [CAP] = captura del original (ninguna en esta sesión); [NV] = no verificado. Nota técnica: `inspect_pe_resources` devuelve 0 recursos en ed3dlib (no hay RT_DIALOG; la UI se construye por código con el framework propio `panel_*`/`radiogru_*`), y las ids de cadena no aparecen como constantes en el código (los mensajes son estructuras `ed3dlib.msg` inicializadas por funciones `FUN_*` con el idioma por columnas): por eso no se pudo hacer xref directo id -> punto de emisión; los avisos se listan con su id y la condición, sin ruta de emisión confirmada salvo donde se indica.

## 1. Ribbon Carga > Barras [REA]
Constructor del menú: `FUN_1803d7a40` (nombre interno del grupo `"cargas_barras"`, aspecto ribbon 0x10; el título del grupo sale de la cadena formateada «Cargas sobre barras» #64898 [STR]/#64571 «Cargas en barras»). Botones en este orden (ids internos del receptor de opción, imágenes `ed3mnucar.c`):
| # | Id interno | Texto (botón / tooltip) |
|---|---|---|
| 1 | `Carga_barra_nueva_carga` | Nueva (título del modo: «Introducir cargas sobre barras» #64344; tooltip #64653: «Permite introducir cargas sobre un grupo de barras.\nSeleccione una o varias barras y pulse el botón derecho del ratón. Aparecerá el cuadro de diálogo de '%s', donde se introducen los valores que definen la carga.») |
| 2 | `Carga_barra_editar_carga` | Editar (#64647 «Editar cargas sobre barras»; tooltip #64654 «Permite modificar los datos de las cargas ya introducidas.» + #64655 «Seleccione un grupo de cargas y pulse con el botón derecho del ratón. Aparecerá el diálogo de '%s' para modificar los datos de las cargas.») |
| 3 | `Carga_barra_borrar_carga` | Borrar (#64656 «Permite eliminar una o varias cargas vistas en pantalla.» + #64657 «Para ello, seleccione el grupo de cargas que desea borrar y pulse el botón derecho del ratón.») |
| — | espacio vacío (separador) | |
| 4 | `Editar_hipotesis_cargas_barras` | Hipótesis (imagen `chbar`; sin tooltip propio en el código del menú) |
| 5 | `Carga_barra_activar_desactiva_pp` | Peso propio (#25165; tooltip #64694 «Permite activar / desactivar la generación automática de la carga de peso propio.» + #64695, ver §6) |
Editar y Borrar comparten el manejador genérico de opción (`FUN_1803d9e20`), Nueva y Peso propio otro (`FUN_1803d9e00` -> `FUN_180230ad0`); el orden de código es Nueva, Editar, Borrar (el orden visual capturado en el grupo Nudos fue Nueva|Editar|Hipótesis|Borrar; aquí lo único cierto por código es el de arriba [REA]; orden visual final y habilitado/deshabilitado [NV, medir]; por analogía con nudos [CAP cn]: Editar/Borrar/Hipótesis en gris sin cargas; Peso propio previsiblemente siempre activo [NV]).
Mensaje de estado del modo: patrón de nudos «Barras - Nueva | - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.» [CAP por analogía, NV para barras].
Hipótesis vista (panel flotante) y Escalas: ver cargas_nudos.md §2.1 y §2.6/§7 (comunes; #64651, #64652, #64478).

## 2. Diálogo «Introducir cargas sobre barras» [REA]
Estructura del panel `descripcion_carga_barra` (`FUN_18014aa40`), caja vertical: (1) fila de hipótesis (`hipocal_panel_con_viento_sin_excentricidad`, etiqueta «Hipótesis» `FUN_1808a87e0`; la parte «viento sin excentricidad» es una variante del panel cuando la hipótesis es de viento [NV]), (2) línea horizontal, (3) cuerpo de la carga, (4) panel dinámico inferior (`panel_crea_dinamico`: ayuda/valores que cambian con el tipo). Título de edición: «Descripción de la carga» #64899.
### 2.1 Selector de tipo (grupo radio `tipo_carga`, enum `ed3tipo_carga_t`, botones con imagen)
Cuerpo `FUN_180154590`: grupo con título en negrita (`FUN_1806135e0`, texto «Tipo de carga» #39064 [STR, NV que sea este id]) y botones de imagen. Orden de aparición y valor del enum [REA, `FUN_180154bb0`]:
| Orden | Clave | Valor enum | Texto |
|---|---|---|---|
| 1 | `carga_puntual` | 0 | Carga puntual #64218 |
| 2 | `carga_uniforme` | 1 | Uniforme #38949 |
| 3 | `carga_faja` | 2 | Faja #38950 / «Carga en faja» #64214 |
| 4 | `carga_triangular_izq` | 3 | Triangular Izq. #38933 / «Carga triangular izquierda» #64216 |
| 5 | `carga_triangular_dcha` | 4 | Triangular Der. #38932 / «Carga triangular derecha» #64215 |
| 6 | `carga_trapezoidal` | 5 | Carga trapecial #64217 |
| 7 | `momento_puntual` | 6 | Momento puntual #64330 |
| 8 | `temperatura_uniforme` | 8 | Incremento de temperatura uniforme #65411 [id por posición, NV] |
| 9 | `temperatura_variable` | 7 | Incremento de temperatura variable #64219 |
Nota: el orden de la UI no coincide con el valor (uniforme=8 va antes de variable=7). Los tipos «Carga variable en el eje x/y local» (#64220/#64221, #64592/#64593) NO están en este selector: pertenecen a láminas/paños (ver laminas.md) [REA por ausencia en el constructor].
### 2.2 Panel de ejes y dirección [REA, `FUN_1801542e0`]
- Existe solo para tipos 0..7 (`FUN_180153650`); **temperatura uniforme (8) no tiene selección de dirección ni ejes** (panel «vacio»).
- Grupo radio `seleccion_ejes` (`ed3tipo_ejes_carga_t`), título en negrita «Ejes sobre los que se define la dirección de la carga» #65395: opción `ejes_globales` (valor 0, «Globales» #65397 / «Ejes globales» #65396) solo para tipos 0..6; `ejes_locales` (valor 1, «Locales» #65398) siempre. Por tanto **temperatura variable solo admite ejes locales** (y el panel de dirección de la temperatura es el de #65310). Ejes locales: «Ejes locales de la barra» #64212 (nombre del sistema).
- Sub-panel Globales: tipos 0..5 usan `FUN_1802c5d10` (dirección de fuerza), momento (6) usa `FUN_1802c5e70` (dirección de momento). Locales: tipos 0..5 `FUN_1802c8020`, momento `FUN_1802c8190`, temperatura variable `FUN_1802c9ec0`. Contenido de esos paneles (botones de dirección +X..-Z, «Definir dirección», «Vector director», «Ángulo en el plano 'XZ' local», «Dirección de la carga respecto al eje 'Z' local») por cadenas: «Dirección y sentido de aplicación de la carga» #65314; «Definir dirección» #64320 + «La carga se aplica en la dirección definida por el sistema de referencia global introducido por el usuario.» #64319; «Vector director» #64318; «Ángulo en el plano 'XZ' local» #64321 y «Ángulo con el plano 'XZ' local» #65313; «Dirección de la carga respecto al eje 'Z' local» #64322; temperatura: «Dirección y sentido de la variación del incremento de temperatura» #65310 con «Dirección según el eje %s, en sentido positivo.» #65311 / «...negativo.» #65312. «Sentido positivo de la carga» #65051 (etiqueta; el texto de sentido positivo mostrado para cada dirección: #65329 «Sentido positivo», #65330/#65331 «Vertical hacia abajo/arriba»; esos dos últimos son de paños, [NV] para barras). Qué botones concretos hay en cada sub-panel y sus valores por defecto: [NV, medir].
### 2.3 Panel de valores por tipo [REA parcial + STR]
Paneles construidos por tipo (dentro del panel dinámico/cuerpo): puntual `FUN_180157770`, uniforme `FUN_180154980`, faja `FUN_18015b8d0`, triangular izq. `FUN_18015b9f0`, triangular dcha. `FUN_18015b990`, trapecial `FUN_18015b930`, momento `FUN_1804138a0`, temperatura uniforme `FUN_180154890`, temperatura variable `FUN_180154a60` (dos campos en una caja horizontal `valores_carga_temperatura`, cada uno creado por `FUN_180154790` con un indicador de cara 1/0; tooltip del conjunto = #64453 «Valor de la temperatura a ambos lados del perfil.» con #64454 «La orientación de la variación térmica sobre la sección transversal dependerá de la opción '%s' de este cuadro de diálogo.»; validador de finalización `FUN_180146730` con los dos campos). Etiquetas por cadenas:
- Puntual: «Valor de la carga puntual» #65289 (valor en kN) + distancia al nudo inicial (#64912: «Cargas y momentos puntuales: '%s' es la distancia entre el nudo inicial de la barra y la posición donde se aplica la carga. '%s' no se utiliza.»).
- Uniforme: «Valor absoluto de la carga uniforme» #64213. Texto de ayuda #64908: «Cargas puntuales, uniformes, en faja y momentos puntuales: '%s' es el valor de la carga. '%s' no se utiliza.» (los %s son los nombres de los campos 1.º y 2.º valor).
- Faja: «Valor absoluto de la carga en faja» #64225; posición inicio/fin por #64913 («Cargas trapeciales, en faja, y triangulares: '%s' es la distancia entre el nudo inicial de la barra y la posición donde comienza la carga, '%s' ... donde termina la carga.»).
- Triangulares: #64910 («'%s' es el valor máximo de la carga. '%s' no se utiliza.»).
- Trapecial: «Valor absoluto de la carga en el extremo inicial» #64223 y «... extremo final» #64224; #64909 («'%s' es el valor de la carga en el punto donde comienza (%s) y '%s' ... donde termina (%s).»).
- Momento puntual: #64330 con el valor (#64908/#64912).
- Temperatura variable: #64911 («'%s' y '%s' son los valores de la temperatura en las caras exteriores o paramentos de la pieza. La orientación de la variación del incremento de temperatura sobre la sección transversal dependerá de la dirección seleccionada.»).
- Signos [STR #64914-#64916]: «Cargas uniformes, en faja, triangulares y trapeciales: %s.» / «Momentos puntuales: %s.» / «Incrementos de temperatura: %s.» (el %s se rellena con el texto del sentido positivo que corresponde a los ejes elegidos, p. ej. de #65311/#65312 o «Dirección según el eje X...»; por analogía con #65311/#65312 [NV]).
- Títulos de columna/cabecera de «valores de carga»: el panel lleva dos campos nombrados por las cadenas anteriores: 1.er valor (el valor o valor inicial/ temperatura de una cara) y 2.º valor (valor final, o '%s' no utilizado) [NV nombres exactos].
Las unidades (kN, kN/m, kN·m, ºC, m) y decimales NO se pueden obtener de estas funciones sin descender a los constructores de campo (`FUN_180154790` etc.): [NV, medir].
### 2.4 Lista de cargas / edición múltiple
No hay evidencia de lista de cargas por barra en este diálogo (la lista tipo tabla es de paños, #64123-64131). «Editar» abre el mismo panel con las cargas seleccionadas y asigna a todas (por analogía con nudos [CAP cn §2.4]) [NV para barras]. «Hipótesis» abre el diálogo reducido de hipótesis [CAP cn §2.5].

## 3. Validaciones y avisos
### 3.1 Datos de la carga [REA, `ed3carbar.c`/`ed3carsup.c`, aserciones y constantes]
Precisiones del programa: `i_PRECISION3` = 0.001, `i_PRECISION4` = 0.0001 (confirmado en `FUN_180157b70`). Constructor de carga lineal sobre barra (proveniente de superficiales, ed3carsup.c:0x3c6-0x3cb) exige: `despl_inicial + 1e-4 > 0`; `despl_final - 1e-3 < longitud_disponible`; `despl_inicial < despl_final`; `|valor_inicial| > 1e-4 || |valor_final| > 1e-4`; los dos valores con **el mismo signo** (`ed3math_valores_tienen_mismo_signo_con_precision`). Ese constructor decide el tipo resultante (misma lógica que usarán las cargas introducidas): valor inicial nulo o final nulo -> triangular (con el valor no nulo como máximo); |v1| = |v2| (tolerancia 1e-4) -> faja si no cubre toda la barra (inicio > 0.001 o fin + 0.001 < longitud) y **uniforme** si cubre toda la barra; si no -> trapecial. Esto es la regla de normalización a reproducir.
### 3.2 Mensajes [STR] (id -> texto) y cuándo se emiten
| Id | Texto | Emisión |
|---|---|---|
| #65050 | No puede introducir cargas de valor nulo. | Aceptar con valor(es) nulo(s) [NV si es esta u otra en barras; por la aserción |v|>1e-4 es el criterio]. |
| #65323 | El valor introducido no puede ser cero. | Campos de valor único (puntual/uniforme/faja/momento) [NV]. |
| #65324 | Al menos uno de los valores de la carga debe ser distinto de cero. | Trapecial/triangular/temperatura variable (dos valores) [NV]. |
| #65319 | La dirección introducida no es correcta. | Vector director nulo en «Definir dirección» [NV barras, [CAP] nudos usa #65319]. |
| #65192 | Se han borrado cargas incompatibles con la longitud de la barra. | Tras cambiar la longitud de una barra (mover extremo, geometría, aplicar cambios) cuando una carga puntual/faja/triang./trapecial/momento queda fuera de [0, L] [NV punto de emisión; texto REA]. |
| #65307 | Al mover una barra sobre otra se pierden sus cargas. | Geometría > Mover barra sobre barra existente: aviso, se descartan sus cargas [NV flujo]. |
| #64002 | Se ha desactivado la generación automática de las cargas de peso propio para esta barra. | Aviso al desactivar peso propio en una barra [NV cuándo salta exactamente, §6]. |
| #64000 | La longitud de la barra es inferior a la longitud mínima permitida. | Geometría; no específico de cargas. |
| #400345 | El nudo no es necesario ya que no tiene cargas ni vinculación exterior. | Nudos. |
Condiciones de rango (el aviso «El valor de '%s' debe estar entre %s y %s.», patrón general ya visto en nudos): rango de cada campo de barra [NV, medir]. El nombre `fabs(...)>1e-4` sugiere que el mínimo práctico es 0.0001 pero el rango mostrado al usuario puede ser 0.01..49000.00 como en nudos [NV].

## 4. Unidades y formato [NV]
Solo se puede afirmar: carga puntual en kN, uniforme/faja/triangular/trapecial en kN/m, momento en kN·m, temperatura en ºC, posiciones en m (las unidades derivan del diálogo de Unidades de CYPE 3D, `dialogs_unidades.md`). Decimales y rangos exactos por campo: pendientes de medir.

## 5. Dibujo 3D [REA/STR parcial]
- Opción de la pestaña «Dibujar las cargas con relleno» #388085 (relleno de los diagramas de carga distribuida, parece opción de Opciones/Escalas) [STR].
- Escalas por tipos: campos Uniforme, Puntual, Faja, Triangular, Trapecial, Momento puntual, Superficial, Temperatura (1.000) y Retracción (100.000) [CAP cn §7]; modo fijo 0.5 m / proporcional 0.1 m por kN [CAP cn §7] para fuerza puntual; la fórmula para uniforme/faja/etc. (¿0.1 m por kN/m?) [NV].
- Proyección sobre la barra, colores, etiquetas y dibujo de temperatura/momento [NV: no se tocó el código de dibujo; requiere medir en el original o rastrear `ed3cardib`/similar].
- Elementos: «Carga en barra» #387137 (nombre de elemento selección).

## 6. Peso propio, Hipótesis, Editar, Borrar
- Peso propio (botón 5): tooltip #64694 + #64695 («Para ello, seleccione las barras para las que desea activar o desactivar la generación de cargas de peso propio y pulse el botón derecho del ratón. Si pulsa el botón derecho del ratón sin haber seleccionado ninguna barra, volverá a aparecer el diálogo en el que puede escoger la acción a realizar.»). Título del diálogo «Generación de las cargas de peso propio de las barras» #64697 (láminas #64702). Tras RMB sin selección vuelve a mostrarse el diálogo de acción (activar/desactivar) [STR]. Aviso #64002 al desactivar [NV]. El botón no es de «modo con selección única» sino con diálogo previo de elección de acción [STR #64695, NV forma exacta].
- Hipótesis (botón 4): imagen `chbar`; asigna otra hipótesis a las cargas seleccionadas [CAP cn §2.5 por analogía].
- Editar/Borrar: como nudos (RMB sobre la selección; Borrar sin confirmación [CAP cn §2.5]) [NV para barras].
- Importación de fichero: la secuencia de cargas no existe en el formato de texto (#64154-64159 solo nudos, barras, láminas; #399481 paños de carga) [STR].

## 7. Modelo de datos propuesto (C++20)
Del solver (CALCESF `esport3d_append_carga_*`, REA [lista de procedimientos]): puntual (locales/general), momento puntual (locales/general), trapezoidal (locales/general), térmica (solo locales), y carga general de nudo; faja y triangulares se reducen a trapezoidal (ver §3.1). Propuesta:
```cpp
enum class BarLoadType { Point=0, Uniform=1, Strip=2, TriangularLeft=3, TriangularRight=4, Trapezoidal=5, PointMoment=6, VariableTemperature=7, UniformTemperature=8 }; // valores = enum ed3tipo_carga_t [REA]
enum class LoadAxes { Global=0, Local=1 };                                  // ed3tipo_ejes_carga_t [REA]
struct BarLoad {
  int id; int barId; int loadCaseId;
  BarLoadType type; LoadAxes axes;                // Global not allowed for types 7,8; temperature uniform has no direction
  // direction: one of +X..-Z presets, user vector, or (local) plane angle / angle vs local Z [exact set NV]
  DirectionSpec direction;
  double startDistance{0}, endDistance{0};       // m from start node, [0,L] (point/moment use startDistance)
  double value1{0}, value2{0};                   // kN, kN/m, kN.m, degC; value1 = max for triangular
  double temperatureFace1{0}, temperatureFace2{0}; // type 7
};
```
Reglas: normalización §3.1 (tolerancias 1e-3/1e-4, mismo signo en trapeciales); al cambiar la longitud, borrar las cargas fuera de rango y avisar con #65192; al mover barra sobre otra descartar cargas (#65307); barras sin límite de nº de cargas (modelo lista, igual que nudos).

## 8. Reglas GUI para el clon
1. Ribbon Carga > Barras: Nueva, Editar, Borrar, separador, Hipótesis, Peso propio, textos y tooltips §1. 2. Diálogo: fila hipótesis, línea, grupo «Tipo de carga» (9 botones de imagen en el orden de §2.1), grupo de ejes (Globales sólo tipos 0-6, Locales siempre; ninguno para temperatura uniforme), grupo dirección, valores del tipo, texto de ayuda de signos con #64908-#64916. 3. Selección LMB + RMB como en nudos. 4. Validaciones §3. 5. Normalización de tipo §3.1.

## 9. Pasada 2 (REA, 2026-11-09): campos, rangos y validadores
Binarios: `ed3dlib.dll` y `cypeud.dll` (exporta `cargas_carga_*_crea`, que ed3dlib importa por delay-load; sus callbacks definen los campos de valor). Los doubles de `udentsal_crea_generado(unidad, min, max, defecto, paso, factor)` se decodificaron desde IEEE-754 [REA].

### 9.1 Valores de fuerza por sistema de unidades
Cada campo se crea con 3 especificaciones (una por sistema de unidades de la obra). Nombres internos de unidad leídos en cypeud.dll: `Tn`, `kN`, `kips`, `Tn_m`, `kN_m`, `kips_ft` (bloque vecino: `Tn_div_m2`, `kN_div_m2`, `psf`, `presion`, de superficiales).

| Carga (campo) | Unidad | min | max | defecto | paso (decimales) |
|---|---|---|---|---|---|
| Puntual («fuerza», FUN_1800011e0/1230/1280) | Tn | 0.001 | 5000 | 1 | 0.001 (3) |
| | **kN** | **0.01** | **49000** | **10** | 0.01 (2) |
| | kips | 1 | 10992 | 2 | 1 (0) |
| Lineal («fuerza_lineal»: uniforme, faja, triangulares, trapecial; FUN_1800010f0/1140/1190) | Tn_m | 0.001 | 1000 | 0.5 | 0.001 (3) |
| | **kN_m** | **0.001** | **10000** | **0.5** | 0.001 (3) |
| | kips_ft | 0.001 | 10000 | 0.5 | 0.001 (3) |
| Momento puntual («momento») | mismas 3 especificaciones que la lineal (Tn_m, kN_m, kips_ft) | 0.001 | 10000 (kN_m) | 0.5 | 3 |

El caso kN puntual (0.01..49000) coincide con el rango medido en nudos (`cargas_nudos.md` §2.2), lo que confirma que el aviso «El valor de 'Valor' debe estar entre 0.01 y 49000.00.» sale de esta especificación. El 9.81 visto en nudos es otro valor por defecto/recordado [NV].

### 9.2 Temperatura, ángulos y vector
- **Incremento de temperatura** (`incremento_grados_centigrados`, FUN_1802cd410): **-100 .. 1000 ºC**, defecto **10**, paso **0.01** (2 decimales). Dos campos en la caja «valores_carga_temperatura» (FUN_180154790): `valor_temperatura1` (alineado a la izquierda, cara 1) y `valor_temperatura2` (a la derecha, cara 0); etiqueta formada con el patrón `"%s %s"` (texto base + sufijo de cara; literales [NV]). Validador conjunto FUN_180146730 (no descompilado). Tooltip #64453/#64454. Temperatura uniforme (tipo 8): un solo campo [NV, FUN_180154890 sin descompilar].
- **Ángulos** (`angulo`/`grados`, FUN_1802cd2f0): **-180 .. 180 º**, defecto **0**, paso **0.001** (3 decimales).
- **Vector director** (FUN_1802c51c0, caja horizontal «direccion_recta_o_plano», campos `direccion_x`, `direccion_y`, `direccion_z`): cada uno **-1000.000 .. 1000.000**, 3 decimales; título del grupo = «Vector director» #64318. Validador FUN_1802c4a40 -> FUN_1802c5540 (vector nulo; aviso esperado #65319 [NV]).

### 9.3 Paneles de dirección
Grupos de botones con imagen, enum `ed3tipo_direccion_t`, orden idéntico al diálogo de nudos: **0 +X, 1 +Y, 2 +Z, 3 -X, 4 -Y, 5 -Z, 6 Definir dirección** (claves `direccion_x_pos`, `y_pos`, `z_pos`, `x_neg`, `y_neg`, `z_neg`, `direccion_def_usuario`); título del grupo «Dirección y sentido de aplicación de la carga» (#65314 [NV id exacto]).
- **Global** (`ed3dirglob.c`, «seleccion_direccion_global»): botón 6 despliega el vector X/Y/Z (§9.2). El panel de momento (FUN_1802c5e70) no se descompiló (solo iconos/textos).
- **Local** (`ed3dirloc.c`, «seleccion_direccion_local»): mismos 6 botones sobre ejes locales; botón 6 despliega una rejilla 2x3 «direccion_def_usuario» con dos campos de ángulo: `angulo_plano_XZ` («Ángulo en el plano 'XZ' local», #64321) y `angulo_respecto_plano_XZ` («Dirección de la carga respecto al eje 'Z' local» #64322 / «Ángulo con el plano 'XZ' local» #65313; cuál va en cada etiqueta [NV]); ambos -180..180 º, 3 decimales, defecto 0.
- **Temperatura variable** (`ed3dirtemp.c`, «direccion_temperatura», enum `i_tipo_t`): tres botones en fila: 0 `direccion_y_pos` (Y+), 1 `direccion_z_pos` (Z+), 2 `direccion_def_usuario` con un único campo `angulo_respecto_plano_XZ` (-180..180 º, 3 dec.). Título del grupo = #65310. Solo ejes locales. Texto del sentido por FUN_1802ca3b0 (no descompilado; #65311/#65312).

### 9.4 Validadores y avisos
- Puntual: FUN_1801466b0 -> FUN_180153710: el valor debe ser **estrictamente > 0**; si no, `dlgavis_fallo_u(dialogo_padre, mensaje)` con el mensaje de `ed3texto.msg` **#65323 «El valor introducido no puede ser cero.»** (se emite con 0 y con negativos: el signo lo da la dirección; los rótulos son «valor absoluto», #64213/#64223-#64225). Se dispara al finalizar (Aceptar) vía `comp_setFuncionSePuedeFinalizar`; el diálogo no se cierra.
- Fuera de min..max: lo valida el propio control con el aviso genérico «El valor de '%s' debe estar entre %s y %s.» [CAP en nudos].
- Temperatura variable: FUN_180146730 valida los dos campos (probablemente #65324; no descompilado) [NV].
- #65050, #65192, #65307, #65319 y #65324: no se halló punto de emisión estático (mensajes de estructuras `FUN_*` sin xref por id).

### 9.5 Dibujo 3D
No localizado en esta pasada (presupuesto agotado): quedan por trazar los consumidores del dibujo de `ed3carbar` y `esport3d_append_carga_*`. Escalas ya medidas en `cargas_nudos.md`.

## 10. Medido en el original (sesión RE 2026-11-09, T-P7-03) [CAP]
Original v2027.a lanzado con `tools/cb_run.ps1` (obra vacía + import `m6_gen.txt`, ribbon Carga, Barras > Nueva, barra 1 por dos LMB + RMB). Capturas `docs/screenshots/cb_*.png` (`cb_b0` tooltip y estado, `cb_b2` diálogo, `cb_t0g..cb_t8g` un diálogo por tipo, `cb_p_*`/`cb_m_*`/`cb_tv*`/`cb_tu` ejes y direcciones, `cb_tabv_*`/`cb_tb_*` orden de tabulación, `cb_v_*` avisos, `cb_tip_*` tooltips, `cb_btn_state`, `cb_draw1` dibujo 3D). Nota de conducción: el campo de valor NO acepta teclas tras un clic; hay que dejarlo con foco por teclado (clic en el campo, `{TAB}`, `+{TAB}`, `^a`, `^a`, texto; el primer carácter tras el primer `^a` se pierde).

### 10.1 Ribbon y estado [CAP]
- Grupo **Barras**, orden visual: **Nueva** (botón grande) | columna Editar sobre Borrar | columna Hipótesis sobre Peso propio (corrige §1: no hay separador visible). Sin cargas: Editar, Borrar e Hipótesis en gris; Nueva y Peso propio activos. Con una carga creada: Editar, Borrar e Hipótesis activos.
- Estado: «Barras - Nueva | - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.» Al activar aparece el panel flotante «Propiedades - Hipótesis vista» (combo Hipótesis + «Ver todas»).
- Tooltips: **Nueva** «Nueva» + #64653 con «'Introducir cargas sobre barras'»; **Editar** «Permite modificar los datos de las cargas ya introducidas. / Seleccione un grupo de cargas y pulse con el botón derecho del ratón. Aparecerá el diálogo de 'Editar cargas sobre barras' para modificar los datos de las cargas.»; **Borrar** «Permite eliminar una o varias cargas vistas en pantalla. / Para ello, seleccione el grupo de cargas que desea borrar y pulse el botón derecho del ratón.»; **Hipótesis**: solo el título «Hipótesis» (sin texto de ayuda); **Peso propio**: título «Generación de las cargas de peso propio de las barras», texto #64694+#64695 y pie «Pulse 'F1' alternativamente para mostrar u ocultar los textos de ayuda.» Tooltips de los botones de tipo = nombre: «Carga puntual», «Carga uniforme», «Carga en faja», «Carga triangular izquierda», «Carga triangular derecha», «Carga trapecial», «Momento puntual», «Incremento de temperatura…» (recortado).

### 10.2 Diálogo «Introducir cargas sobre barras» [CAP]
- Ventana ~402x516 px en (764,263) (pantalla 1920x1080), título rojo con aspa, centrada; Aceptar (azul, por defecto) y Cancelar abajo. Combo **Hipótesis** (defecto Peso propio), línea separadora, grupo en negrita **Tipo de carga** con **9 botones de imagen** en el orden de §2.1. **Tipo por defecto: Uniforme**, ejes **globales**, dirección **-Z** (6.º botón resaltado), valor **9.810**.
- Etiquetas y campos por tipo: Puntual: «Valor» [9.81] **kN** (2 dec.), casilla **Posición relativa** (marcada), «Distancia» [0.000] «(0..1)». Uniforme: «Valor» [9.810] **kN/m**. Faja: Valor kN/m, Posición relativa, «Posición inicial» [0.000] «(0..1)», «Posición final» [1.000] «(0..1)». Triangular izq./der.: «Valor inicial» [9.810] kN/m + Posición relativa + posición inicial/final (la derecha igual, etiqueta tapada por tooltip). Trapecial: «Valor inicial» y «Valor final» [9.810] kN/m, Posición relativa, Posición inicial [0.000], Posición final [1.000]. Momento puntual: «Valor» [9.810] **kN·m**, Posición relativa, Distancia [0.000] (0..1). Temperatura uniforme: «Valor» [10.00] **°C** (2 dec.), **sin** grupos de ejes ni dirección. Temperatura variable: «Valor 1» [10.00] °C y «Valor 2» [10.00] °C.
- «Ejes sobre los que se define la dirección de la carga»: radios **Ejes globales** / **Ejes locales de la barra** (la elección se recuerda al cambiar de tipo); temperatura variable solo «Ejes locales de la barra». «Dirección y sentido de aplicación de la carga»: **7 botones** (+X, +Y, +Z, -X, -Y, -Z, Definir dirección); en momento puntual los iconos llevan flecha doble. Temperatura variable: «Dirección y sentido de la variación del incremento de temperatura», **3 botones** con perfil en I y T1/T2 (2.º por defecto; 3.º = definido, con campo **«Ángulo» [0.000] grados**).
- Definir dirección **global**: «Vector director» X [1.000] Y [0.000] Z [0.000]. **Local**: «Dirección de la carga respecto al eje 'Z' local» con «Ángulo en el plano 'XZ' local» [0.000] grados y «Ángulo con el plano 'XZ' local» [0.000] grados (iconos locales distintos de los globales; 6.º resaltado por defecto).
- Orden de tabulación: (combo Hipótesis), Valor(es), Posición relativa, Distancia / Posición inicial, Posición final, grupo Ejes (un único tope: el radio activo), [X, Y, Z o los dos ángulos si «Definir dirección»], Aceptar, Cancelar, y vuelta a Hipótesis. Los grupos de botones de tipo y dirección NO son topes. Temp. variable: Valor 1, Valor 2, Ejes, Ángulo (si definido), Aceptar, Cancelar. Temp. uniforme: Valor, Aceptar, Cancelar.

### 10.3 Rangos y avisos [CAP]
- Uniforme: «El valor de 'Valor' debe estar entre 0.001 y 10000.000.» (-1, 10000.001); texto: «La cadena introducida no es un número.»; 0.0005 se acepta (redondea a 0.001 y cierra). Puntual: «El valor de 'Valor' debe estar entre 0.01 y 49000.00.» (0 y 49000.01). Distancia relativa fuera de 0..1: **«El valor de 'Distancia' debe estar entre 0.000  y 1.000 .»** (doble espacio). Cuadro «Aviso» (icono amarillo) con Aceptar; el diálogo sigue abierto.
- Faja con inicio 0.6 y final 0.4: el diálogo **se cerró sin aviso** (aceptada; normalización o intercambio [NV]).
- Rangos de temperatura (-100..1000), ángulos y vector: solo REA, no medidos en pantalla.

### 10.4 Dibujo 3D [CAP, `cb_draw1`, `cb_load1`]
- Cargas distribuidas (con «Dibujar las cargas con relleno», activo por defecto) = **banda de relleno amarillo pálido** proyectada desde la barra en la dirección de la carga, contorno amarillo, **líneas amarillas perpendiculares** a intervalos regulares y **etiqueta amarilla** (valor sin signo ni unidad) girada a lo largo; la uniforme cubre toda la barra. Puntual: flecha amarilla con etiqueta («20»). Momento puntual: **arco semicircular amarillo con relleno** en torno a la barra con etiqueta («7»). Faja: banda solo en el tramo con etiquetas en los extremos. Varias cargas se dibujan por separado y superpuestas.

## 12. Medido en la sesión 2 (2026-11-09, T-P7-03) [CAP]
Recetas: `tools/cb_run.ps1` (+ `-NoOpen` para ir directo al ribbon). Coordenadas absolutas (pantalla 1920x1080): ribbon Barras Nueva (421,80), Editar (488,66), Borrar (488,97), Hipótesis (565,66), Peso propio (570,97); barra 1 (X, 4 m) se selecciona con `aclick:1328,677` x2 (el primero se pierde) y RMB `arclick`. Diálogo de carga (402x516 en 764,263, fijo para todos los tipos): botones de tipo y=380, x=796,839,881,923,965,1007,1049,1091,1133; Valor (848,410); casilla Posición relativa (785,438); Distancia/Posición inicial (885,462), Posición final (1045,462); ejes (786,517)/(786,540); direcciones y=617, x=802..1114 paso 52; Aceptar (811,755), Cancelar (1121,755). **Mecánica de escritura**: tras clic en un campo, `{TAB}`,`+{TAB}`,`^a`,`^a`, un carácter comodín `0`, `^a`, valor. Un clic sobre un campo distinto del enfocado NO lo enfoca por teclado: para ir de Valor a Distancia usar `{TAB}{TAB}` desde Valor (orden Valor, Posición relativa, Distancia). Las pruebas de la sesión 1 con posiciones tecleadas (p. ej. «faja 0.6/0.4 cerró sin aviso») eran artefactos de tecleo: ver 12.2.

### 12.1 Peso propio (botón 5) [CAP `cb_pp_a..k`]
- El clic abre **de inmediato** un diálogo modal pequeño, título **«Generación de cargas»** (no el #64697), ~186x134 px en (871,454): radios **Activar** (defecto) y **Desactivar**, botones **Aceptar** (defecto) y **Cancelar**. El diálogo recuerda la última opción elegida.
- Tras **Aceptar** el modo queda activo: estado «Barras - Peso propio | - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.» (antes de Aceptar: «Barras - Peso propio»). Botón resaltado.
- LMB selecciona barras (la barra seleccionada se pinta naranja-marrón); **RMB con selección aplica la opción elegida** sin confirmación ni aviso: con Desactivar la banda amarilla pálida de peso propio de esa barra desaparece. **RMB sin selección vuelve a abrir el diálogo** (con la opción recordada, Desactivar) [#64695 confirmado].
- **No apareció el aviso #64002** al desactivar (ni al aplicar ni al cerrar): [CAP negativo]. No se encontró otra vía que lo dispare; queda como pendiente real (solo texto REA).
- Las cargas de peso propio se dibujan SOLO en este modo (banda amarilla pálida sin etiqueta, con líneas transversales; en barras verticales, línea amarilla); en Nueva/Editar/Borrar no se ven ni se seleccionan.

### 12.2 Posiciones, avisos y errores [CAP]
- Casilla «Posición relativa» desmarcada: la unidad pasa a **m** y desaparece «(0..1)» (`cb_abs_p0`); el campo queda con 3 decimales (0.000).
- Posición absoluta fuera de la barra (L = 4 m): 5 -> «El valor de 'Distancia' debe estar entre 0.000 y 4.000.» (cuadro **Aviso**, icono amarillo, Aceptar; el diálogo sigue abierto); 4.0001 -> mismo aviso; **4.000 se acepta**; -1 -> «El valor de 'Distancia' debe estar entre 0.000 y 1000.000.» (el control tiene rango 0..1000, el límite de longitud se comprueba al Aceptar). Capturas `cb_abs_5/m1/4001/4`.
- Faja con inicio >= final (relativa 0.6/0.4 y 0.5/0.5): cuadro **«Error»** (icono rojo con aspa): **«El valor de 'Posición inicial' debe ser inferior al de 'Posición final'.»**; el diálogo sigue abierto (`cb_ed_f1`, `cb_ed_feq`). Corrige 10.3.
- El diálogo «Nueva» **recuerda el último tipo y valores** de la sesión (tipo Puntual, 20.00, Posición relativa marcada reabrieron así; `cb_ml_n3`).

### 12.3 Editar y Borrar con cargas [CAP]
- **Editar**: estado «Barras - Editar | - LMB: ... - RMB: ...»; tooltip del botón visible (`cb_ed_g2`). Las cargas se seleccionan con LMB sobre la carga/barra o ventana (2 clics tras el de activación): la carga seleccionada se pinta **naranja** (banda con contorno naranja grueso). RMB abre el diálogo de título **«Descripción de la carga»** (mismo panel, rellenado con la carga: tipo, valor, posiciones, ejes, dirección). Con **varias cargas de tipos distintos** el diálogo muestra los datos de la **primera** (Uniforme 3.000 con una puntual de 20 en la selección) y al Aceptar **sobrescribe TODA la definición (tipo, valores, posiciones, ejes y dirección) en todas las cargas seleccionadas**: una puntual y una uniforme editadas con Valor 7 quedaron las dos como uniforme 7 (`cb_ml_f1/f2`).
- **Borrar**: estado «Barras - Borrar | ...»; selección naranja; RMB borra **sin confirmación**; el modo sigue activo y al no quedar cargas Editar, Borrar e Hipótesis vuelven a gris (`cb_bd*`).

### 12.4 Avisos y rangos exactos [CAP]
Tipos de cuadro: **Aviso** (icono amarillo) para rangos y dirección; **Error** (icono rojo con aspa) para las reglas de negocio. Todos con Aceptar (defecto); el diálogo de carga sigue abierto y el campo vuelve al valor anterior.
| Caso | Cuadro | Texto exacto |
|---|---|---|
| Puntual 0 | Aviso | El valor de 'Valor' debe estar entre 0.01 y 49000.00. |
| Uniforme/Faja 0 | Aviso | El valor de 'Valor' debe estar entre 0.001 y 10000.000. |
| Triangular 0 | Aviso | El valor de 'Valor inicial' debe estar entre 0.001 y 10000.000. |
| Trapecial inicial 0 / final negativo | Aviso | ...'Valor inicial' / 'Valor final' debe estar entre 0.001 y 10000.000. |
| Temp. uniforme 1001 / -101 | Aviso | El valor de 'Valor' debe estar entre -100.00 y 1000.00. |
| Temp. uniforme 0 | Error | El valor introducido no puede ser cero. (#65323) |
| Temp. variable ±fuera | Aviso | El valor de 'Valor 1' (o 'Valor 2') debe estar entre -100.00 y 1000.00. |
| Temp. variable 0 y 0 | Error | Al menos uno de los valores de la carga debe ser distinto de cero. (#65324) |
| Temp. variable 0 y 10, 10 y 0, 30 y -30 | (aceptada; signos distintos permitidos) | |
| Vector director 0,0,0 | Aviso | La dirección introducida no es correcta. (#65319) |
| Vector X=1001 | Aviso | El valor de 'X' debe estar entre -1000.000  y 1000.000 . (espacios dobles como en 10.3) |
| Ángulo local 181 | Aviso | El valor de 'Ángulo en el plano 'XZ' local' debe estar entre -180.000 y 180.000. (con salto de línea) |
| Faja inicio >= fin | Error | El valor de 'Posición inicial' debe ser inferior al de 'Posición final'. |
| Distancia absoluta > L | Aviso | El valor de 'Distancia' debe estar entre 0.000 y 4.000. |
- En todos los tipos de rango positivo (puntual, lineales) **el valor 0 y los negativos dan el aviso de rango** (el signo lo da la dirección). **#65323 solo sale en temperatura uniforme** (rango que incluye 0) y **#65324 solo en temperatura variable con ambos 0**. **#65050 no apareció** en ningún flujo de barras de la interfaz.
- Trapecial: no hay triangulares por valor 0 (el campo exige >= 0.001); las triangulares se crean con sus botones propios.
- Tamaño del diálogo de carga: 402x516 px (ancho x alto) centrado en pantalla, **la altura se fija al abrir según el último tipo recordado y no cambia al cambiar de tipo dentro del diálogo**: 516 (tipos con dirección), ~416 (temperatura variable), ~262 (temperatura uniforme). Diálogo Hipótesis (modo Hipótesis): ~188x120, título «Hipótesis», combo + Aceptar/Cancelar; estado «Barras - Hipótesis | ...». Diálogo Escalas ~308x576.
- Textos de ayuda de signos #64908-#64916 y panel dinámico inferior: **no se muestran** en el diálogo de barras (F1 y pasar el ratón sobre campos, etiquetas, iconos o la zona vacía inferior no muestran más que los tooltips de campo «Valor / Debe estar entre ...» y el nombre del botón de tipo; `cb_h_*`). Probablemente pertenecen a la tabla de láminas/paños.

### 12.5 Dibujo 3D y escalas [CAP `cb_d_all`, `cb_d3_all`, `cb_sc_*`, `cb_sc2_*`]
- Temperatura uniforme: banda amarilla pálida a un lado de la barra (hacia +Z local/arriba con la barra Y) con etiqueta «**20/20**» girada a lo largo (formato `T1/T2`). Temperatura variable: banda con etiqueta «**30/-30**» (T1/T2). Ancho constante a lo largo de la barra (sin cuña).
- Carga distribuida en ejes locales (uniforme 5 kN/m, local, dirección por defecto): misma banda; con la barra X el -Z local coincide con el global.
- **Escalas modo fijo** (defecto «Proporcionales a un valor fijo», 0.500 m): TODAS las bandas (uniforme 5 y 10, temperatura 20 y 30/-30) miden 0.5 m (50 px a 100 px/m) con independencia del valor. **Modo proporcional**: altura = **0.1 m por unidad (kN, kN/m, kN·m) x escala hipótesis x escala del tipo**: uniforme 5 -> 0.5 m, 10 -> 1.0 m; trapecial 5->10: de 0.5 a 1.0 m (cuña); momento puntual 5 -> arco de radio ~0.5 m. El campo «Uniforme» = 2 no alteró la trapecial (escala por tipo). Temperatura proporcional no medida (nota: [NV]).
- Diálogo Escalas (proporcional): radios «Proporcionales a un valor fijo» y «Proporcionales al valor de la carga»; tabla Hipótesis|Escala (Peso propio 1.000) y «Escalas de cargas por tipos» (Uniforme...Temperatura 1.000, Retracción 100.000).
- Selección: carga seleccionada = banda naranja con contorno naranja grueso; barra seleccionada (peso propio, hipótesis) = línea naranja-marrón.

### 12.6 Geometría con cargas [CAP parcial]
- **Borrar barra** (Geometría > Barras > Borrar, LMB + RMB): la barra y **todas sus cargas se eliminan sin aviso**; Editar/Borrar de Carga quedan en gris; el nudo extremo queda aislado.
- Nudos > Mover: tras seleccionar el nudo, teclear un número abre la **«Calculadora»** (Coordenada X/Y/Z, teclado numérico, ENTER pasa de campo) para fijar la posición; el tooltip del nudo muestra «N2 (4, 0, 0)» / «2.000, 0.000, 0.000 m». En la prueba no se vio refresco de la geometría ni aviso #65192 (probablemente hace falta confirmar en la calculadora; no concluyente).

## 13. Pendiente real (solo lo imposible o no concluyente en esta sesión)
1. **#64002** (peso propio desactivado «para esta barra»): no se disparó con el diálogo Activar/Desactivar; sin otra vía conocida.
2. **#65050** no emitido en barras; **#65192** y **#65307** (mover/dividir barra con cargas): el movimiento de nudo por la Calculadora no se pudo concluir (sin refresco ni aviso) y no se probó Mover extremo, Dividir ni mover barra sobre otra. Requiere sesión dedicada a Geometría.
3. **Hipótesis adicionales** (combo no scriptable): efecto de «Hipótesis» al reasignar cargas, hipótesis de viento sin excentricidad y escalas por hipótesis distintas de 1.000. Acción manual pedida al orquestador: crear una hipótesis adicional en la obra del original (Obra > Hipótesis) y elegirla en el combo con el ratón, capturando con `fshot`.
4. Textos de ayuda #64908-#64916 (no visibles en barras), escala proporcional de temperatura, factor exacto del arco del momento y direcciones definidas por vector/ángulos en 3D (no dibujadas aparte).

## 14. Backend implementado (T-P7-03, 2026-11-09)
Código: `src/core/bar_load.{h,cpp}` (modelo, campos, avisos, normalización, geometría de dibujo, escalas, reconciliación con la geometría), `src/core/bar_load_edit.{h,cpp}` (comandos con deshacer), sección JSON `bar_loads`, tests `tests/bar_load_tests.cpp`. Prioridad [CAP] sobre [REA]: defecto 9.810, rangos y textos de §10-§12.
- **Modelo**: `BarLoad` (= `BarLoadData` + id + barra) con tipo (valores de `ed3tipo_carga_t`), ejes, dirección (`LoadDirection` de nudos: +X..-Z / vector), ángulos locales, dirección de temperatura (Y+, Z+, ángulo), posiciones relativas o absolutas tal como se teclearon, valor 1 / 2. Forma canónica (`normaliseBarLoadData`): redondeo a los decimales del campo (puntual 2, temperatura 2, resto 3), campos sin uso a su defecto. `BarLoads::selfWeightOff` guarda las barras sin generación automática de peso propio (no es una carga: Editar/Borrar siguen en gris).
- **Comprobación** (`checkBarLoadData`, en el orden de tabulación): valores (Aviso de rango; 0 solo da #65323 en temperatura uniforme y #65324 con ambos nulos en variable), posiciones (rango 0..1 o 0..1000; longitud de la barra más corta seleccionada sobre el valor sin redondear; inicio < fin = Error), dirección (vector, ángulos, #65319). Los textos incluyen la rareza medida de los controles sin unidad («entre 0.000  y 1.000 .»).
- **Normalización de tipo** (`normaliseLinearLoad`, §3.1): nulo -> triangular; |v1|=|v2| -> faja / uniforme (cubre la barra); resto trapecial. Se usa al partir cargas al dividir una barra; no se aplica a las cargas creadas con el diálogo (no medido).
- **Operaciones**: `addBarLoads`, `editBarLoads` (sobrescribe todo, también hipótesis), `changeBarLoadCase`, `deleteBarLoads`, `setBarSelfWeight`; `BarLoadSession` recuerda el último tipo y valores y la opción de Peso propio. Borrar barra / nudo: sus cargas se eliminan sin aviso (deshacer las devuelve); borrar hipótesis elimina sus cargas de barra en el mismo comando (`LoadCaseDependents::barLoads`).
- **Supuestos no medidos (TK-098)**: acortar barra (relativas se escalan; absolutas fuera de [0, L] se borran, `SnapshotCommand::barLoadsRemovedOutOfLength` y `barLoadsOutOfLengthMessage` #65192 para la GUI); dividir barra (reparto físico entre tramos: uniformes y temperaturas se copian, puntuales/momentos al tramo que los contiene, distribuidas recortadas con interpolación); unir dos barras al borrar el nudo intermedio (pierden sus cargas); triangular izq. = máximo en el inicio; ángulos locales (alfa en XZ desde +x hacia +z, beta hacia +y); sistema local de barras verticales y signo del giro; lado de la banda (hacia la dirección de la carga, §10.4), flecha puntual como en nudos; líneas transversales cada 0.25 m; temperatura proporcional 0.1 m/ºC; invertir el eje x no mueve las cargas.
- **Dibujo** (`barLoadDrawing`, funciones puras): banda (polígono + líneas + etiquetas) para uniforme/faja/triangulares/trapecial/temperaturas (ancho constante, etiqueta `T1/T2`), flecha puntual (punta en la barra), arco semicircular de momento. Tamaño: modo fijo = tamaño máximo (los demás extremos en proporción), modo proporcional = 0.1 m x valor x escala de hipótesis x escala del tipo (`barLoadDrawSizeM`, escalas compartidas con nudos en `Project::nodalLoads.scales`).
