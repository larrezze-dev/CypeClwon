# Funcionalidades de CYPE 3D (fuente: info.cype.com, leído 2026-10-31)

Páginas: /es/software/cype-3d/ y /es/producto/cype-3d-{obra,cálculo,geometria,propiedades,carga,uniones,cimentacion,documentos-y-exportacion,analisis-de-pandeo,analisis-modal,analisis-de-pushover}/.
Leídas completas: Uniones, Documentos y exportación, Pandeo, Modal. Parciales (resumidas por el extractor, marcar como "por verificar"): Obra, Cálculo, Geometría, Propiedades, Carga, Cimentación, Pushover. No leídas: Inicio, Entorno de trabajo, Armaduras, Normativa implementada (/es/normativa-implementada/), Historial.

## Elementos
Nudos · Barras (genérica lineal/no lineal, solo tracción/compresión, pretensado) · Pilares (por pares de niveles) · Vigas (horizontales/inclinadas, continuas) · Tirantes (biarticulados, solo tracción) · Láminas (EF de superficie, contorno poligonal) · Piezas (grupos de barras alineadas) · Diafragmas rígidos (por niveles, cargas Fx, Fy) · Paños de carga · Cargas superficiales.

## Materiales y secciones
Hormigón armado (pilares/vigas/cimentación; acero de barras; árido; recubrimientos; mermas) · hormigón en masa · acero laminado (doble T, C, T, L, platabanda, redondo, cuadrado) · acero armado de chapas (doble T, U, T, cajones, tubos, chapa) · acero conformado (tubo, cajón, U rigidizado, Z, omega, canal, L) · madera (aserrada, redondo, rectangular, canto variable; clase de servicio, duración) · aluminio extruido y secciones especiales · mixtos hormigón-acero · pretensado · genérico isótropo/ortótropo (CLT). Disposiciones: simple, dobles (cajón, I, T), cuádruples, con presillas. Biblioteca de aceros de usuario; catálogos de fabricante (Open BIM Database).

## Propiedades
Vinculaciones interiores (empotrado, parcial, articulado, genérico) y exteriores (libre, empotramiento, articulación, deslizamientos, elásticas); ligaduras; pandeo (longitudes/coeficientes por plano, 0.5/0.7/1/2, aproximado traslacional/intraslacional, axiles críticos); parejas de arriostramiento; articulaciones y empotramientos parciales; rótulas plásticas; láminas: vinculaciones, bandas de integración, discretización, factores de rigidez.

## Cargas
Barras: puntual, uniforme, faja, triangular izq./dcha., trapecial, momento puntual, temperatura (uniforme/variable); posición absoluta/relativa. Nudos: puntuales, desplazamientos/giros prescritos. Láminas: uniforme, variable, temperatura, retracción. Peso propio automático. Paños (uniformes/variables con altura; reparto paralelo/perpendicular; viento vs. verticales). Cargas superficiales. Hipótesis; hipótesis nocional; P-Δ. Combinaciones según situaciones de proyecto (persistente/transitoria, sísmica, accidental, incendio) con γ y ψ; combinaciones no lineales manuales o generadas. Sismo estático (S1/S2, XY). Categorías de uso.

## Análisis
Lineal y no lineal de tensión/deformación (OpenSees) · segundo orden P-Δ · dimensionado de perfiles (no dimensionar/rápido/óptimo) · dimensionado de uniones · sismo dinámico (espectro, modos, % de masa; EC8 y normas por país) · pandeo lineal (factores críticos, modos, sensibilidad por energía) · modal (frecuencias, periodos, participación) · pushover (control por desplazamientos, rótulas plásticas, FEMA 440, ASCE 41-23, EC8 N2, Newton-Raphson/Krylov, línea de búsqueda) · resistencia al fuego (acero: revestimientos, temperatura crítica; madera) · rótulas: Elastic-hardening, IMK modificada, genérica por tramos, AISC 342-22, ACI 369.1-22, Hysteretic SM.

## Normativas
Selección por material (hormigón, acero conformado, acero laminado, madera, aluminio). Asistentes C.E./CTE: Código Estructural, CTE DB SE-A, CTE DB SE-M, Eurocódigos 2, 3, 4, 8, 9, CTE DB-SE-C. Otras citadas: AISC (LRFD/ASD), NBR 6118, CEB-FIP 90, ASCE 41, ACI 369.1. Lista oficial: por leer.

## Uniones
Nudo/extremo/grupo; generación automática y agrupación; dimensionado soldado/atornillado/placas de anclaje; comprobaciones con referencias normativas; planos de detalle y listados; rigideces rotacionales (componentes/Frye-Morris). Módulos: Uniones I–V, Placas de anclaje (según licencia). Otros: CYPE Connect, StruBIM Steel.

## Cimentación
Zapatas aisladas/combinadas (cuadradas, rectangulares, piramidales, excéntricas), encepados 1–7 pilotes, vigas de atado y centradoras, pilotes y micropilotes; generación automática; comprobar/dimensionar (rápido, iterativo, individual); tablas de armado; fisuración; deslizamiento; hormigón de limpieza.

## Documentos y exportación
Listados HTML/DOCX/PDF/RTF/TXT; planos DWG/DXF/PDF; CSV de nudos, barras y bandas; BIMserver.center (IFC, glTF); importación IFC, DXF/DWG/TXT; cajetín y detalles de usuario DWG/DXF; generador de pórticos; integración CYPECAD/Arquímedes/CYPE Connect/Revit.
