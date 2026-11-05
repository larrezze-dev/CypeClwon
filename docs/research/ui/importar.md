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

## Pendiente (no verificado)
Tipos de entidad DXF/DWG aceptados y su conversión (línea -> barra, 3DFACE -> lámina, polilínea cerrada -> paño); diálogo «Importar geometría» (disposición exacta de los campos, valores por defecto); máximo de capas; tolerancia de fusión de nudos; separadores/decimal del fichero de texto. Siguiente paso REA: abrir `imporstr.dll` y `dxflib.dll`, analizar `impdxf_se_lee_estructura_dxf` y el lector de secciones de texto (cadenas #106871-106890).
