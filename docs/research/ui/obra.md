# Pestaña Obra

Fuente: [captura] `07_ribbon_tabs.png` pestaña Obra no está en esa captura (solo 7 pestañas posteriores): jerarquía tomada de `02_ui_inventory.md` (observado en sesión previa). Cadenas: [cadena] `ed3dlib.dll`.

## Comandos (índice)
| Grupo | Comando | Evidencia |
|---|---|---|
| (sin grupo) | Datos generales | #139006 "Datos generales" (en: General data); #75057 "Datos generales de la obra" |
| Obra | Biblioteca de perfiles | #315578; #131489 "Gestión de la biblioteca de perfiles" |
| Obra | Catálogos de fabricante | #376157 |
| Obra | Biblioteca de rótulas plásticas | #381517 (en: Plastic hinge library) |
| Obra | Rendimiento | #393174; #377916 "Opciones de rendimiento para el análisis no lineal" |
| Obra | Unidades de acotación | #64848; opciones de unidad: Metros / Centímetros / Milímetros (#59655-59657) |
| Numeración | Editar, Reiniciar | #395135 "Numeración", #395136 "Reiniciar" |
| Vista 3D | Estructura completa, Ventana activa, Selección de elementos, Uniones | #64791, #64792, #64794 (+#64795 tooltip "Generación de una vista tridimensional que incluye sólo los elementos seleccionados"), #82357 |
| Listados | Elementos seleccionados, CSV de los elementos seleccionados, Etiquetas de vigas | #393601, #393602, #393603 |
| BIMserver.center | Vincular, Desvincular, Consultar, Compartir | #138713 "Compartir"; resto no localizado |

## Diálogos y campos verificados por cadena
**Numeración** (#395134): al aceptar, mensaje "Este proceso asignará una nueva referencia a todos los elementos del modelo, por lo que la referencia de algunos elementos puede cambiar…". Campos candidatos: "No numerar" (#59909), "Numeración consecutiva de la obra" (#59911), "Referencia inicial" (#64972), "Referencia base" (#64975), "Prefijo" (#33058), "Numerar primero los pórticos paralelos al eje Y global" (#64889), "Asignar referencia" (#397012). Validaciones (mensajes): "La referencia está repetida." (#64970), "La referencia no puede quedar vacía." (#64971), "El prefijo no puede quedar vacío." (#64974).

**Unidades de acotación** (#64848): Metros, Centímetros, Milímetros (verificado como etiquetas; asociación con el diálogo [no verificado]).

**Rendimiento / análisis no lineal** (#377916 y vecinos): "Sólo tracción", "Sólo compresión" (#377935, #377979), Comportamiento (#377936), Sentido: Positivo/Negativo/Ambos (#377980-377983), "Actualizar las combinaciones no lineales tras el análisis lineal" (#377977, #377978).

**Normas** (en Datos generales, #127953-127956): "Norma de acero laminado", "Norma de acero conformado", "Norma de madera", "Norma de aluminio extruido". Otras cadenas relacionadas: Hormigón para vigas de forjado / de cimentación (#61380, #61381), Hormigón en cimentaciones (#64574), "Proceso constructivo" (#46087), "Situaciones de proyecto introducidas por el usuario" (#46232), "Hipótesis adicionales" (#46254 en e3dcomun), "Accidental no sísmica" (#46227), E.L.U. / E.L.S. (#46228, #46229), "Duración de las cargas introducidas en las hipótesis de sobrecarga" (#64962), "Considerar la dimensión finita de los nudos" (#63989, tooltip #63990: activar si se calcularán/comprobarán uniones), "Comprobar la resistencia al fuego" (#46972), "Sistema sismorresistente" (#30207), "Sismo dinámico" (#64796).

**Biblioteca de perfiles** (#131489 "Gestión de la biblioteca de perfiles"): campos de material en #139476-139479 (Módulo de cortadura, Módulo de Poisson, Coeficiente de dilatación), "Límite elástico" (#29413), "Módulo de elasticidad" (#29419), "Datos definidos en biblioteca" (#75025). Resto [no verificado].

**Listados**: ver `03_features_web.md` (HTML/DOCX/PDF/RTF/TXT; CSV). "No hay elementos para listar." (#117226).

## No verificado
Todos los valores por defecto, rangos numéricos, estructura del diálogo Datos generales (por pestañas: ver wizard en `01_reverse_engineering_findings.md`), diálogos BIMserver.center, captura de la pestaña Obra.

## Captura real de la pestaña Obra (2026-10-31, `docs/screenshots/orig_obra_tab.png`, TK-022)
Original v2027.a con una obra vacía abierta, ventana maximizada 1920x1032. Título de ventana: **"CYPE 3D v2027.a - orig_obra.ed3"** (sin marca de modificada en una obra recién creada).
Pestañas: Obra (seleccionada al abrir), Geometría, Propiedades, Carga, Uniones, Cálculo, Armaduras, Ventana. Grupos de la pestaña Obra, de izquierda a derecha:
| Grupo | Contenido (verificado en pantalla) |
|---|---|
| (sin título) | **Datos generales**: botón grande (icono de hoja con lista, texto en dos líneas "Datos / generales") |
| Obra | Tres filas de botones pequeños con icono: Biblioteca de perfiles, Catálogos de fabricante, Biblioteca de rótulas plásticas; segunda columna: Rendimiento, Unidades de acotación |
| Numeración | Botones grandes **Editar** y **Reiniciar** (iconos "123") |
| Vista 3D | Estructura completa, Ventana activa, Selección de elementos (deshabilitados), **Uniones** (botón de alternancia, pulsado) |
| Listados | Elementos seleccionados, CSV de los elementos seleccionados, Etiquetas de vigas (deshabilitados) |
| BIMserver.center | Vincular (habilitado), Desvincular, Consultar, Compartir (deshabilitados) |
Resto de la ventana: barra de herramientas superior derecha con iconos, panel izquierdo Etiquetas / Categorías (Nudos, Barras, Tirantes, Pilares, Vigas, Paños, Láminas) / Opciones de visualización, vista 3D con tooltip de coordenadas, pestañas inferiores Estructura / Cimentación y barra de estado "Seleccione una opción del menú.".
