# Nueva obra, asistente Obra vacía y Obra > Datos generales (T-RE-03)

Fuente: ejecución real de CYPE 3D v2027.a (capturas `docs/screenshots/orig_nueva_obra_*`, `orig_asistente_*`, `orig_obra_vacia_*`, `orig_datos_generales_01`). Todo lo siguiente está **[verificado en pantalla]** salvo lo marcado "no verificado". Todos los diálogos son ventanas propias dibujadas por el programa (marco rojo `#B22A20` aprox., título centrado, botón cerrar ×); no son diálogos Win32 estándar, salvo el selector de fichero (sí es el cuadro estándar de Windows).

## 1. Nueva obra (Archivo > Nuevo...)
Capturas: `orig_nueva_obra_01` (inicial), `_02` (Examinar), `_03`, `_04` (rellenada).
- Ventana 537x285. Grupo **Nombre de la obra**: ruta (texto, por defecto la última usada; instalación nueva `D:\CYPE Ingenieros\Proyectos\CYPE 3D\`), botón **Examinar**, campo **Nombre del fichero** (valor por defecto `nuevo`, preseleccionado, sufijo fijo `.ed3`). Grupo **Descripción**: campo de una línea, vacío.
- Botones: **Aceptar** (por defecto, foco inicial en el campo nombre), **Cancelar**.
- Examinar abre el cuadro estándar de Windows "Guardar" titulado "Nueva obra", filtro `Obra CYPE 3D (*.ed3)`, nombre `nuevo.ed3`; botones Guardar/Cancelar. Pulsar Enter con el nombre por defecto vuelve al diálogo con la ruta actualizada.
- Aceptar abre el selector de tipo (ver 2).

## 2. Selector de tipo de obra (mismo título "Nueva obra", 682x457)
Captura `orig_asistente_01`. Radio **Obra vacía** (por defecto, en negrita) / **Introducción automática IFC**. Imagen de una estructura de ejemplo (nave con cubierta y forjados). Botones Aceptar / Cancelar. Tras Aceptar el asistente tarda unos 3-5 s en abrirse.

## 3. Asistente "Obra vacía" (848x774, una sola página)
Captura general `orig_obra_vacia_01`. **No es un asistente por pasos**: toda la configuración está en una página; los botones inferiores son Cancelar / `< Anterior` (deshabilitado) / `Siguiente >` (deshabilitado) / **Terminar** (por defecto). En **Obra > Datos generales** se reutiliza exactamente la misma página con solo el botón **Aceptar** (captura `orig_datos_generales_01`).
Tras **Terminar** aparece el diálogo *Estados límite* (ver `dialogs_estados_limite.md`); Aceptar abre el editor. Esc en Estados límite vuelve al asistente (cancela).

### 3.1 Normas
Botón ancho con el resumen: "Eurocódigo 2, CTE DB SE-A, Código Estructural, CTE DB SE-M y Eurocódigo 9". Abre **Selección de normas** (418x247, `orig_obra_vacia_02_normas`): cinco combos con valores por defecto:
| Combo | Valor por defecto |
|---|---|
| Hormigón | Eurocódigo 2 (UE) |
| Acero conformado | CTE DB SE-A (España) |
| Acero laminado | Código Estructural (España) |
| Madera | CTE DB SE-M (España) |
| Aluminio | Eurocódigo 9 |
Botones laterales con flecha azul "C.E." y "CTE" (aplican de golpe el conjunto Código Estructural / CTE). Aceptar / Cancelar.
Listas completas (capturas `orig_obra_vacia_02_normas_*_p1.png`):
- Hormigón: Código Estructural (España), EHE-08, EHE-98-CTE, EHE-98, EH-91, REBAP (Portugal), Eurocódigo 2 (UE, España, Portugal, Francia, Bulgaria, Rumanía, Polonia, Eslovenia, Malasia, Singapur), SABS 0100, BAEL-91 (R-99), NTC 14-01-2008, BS 8110-1:1997, SP 63.13330.2012, ACI 318M-25/-19/-14/-11/-08/-99 (USA), NTC:2023, NTCRC:2017, NTCRC:2004, NTCRC (México), ABNT NBR 6118:2026/2023/2014/2007/2003, NB-1 (Brasil), CIRSOC 201-2005/1982, NCh430.Of2008 D°60:2011, ACI 318-99 (Chile), NTE E.060:2009, NSR-10, NB 1225001:2020 y :2017, CBH 87, IS 456:2000.
- Acero conformado: CTE DB SE-A, EAE 2011, EA-95 (MV110), MV110 (Portugal), Eurocódigos 3 y 4 (UE, España, Portugal, Francia, Italia, Bulgaria, Eslovenia, Rumanía, Reino Unido, Malasia, Singapur), NTC 14-01-2008, BS 5950-1:1998, IS 801:1975, DIN 18800:2008-11, AISI S100-2016/2007 (LRFD) (USA y México), AISI (USA), CAN/CSA S136-16/-07, ABNT NBR 14762:2010, NBR 14762:2001, AISI (Brasil), NCh427, SANS 10162-2:2011, AS 4600:2005, NZS 4600:2005.
- Acero laminado: Código Estructural, EAE 2011, CTE DB SE-A, EA-95 (MV103), REAE, Eurocódigos 3 y 4 (UE, España, Portugal, Francia, Italia, Reino Unido, Malasia, Singapur, Eslovenia, Rumanía), NTC 14-01-2008, BS 5950-1:2000, DIN 18800:2008-11, ANSI/AISC 360-22/-16/-10/-05 (LRFD), AISC LRFD 86, AISC ASD 89, CAN/CSA S16-01, NTC-DCEA 2020 (México), NTCRC Estruct.Metal., NBR8800, ABNT NBR 8800:2008 y :2024, NCh427, IS 800:2007, SANS 10162-1:2011, NTC-DCEA 2023, NTC-2018 (Italia).
- Madera: CTE DB SE-M (España), NBR 7190, Eurocódigo 5 (UE, Francia, Bélgica), ANSI/AWC NDS-2015 (ASD), CIRSOC 601.
- Aluminio: Eurocódigo 9.

### 3.2 Perfiles
- Icono **fy** (arriba derecha de la cabecera): *Biblioteca de aceros de usuario* (478x245, `orig_obra_vacia_06_fy`): dos paneles "Acero laminado" y "Acero conformado", cada uno con barra de herramientas (+ nuevo, editar, duplicar, subir, bajar, importar, exportar) y tabla (Tipo | Referencia | Exportar), vacía por defecto; Aceptar/Cancelar.
- **Acero laminado**: combo, defecto `S275 (UNE-EN 10025-2)`. Lista (con scroll): S235, S275, S355, S450, S275M, S355M, S420M, S460M, S235/S275/S355/S450 (UNE-EN 10025-2), S275M…S460M (UNE-EN 10025-4), S355 HISTAR®, S460 HISTAR® (parcial; ver capturas `_laminado_p1` y `_laminado_top_p1`; lista completa no verificada).
- **Acero conformado**: combo, defecto `S235`; lista: S235, S275, S355, S420.
- **Madera**: botón con texto "Aserrada, procedente de coníferas o ..." abre *Madera* (492x285, `orig_obra_vacia_04_madera`): grupo Material (combo "Aserrada, procedente de coníferas o chopos" + combo **Clase resistente** defecto `C24`); grupo **Clase de servicio** radios 1 (defecto) / 2 / 3 con panel de texto explicativo ("Clase de servicio 1: se caracteriza por un contenido de humedad ... 65% unas pocas semanas al año"). Aceptar / Cancelar.
- **Aluminio**: botón "EN AW-5083 - F" abre *Aluminio* (308x321, `_05_aluminio`): radios Aleación EN AW-5083 (defecto), 6060, 6061, 6063, 6005A, 6106, 6082, 7020; radios **Estado de entrega** F (defecto), O/H111, H112.
- **Hormigón** (perfiles de hormigón): combo, defecto `C25/30`. Lista con scroll empieza en C12/15 y llega hasta `Bc 60` (incluye C25/30, C30/37, C35/45, C40/50, C45/55, C50/60, Bc 15, Bc 20, Bc 25 ...). Lista intermedia completa no verificada.

### 3.3 Hormigón armado (columna derecha)
- Combos **Hormigón para pilares / vigas de forjado / elementos de cimentación**: defecto `C25/30` (misma lista que Perfiles > Hormigón).
- **Acero de barras**: botón "S-400" abre *Acero de barras* (277x480, `_07`): tres grupos con combos, todos `S-400` por defecto: Pilares (Armadura longitudinal, transversal), Vigas (superior de montaje, superior de refuerzo, inferior, transversal, de piel), Cimentación (Zapatas, Encepados, Vigas centradoras y de atado).
- **Características del árido**: botón "Cuarcita (15 mm), 30 mm" abre (241x397, `_08`): Pilares / Vigas de forjado / Vigas de cimentación, cada uno con combo **Naturaleza** (defecto Cuarcita) y **Tamaño máximo** (defecto 15 mm); más **Tamaño máximo de árido** de Cimentación = 30 mm.
- **Recubrimientos**: abre menú de tres botones (Pilares, Vigas, Cimentación; 180x191). Subdiálogos:
  - Pilares (404x384, `_09a`): Recubrimiento **3.0 cm** (distancia de la cara del pilar a la cara de la barra del estribo), dibujo explicativo.
  - Vigas (398x468, `_09b`): superior 3.0, inferior 3.0, lateral 3.0 cm.
  - Cimentación (398x350, `_09c`): Zapatas y encepados: superior 4.00, inferior 4.00, lateral 7.50 cm; Vigas centradoras y de atado: superior 5.00, inferior 5.00, lateral 5.00 cm.
  - Los tres tienen la casilla "Grabar como opciones por defecto" (desmarcada) y el botón **Valores de instalación**.
- **Mermas de acero** (398x266, `_10`): Pilares, Vigas, Zapatas, Encepados, Vigas centradoras y de atado = **10.00 %** cada una; casilla "Grabar como opciones por defecto", botón Valores de instalación, Aceptar/Cancelar. Foco inicial en el primer campo (texto seleccionado).

### 3.4 Acciones (columna izquierda)
- Casilla **Con sismo dinámico** (desmarcada).
- **Resistencia al fuego** (598x335, `_11`): pestañas Acero / Madera; radios: "No considerar la situación de incendio" (defecto), "Comprobar la resistencia al fuego (Código Estructural)", "Calcular la temperatura crítica en la situación de incendio". Icono de libro (ayuda) en la barra de título.
- **Estados límite (combinaciones)**: ver `dialogs_estados_limite.md`.
- **Hipótesis adicionales** (280x443, `_13` y `orig_carga_hipotesis_adicionales_02`): grupo "Categorías de uso" ("No hay categorías de uso definidas" + botón de edición) y grupo "Acciones" con tabla de columnas Automáticas / Adicionales: Peso propio (1 / ---), Cargas muertas, Sobrecarga de uso, Temperatura, Retracción, Viento, Sismo, Nieve, Empujes del terreno, Accidental (automáticas "-", adicionales 0 por defecto) cada una con botón de edición. Ver `dialogs_carga.md` para el alta de hipótesis.
- **Cimentación** (253x136, `_24`): casillas "Considerar combinaciones con viento" y "Considerar combinaciones con sismo", ambas marcadas por defecto.
- **Proceso constructivo** (398x152, `_15`): diálogo pequeño (contenido no analizado en detalle; ver captura).

### 3.5 Opciones
Cuatro botones que abren menús de opciones (todos con solo botón Aceptar):
- **Pilares** (575x372): lista izquierda con 4 páginas. *Tablas de armado* (botón Tablas de armado); *Dimensionamiento/Comprobación* (Reducción de longitudes de anclaje en pilares, Criterios de simetría, Criterios de continuidad, Pilares metálicos, Ángulo de inclinación de las bielas de compresión, Tolerancias en comprobaciones de separación, Comprobación de capacidad en pilares por flexión); *Disposiciones de armado* (Disposición de barras, Cortar esperas en último tramo, Transiciones por cambio de dimensiones, Redondeo de longitud de barras, Solapar en la zona central del tramo, Opciones de arranques, Opciones para armado de estribos, Recubrimientos); *Esfuerzos* (Coeficiente de rigidez axil, de torsión, a flexión). Capturas `_16`, `_16a`, `_16b`, `_16c`. Los diálogos hoja de estos botones no se abrieron (hueco).
- **Cimentación** (150x158): botones Opciones, Tablas de armado.
- **Vigas** (223x257): Opciones, Valoración de errores, Límites de flecha, Numeración de vigas continuas, Numeración de vigas.
- **Uniones** (117x158): Materiales, Opciones.

### 3.6 Ambiente
Grupo **Vigas**: botón "X0" abre *Vigas* (442x385, `_20`): grupo **Clase de exposición** (radios: Sin riesgo de ataque por corrosión [defecto], Corrosión provocada por carbonatación, por cloruros, inducida por cloruros de agua marina, Ataque por congelación y deshielo, Ataque químico); grupo **Designación** (radio X0 con texto explicativo); grupo **Fisuración** ("Abertura máxima de fisura 0.40 mm", sólo lectura). Aceptar/Cancelar.

### 3.7 Terreno de cimentación
- Casilla **Verificar deslizamiento de zapatas** (desmarcada).
- **Tensiones admisibles del terreno**: "Situaciones persistentes (A2 - Conjunto C)" = **0.200 MPa**; "Situaciones sísmicas y accidentales (A2 - Conjunto C)" = **0.300 MPa**. Botón flecha azul abre *Importar valores usuales de proyecto* (530x296, `_23`): tabla Tipo de terreno | S1 (C) (MPa) | S2 (C) (MPa): Grava 0.294/0.412; Arena densa 0.245/0.343; Arena semidensa 0.196/0.275; Arena suelta 0.098/0.137; Limo 0.118/0.165; Arcilla dura 0.245/0.343; Arcilla semidura 0.147/0.206; Arcilla blanda 0.098/0.137.
- **Validación**: valor fuera de rango en Situaciones persistentes (probado con -1 + Tab) abre *Aviso* (411x162): "El valor de 'Situaciones persistentes (A2 - Conjunto C)' debe estar entre 0.001 y 50.000." (`orig_obra_vacia_25_valid_tension`); el campo recupera el valor anterior. El mensaje es modal y se cierra con Aceptar/Enter.

### 3.8 Pilotes (pestañas "Pilotes (Hormigón armado)" y "Micropilotes")
Pilotes (HA): Excentricidad para el momento mínimo **0.10 m**; casilla "Considerar esfuerzos con un axil reducido" **marcada**; "Pilotes que trabajan por fuste" **0.00 · Axil**; "Pilotes que trabajan solo por punta" **0.80 · Axil**; Coeficiente de pandeo **1.00**; casilla "Análisis de la estabilidad lateral del pilote" desmarcada; botón **Configuración avanzada** (556x786, `_21`): pestañas Hundimiento / Comprobación estructural / Tope estructural (casilla); "Cálculo de resistencia" combo (Automático); conjunto A2 - Conjunto C con Referencia `A2 + M2 + R3`; coeficientes parciales terreno: ángulo de rozamiento interno 1.25, cohesión c' 1.25, resistencia al corte sin drenaje Cu 1.40, compresión simple qu 1.40, peso específico γ 1.00; pilote (parámetros geotécnicos): Punta γb 1.00, Fuste (compresión) γf,s 1.00, Total (compresión) γt 1.00, Fuste (tracción) γft,s 1.10; pilote (resistencias unitarias): mismos 1.00/1.00/1.00/1.10; Factor de eficiencia combo "Aplicar la fórmula de Converse-Labarre"; casilla "No considerar el factor de eficiencia si la distancia libre entre pilotes es igual o superior a K·D" (marcada) con K = 2.00; casilla "Grabar como opciones por defecto", Valores de instalación, Aceptar/Cancelar.
Micropilotes (`orig_obra_vacia_22_micropilotes`): casilla axil reducido (marcada) + campo 0.00; Análisis de pandeo combo "Coeficiente de pandeo" + 1.00; casilla estabilidad lateral del micropilote; "Reducción de espesor de la armadura por efecto de la corrosión" 0.60 mm; "Coeficiente de minoración del área de la armadura tubular" 1.00; "Coeficiente de influencia del tipo de ejecución" 1.00 (cada uno con flecha azul de valores por defecto); botón Configuración avanzada (no abierto).

## Comportamiento observado
- Los combos se abren como ventana emergente sin título; Esc cierra la lista **conservando la opción resaltada** (no la revierte). Teclas Inicio/Fin cambian la selección.
- Los diálogos modales anidados (hasta 3 niveles) se cierran con Esc = Cancelar.

## Huecos
Orden de tabulación (no medido); lista completa de Acero laminado y Hormigón; subdiálogos de Opciones (Pilares/Vigas/Uniones/Cimentación), Proceso constructivo (contenido), Configuración avanzada de micropilotes, pestañas Comprobación estructural/Tope estructural de pilotes; efecto de marcar "Con sismo dinámico"; opción IFC.
