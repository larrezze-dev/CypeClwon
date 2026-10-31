# Opciones del asistente y Configuración avanzada de pilotes (T-RE-05)

Capturado en el original con `tools/drive_original.ps1` el 2026-10-31. Capturas en `docs/screenshots/` con el prefijo indicado. [verificado]

## Pilotes - Configuración avanzada (556x786)
Tres pestañas: **Hundimiento** (ver `dialogs_nueva_obra_wizard.md` 3.8), **Comprobación estructural**, **Tope estructural** (la pestaña lleva una casilla, desmarcada).
Pie común: casilla "Grabar como opciones por defecto", botones Aceptar / Valores de instalación / Cancelar.

### Comprobación estructural (`orig_pilotes_ca_comprobacion_estructural`)
- Casilla **Considerar reducción de la resistencia del hormigón** (marcada) con icono de información (i azul).
- **Pilote prefabricado hincado**: combo **Relativa** + campo **1.00** + "· fc".
- **Pilote hormigonado in situ**: combo **Absoluta** + campo **18.00** + "MPa".
- Grupo "Coeficientes parciales de seguridad para el terreno (Curvas p-y)": **Resistencia al corte sin drenaje Cu = 1.40**, **Peso específico γ = 1.00**.
- Lista completa de los combos Relativa/Absoluta: no abierta (hueco).

### Tope estructural (`orig_pilotes_ca_tope_estructural`)
Panel vacío con la casilla desmarcada; no muestra contenido adicional con la casilla desmarcada. Con la casilla marcada: no probado (hueco).

## Micropilotes - Configuración avanzada (587x728, `orig_micropilotes_config_avanzada`)
Dos pestañas: Hundimiento y Comprobación estructural (esta segunda no abierta, hueco). Hundimiento es como la de pilotes con estas diferencias:
- Grupo "pilote (Parámetros geotécnicos)" pasa a **micropilote** y solo tiene **Fuste (Compresión) γf,s = 1.00** y **Fuste (Tracción) γft,s = 1.10**.
- Grupo "micropilote (Resistencias unitarias)": Punta γb 1.00, Fuste (Compresión) 1.00, Total (Compresión) γt 1.00, Fuste (Tracción) 1.10.
- Casilla K·D: "...distancia libre entre **micropilotes** es igual o superior a K·D" (marcada), K = 2.00. Factor de eficiencia: "Aplicar la fórmula de Converse-Labarre".
- Resto igual (Automático, A2 - Conjunto C, Referencia A2 + M2 + R3, coeficientes del terreno 1.25/1.25/1.40/1.40/1.00).

## Opciones > Cimentación
- Menú "Cimentación" (150x158): Opciones, Tablas de armado.
- **Opciones** (207x287, `orig_opc_cimentacion_opciones`): menú con 6 botones: Hormigón de limpieza, Comprobación de fisuración, Zapatas aisladas, Encepados, Vigas centradoras, Vigas de atado. Aceptar.
- **Tablas de armado** (165x155, `orig_opc_cimentacion_tablas_armado`): menú con 3 botones: Zapatas aisladas, Vigas centradoras, Vigas de atado. Aceptar.
- Los diálogos hoja de estos botones no se abrieron (hueco).

## Opciones > Vigas
- **Opciones** (641x537, `orig_opc_vigas_opciones`): lista izquierda con 6 páginas: *Tablas de armado en vigas*, *Disposiciones de armado*, *Esfuerzos*, *Dimensionamiento / Comprobación*, *Vigas metálicas*, *Planos*. Aceptar / Cancelar. Página 1 (botones): Armadura de montaje en secciones rectangulares; ... con un ala; ... con dos alas; Armadura transversal; Armadura portaestribos; Armadura de piel; Armadura superior de refuerzo; Armadura inferior de refuerzo. Páginas 2-6: no abiertas (hueco).
- **Valoración de errores** (437x514, `orig_opc_vigas_valoracion_errores`), casillas con valor por defecto:
  - *Vigas de hormigón*: Patilla por compresión [x]; Armadura compresión superior [x]; Armadura compresión inferior [x]; Armado manual de ménsula corta [x].
  - *Estribos*: Estribos de diámetro igual o superior a: [x] + combo Ø16; Celosía muy corta [x].
  - *Armados*: Anclaje de la armadura de piel [ ]; Tramo corto, longitud de barra menor que la longitud mínima de anclaje [x]; Desnivel que necesita detalle constructivo de continuidad de barras [x]; Longitud de la barra mayor que la longitud máxima [x]; Anclaje de la armadura de montaje [x]; Anclaje de la armadura superior [x]; Armadura superior en más de una capa [ ]; Anclaje de la armadura inferior [x]; Armadura inferior en más de una capa [ ].
  - Grabar como opciones por defecto, Aceptar / Valores de instalación / Cancelar.
- **Límites de flecha** (309x371, `orig_opc_vigas_limites_flecha`): solo lectura con botón de edición (icono de lápiz) por grupo: *Hormigón* (A plazo infinito (Cuasipermanente): L/ 250; Activa a largo plazo (Cuasipermanente): L/ 500), *Acero laminado* ("No se ha definido ningún límite de flecha."), *Acero conformado* (Instantánea de sobrecarga L/ 350; Instantánea total (Cuasipermanente) L/ 300; Activa a largo plazo (Característica) L/ 400), *Madera* (Instantánea de sobrecarga L/ 350; A plazo infinito (Cuasipermanente) L/ 300; Activa a largo plazo (Característica) L/ 400). Aceptar / Cancelar. Diálogos de edición: no abiertos (hueco).
- **Numeración de vigas continuas** (531x372, `orig_opc_vigas_numeracion_continuas`): "Referencia base" = **Pórtico 1**; tres grupos de radios (defecto el primero de cada grupo): primero pórticos paralelos al eje X global / al eje Y global; pórticos paralelos al eje X global en sentido positivo / negativo del eje Y global; pórticos paralelos al eje Y global en sentido positivo / negativo del eje X global. Grabar como opciones por defecto; Aceptar / Valores de instalación / Cancelar.
- **Numeración de vigas** (397x233, `orig_opc_vigas_numeracion`): radios **No numerar** (defecto), Numeración consecutiva de la obra, Numeración por niveles. Grabar como opciones por defecto; Aceptar / Valores de instalación / Cancelar.

## Opciones > Uniones
- **Materiales** (291x181, `orig_opc_uniones_materiales`): **Acero laminado** = S275 (UNE-EN 10025-2); **Acero de pernos** = B 400 S, Ys = 1.15; **Hormigón:** = C25/30. Aceptar / Cancelar.
- **Opciones** (671x460, `orig_opc_uniones_opciones`): pestañas **Tornillería no pretensada** (abierta), Tornillería pretensada, Rigidizadores, Placa de anclaje (no abiertas). Tornillería no pretensada: Series de tornillos ISO 4014 [x] ISO 4017 [x]; Clase de acero de los tornillos 4.6 5.6 6.8 **8.8** 10.9 (radios, defecto 8.8); Diámetros disponibles M12 [x] M16 [x] M20 [x] M24 [x] M30 M36 M42 M48 M56 M64 [ ]; "Utilizar diámetros no habituales" [ ]. Imagen de un tornillo a la izquierda. Grabar como opciones por defecto; Aceptar / Valores de instalación / Cancelar.

## Huecos
Opciones > Pilares (19 diálogos hoja), Cimentación (hojas de Opciones y Tablas de armado), Vigas (páginas 2-6 de Opciones; edición de Límites de flecha; Tablas de armado), Uniones (3 pestañas restantes), pestaña Comprobación estructural de micropilotes, listas de los combos Relativa/Absoluta, comportamiento de la casilla Tope estructural marcada.

## Capturas complementarias (2026-10-31, sesión 2) [verificado]
- **Combos Relativa/Absoluta** (`orig_pilotes_ca_combo_precast`, `..._insitu`): la lista se despliega con **Absoluta** y **Relativa** (en ese orden).
- **Icono (i) de Comprobación estructural** (`orig_pilotes_ca_info`): diálogo "Información" (650x218) con icono azul, botón Aceptar y el texto: "Es recomendable considerar una reducción en la resistencia característica del hormigón por razones constructivas relacionadas con la ejecución de los pilotes hormigonados in situ. Esta reducción se considera en la comprobación de los estados límite de agotamiento tanto frente a solicitaciones normales como de cortante."
- **Tope estructural con la casilla marcada** (`orig_pilotes_ca_tope_marcado`): aparecen **Pilote prefabricado hincado**: Relativa 0.30 · fc y **Pilote hormigonado in situ**: Absoluta 3.50 MPa.
- **Micropilotes - Comprobación estructural** (`orig_micropilotes_ca_comprobacion_estructural`): solo el grupo "Coeficientes parciales de seguridad para el terreno (Curvas p-y)": Cu 1.40 y γ 1.00.
