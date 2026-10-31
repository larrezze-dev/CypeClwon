# Páginas web adicionales de info.cype.com (T-RE-07)

Leídas con WebFetch el 2026-10-31. Los resúmenes proceden de un extractor automático (modelo pequeño): fiables para estructura y nombres; cifras y fechas conviene recontrastarlas si se usan como requisito.

Fuentes:
- https://info.cype.com/es/producto/cype-3d-entorno-de-trabajo-y-pestana-ventana/
- https://info.cype.com/es/producto/cype-3d-armaduras/
- https://info.cype.com/es/normativa-implementada/ (filtrada por CYPE 3D por el extractor)
- https://info.cype.com/es/programa_relacionado/cype-3d/ (historial; solo 1.ª página de 20)
- Enlaces adicionales detectados, no leídos: `/es/tema/cype-3d-configuracion-del-entorno-de-trabajo/`, `/es/tema/cype-3d-definicion-y-edicion-de-armaduras/`, `/es/novedades/`, `/es/actualizacion/`.

## 1. Entorno de trabajo y pestaña Ventana

- Solapas inferiores izquierda: **Estructura** y **Cimentación** (esta recoge resultados de la estructura). Ribbon dependiente de la solapa.
- Pestañas de *Estructura*: Obra, Geometría, Propiedades, Carga, Uniones, Cálculo, **Armaduras (solo aparece si hay pilares/vigas de hormigón definidos)**, Ventana.
- Área de trabajo: 3D con encuadre, órbita, zoom; varias ventanas 2D/3D simultáneas.
- Panel lateral izquierdo: Etiquetas, Categorías, Opciones de visualización. Paneles y área de trabajo son *ventanas anclables*.
- Barra superior izquierda: Archivo, guardar, Deshacer, Rehacer, Listados, Planos (impresión), accesos directos personalizables. Superior derecha: plantillas, capturas, Ayuda.
- **Solapa Cimentación**: ribbon con bloques *Obra*, *Elementos de cimentación*, *Límites*, *Cálculo*; área de trabajo en planta.

### Ventana > Abrir nueva (panel "Vista nueva")
1. Vista 2D de un plano ortogonal al eje X, Y o Z (dos líneas que se cortan o son paralelas).
2. Vista 2D de un plano (tres puntos no alineados).
3. Vista 3D de toda la estructura.
4. Vista 3D de una parte de la estructura (dos puntos = diagonal del volumen; no en plano perpendicular a un eje).
5. Vista 3D de una selección de elementos (botón izq. selecciona, dcho. confirma).
6. Vista 2D de un alzado paralelo a rejilla X / Y (requiere rejilla definida en Geometría; navegación "Avanzar/Retroceder una línea", opción "Alzados").
7. Vista 2D por niveles (requiere niveles; "Subir/Bajar un nivel", opción "Niveles").
Después: campo *Título de la ventana* + Aceptar.

Otras opciones: Renombrar, Cerrar, Anterior/Siguiente, Cascada, Mosaico horizontal/vertical, Ordenar ventanas (diálogo), Seleccionar ventana (lista con nº entre corchetes, tipo 3D/2D, título). Ventana activa en azul, resto gris; controles min/rest/max/cerrar por ventana.

### Panel lateral
- **Etiquetas** (capas): una por defecto; elemento puede estar en varias. Botones Añadir, Borrar, reordenar, *Asignar elementos a etiquetas* (diálogo con Operación = Asignar/Reemplazar/Eliminar + Etiquetas, tras seleccionar con panel "Propiedades - Selección"). Columnas: Referencia, Activa, Color (+ "Más colores"), Representación (realista/alámbrica), Visible, Bloqueo.
- **Categorías**: visibilidad y bloqueo de Nudos, Barras, Tirantes, Pilares, Vigas, Paños, Láminas; casilla "Representar la estructura usando los colores de las capas"; desplegable "Elementos considerados en el análisis": Tensión/Deformación, Pandeo, Modal.
- **Opciones de visualización**: información de elementos, textos, ejes locales, láminas con relleno, cargas con relleno, dimensiones de los nudos (unidades), tamaño de textos (%).

## 2. Pestaña Armaduras

Dos bloques: **Pilares** y **Vigas**. Solo para hormigón armado definido en Geometría.

### Pilares
- *Editar armaduras* (clic en pilar) abre ventana **Edición de pilares** con pestañas "Edición sobre tabla" y "Edición sobre cuadro de pilares".
- Pestaña tabla, 4 cuadrantes: *Agrupaciones* (Revisado, Bloqueado, Cumple, Plantas); *Vista en planta de pilares*; *Edición del armado* (plantas, cota, dimensión, longitudinal: Esquinas/Cara X/Cara Y; transversal: Cercos/Separación; cuantía geométrica As/Ac; % barras solapadas, desplazamiento si 50 %; diámetro cercos y ramas; "Seleccionar una disposición"); *Resumen de las comprobaciones* (Posición Cabeza/Pie, aprovechamiento máx., combinación, esfuerzos pésimos vs de referencia, diagrama de equilibrio, lista de comprobaciones; Ver listado, Exportar, Compartir, Imprimir).
- Otras: Buscar un pilar, Reordenar todo, **Rearmar** (Agrupación / Armadura transv. agrupación / Todo / Armadura transv. todo), Resultados>Comprobación, Despiece, Vista 3D, Esfuerzos (axil, momentos, cortantes, torsores; escala, hipótesis, combinaciones), Mostrar zonas de densificación, Actualizar.
- *Bloquear/Desbloquear*: bloqueados en magenta, no se modifican al dimensionar.

### Vigas
- *Editar armaduras* abre editor de vigas (compartido con CYPECAD; en 2027.a ribbon en lugar de menús). Visor principal (sección longitudinal), visor secundario (áreas de armadura necesaria roja / efectiva verde), panel lateral: *Visualización*, *Gráficas de esfuerzos* (momento, cortante, axil, torsor; envolventes/hipótesis/combinaciones; flecha), *Gráficas de áreas de armadura*.
- Herramientas: Rehacer/Deshacer/Guardar; Armaduras longitudinales (montaje, refuerzo, piel, cosido, dividir, solapes, unir, dividir paquete, otra capa, editar, patillas 90º/90º+horizontal/135º/gancho 180º, borrar); Armaduras transversales (estribos: cerrado, cerrado con rama superior solapada, abierto, abierto con rama de cierre; ramas: recta, en S, cerrada; plantilla; editar/dividir/igualar/borrar tramos); Huecos; Celosías, Macizados, Vigas pretensadas (solo CYPECAD según el texto); Secciones transversales (auto, 1 punto, 2 puntos, borrar), Editar sección; Pernos conectores; Rearmar; Comprobaciones (perfiles metálicos/mixtos/madera: tabla Errores, Peso, aprovechamiento resistencia y flecha); comprobaciones ELU/ELS en punto pésimo o en un punto; flechas en vanos; grupos de flecha (solo CYPECAD); áreas necesarias/efectivas; errores/incidencias.
- *Asignar armaduras*: viga origen en naranja, compatibles en amarillo; clic dcho. sale. *Bloquear/Desbloquear*: magenta, no impide edición manual.
- La página no cita normas concretas.

## 3. Normativa implementada (filtrada a CYPE 3D)
Solo aparecen las incorporadas recientemente (parches 2026); el extractor no devolvió el catálogo completo, por lo que la lista histórica queda **por verificar**.

| Norma | País | Ámbito | Parche |
|---|---|---|---|
| ACI 318-25 | EE. UU. | Hormigón | 2026.c |
| IS 801-1975 (Rf 1995) | India | Acero conformado | 2026.b |
| SR EN 1993-1-3 | Rumanía | Acero conformado | 2026.b |
| SR EN 1993-1-1 | Rumanía | Acero laminado y armado | 2026.b |
| ABNT NBR 8800:2024 | Brasil | Acero laminado y armado | 2026.a |
| NSE 3-2024 | Guatemala | Sismo | 2026.c |
| RPA 2024 | Argelia | Sismo | 2026.b |
| DPT 1301/1302-61 | Tailandia | Sismo | 2026.a |
| DPT 1311-50 | Tailandia | Viento (sismo en 2026.a/c) | 2026.a, 2026.c |

Página con filtros País / Categoría / Ámbito / Programa. Hormigón en CYPE 3D: solo ACI 318-25 figura en este filtrado (las demás normas de hormigón ya existentes, p. ej. Código Estructural/EC2, no aparecieron: no concluir que no existan).

## 4. Historial de versiones (solo primera página de 20)
- **2027.a (09/06/2026)**: análisis **pushover** (módulos "Análisis de pushover" + "OpenSees"); **análisis elástico de segundo orden** P-δ/P-Δ en tensión/deformación, modal y pushover (módulo OpenSees); editor de vigas con ribbon; **Módulo de pilotes IV** (micropilotes).
- **2026.c (21/01/2026)**: nuevos códigos internacionales de viento/sismo y ACI 318-25.
- **2026.b (02/10/2025)**: asociación de ficheros en Windows (doble clic); comprobaciones de pilotes/cimentación (varias zonas de estribos, encepado de cabeza de pilote, CIRSOC 103); barras solo compresión (módulo NLT; ya existían solo tracción); morteros intumescentes vía Open BIM Database y nuevos listados de protección contra incendio; RPA 2024, anexos nacionales EC3 Rumanía, IS 801, AS 3600:2018, AS 5216:2021.
- **2026.a (09/06/2025)**: **análisis modal** (nuevo módulo); análisis espectral modal en cálculos no lineales (listado "Justificación de la acción sísmica").
- Versiones anteriores: no leídas (19 páginas más de archivo).

## Implicaciones para el clon
- Armaduras es una pestaña condicional (aparece solo con pilares/vigas de hormigón).
- El editor de vigas/pilares de hormigón es un subsistema grande (compartido con CYPECAD) con ventana propia: planificar como épica independiente.
- Las vistas (2D por plano, alzado, niveles, 3D parcial/selección) son una funcionalidad central del MDI.
