# Geometría > Barras (T-P5-03): hallazgos de código (REA), pasada 1

Estado: pasada REA terminada; NO se ha lanzado ninguna aplicación. Etiquetas: [cadena] = texto UTF-16 `<id>texto` literal; [código] = nombre de procedimiento / pseudocódigo; «no verificado» = deducción o ausencia de evidencia.
Fuentes: `ed3dlib.dll` (ids 33xxx-65xxx y 372xxx-402xxx; volcado propio de todos los idiomas, es-ES elegido a mano), `enrcppan.dll` (panel de entrada dinámica, ids 62169-62180, 62218) y `entopane.dll` (textos de ayuda/estado genéricos 62066-62091). Las tres tablas son del mismo formato `<id>texto`. Comprobado con búsqueda binaria UTF-16 que `<64616>`, `<64882>` y `<64743>` solo existen en `ed3dlib.dll`.

## 1. Grupo «Barras» de la pestaña Geometría

Los rótulos cortos de botón vienen de tablas de etiquetas genéricas, no de tooltips de comando: #50415 «Barra» (otros idiomas: Pręt, Barre, Asta, Bar, Stab), #30203 «Pilar», #29511 «Viga». La jerarquía (grupo «Barras» con Barra / Pilar / Viga / Pieza / Sección / Material / Borrar / Disposición / Mover extremo / Invertir eje 'x' / Información / Agrupar / Desagrupar / Edición de pilares / de vigas) viene de la captura 07 y de geometria.md; la estructura del ribbon está construida en código y no se pudo leer como datos. NO existe una cadena «Barra genérica» como nombre de botón: «genérica(s)» aparece solo como tipo de elemento (ver §4).

| Botón | Descripción (tooltip) | Pasos / ayuda |
|---|---|---|
| Barra | #64616 «Introducción de una nueva barra en la estructura.\nLa barra queda definida por sus nudos inicial y final, por lo que, para introducirla, deberá seleccionar dos nudos de la estructura.» | Dos nudos existentes (inicial, final). Etiquetas de campo asociadas: #33765 «Nudo inicial», #33766 «Nudo final». Cadena de estado genérica de entopane: #62066 «Introduzca un punto», #62068 «Seleccione un elemento» (cuál usa cada paso: no verificado; la barra de estado real se debe medir). |
| Pilar | #64850 «Permite introducir un pilar indicando la posición en planta y el nivel inicial y final del mismo. El programa creará una pieza entre cada par de niveles que recorra el pilar.» | Requiere ≥ 2 niveles: #65140 «Para poder introducir pilares con esta opción debe definir previamente al menos 2 niveles en la obra.» Niveles: #64844-64846 («Los pilares deben comenzar y terminar en dos niveles diferentes (pueden hacerlo con un desnivel respecto a ellos). No es necesario definir niveles para introducir vigas.»). Confirmación posible: #65127 «¿Desea crear el pilar?» (#65126 «¿Desea crear la pieza?» para Pieza). |
| Viga | #64882 «Permite introducir una nueva viga. Si introduce más de una de forma consecutiva el programa creará, si es posible, una viga continua con todas las introducidas. Cada una de las vigas tiene su propio grupo de flecha.» | #64883 «Introduzca con el botón izquierdo del ratón los nudos que definen la viga. El programa creará una barra entre cada par de nudos. Pulse el botón derecho del ratón para finalizar la introducción.» (el único flujo de comando de Barras con texto de pasos completo). |
| Pieza | #64624 «Permite describir conjuntamente un grupo de barras alineadas como una única pieza.» | #64625 «Seleccione el nudo inicial de la primera barra y el nudo final de la última barra de las que desea crear una pieza.» (no es de esta tarea, pero comparte el modelo barra/pieza). |
| Borrar | #64617 «Permite eliminar una barra ya introducida.» | #64618 «Seleccione las barras que desea borrar y pulse el botón derecho del ratón.» #64619: tras borrar una barra permanecen los nudos; #64620 sugiere «Nudos - Borrar». |
| Sección / Material / Disposición | #64621 «Permite seleccionar el tipo de elemento estructural, el material y la geometría para un grupo de barras.»; #64623 material; #64622 «ángulo de giro de la sección transversal… así como su disposición (enrase o desplazamiento transversal)» | #65390 «Describir sección», #64339 «Describir perfil», #64340 «Describir material», #65308 «Describir disposición». |
| Mover extremo | #64645 «Permite mover el extremo de una barra a una nueva posición.» | #65385/65386 (referencia/destino), #65388 (ver geometria.md). |
| Invertir eje 'x' | #64640 «Al pulsar sobre una barra con esta opción activada, se invierte la dirección del eje local X de la barra. De esta forma puede conseguir disposiciones simétricas…» | #64641: el cambio invierte origen y final de la barra y el programa reajusta coeficientes de empotramiento y cargas para que no cambien. |
| Información | #64753 «Muestra en pantalla los desplazamientos y giros de los dos nudos de una barra.» | – |

Otros comandos con «Crear pilar continuo» #64852 / «Crear viga continua» #64862 (#64863 «Seleccione las vigas que compondrán la nueva viga continua y pulse el botón derecho del ratón.», #64854 «Seleccione el nudo inicial de la primera pieza y el nudo final de la última pieza que desea que formen el pilar.») pertenecen a «Edición de pilares/vigas» (agrupar piezas ya existentes), NO al botón Pilar/Viga. Corrección respecto a geometria.md, que citaba #64852 como el botón Pilar.

## 2. Panel de introducción dinámica (TK-040)

Cadenas [cadena] de `enrcppan.dll` (es-ES), que son los iconos/campos del panel que el original muestra cerca del punto clicado:
| Id | Texto (tooltip del icono) |
|---|---|
| #62218 | «Distancia» (también ed3dlib) |
| #157223 | «Longitud» (ed3dlib) |
| #62175 / #62176 | «Desplazamiento XY absoluto» / «Desplazamiento XY relativo» |
| #62173 / #62174 | «Longitud y ángulo de salida absoluto» / «Longitud y ángulo de salida relativo» |
| #62177 / #62178 | «Ángulo de salida absoluto» / «Ángulo de salida relativo» |
| #157018, #358478 | «Ángulo» (rótulos de campo) |
| #360959 / #360960 | «Ángulo absoluto» / «Ángulo relativo» (solo existen en pl/tr: la versión es-ES no se localizó en esa DLL; no verificado) |
| #219300 / #219301 / #219302 | «Coordenada absoluta» / «Coordenada relativa al punto actual» / «Coordenada relativa al punto anterior» (el es-ES solo está confirmado indirectamente; sí aparece en pl: «Współrzędna bezwzględna…») |
| #266839 | «Relativo» |
| #74975 | «Introducción por coordenadas» |
| #74981 / #74982 | «Coordenada X» / «Coordenada Y» (confirmado en pl; Z no hallado, no verificado) |
| #62123 | «Introduzca el desplazamiento en metros» |
| #62124 | «No se permite trabajar en coordenadas superiores a %s» |
| #383591 | «El punto introducido está fuera de los límites de la vista» |
| #62169 / #62170 | «Borrar el último punto introducido» / «Borrar todos los puntos introducidos» |
| #62157 / #62164 | «Dibujar la acotación» / «Si selecciona esta opción, se mostrará la acotación al introducir cada elemento.» (#62158 «Permite acotar al introducir cada elemento.») |
| #178757 | «Forzar la captura a la rejilla» (#62161: «Forzar la posición del cursor sobre la malla de puntos definida mediante la opción '%s'.») |
| #62143 / #62168 | «Ortogonal» / «Ortogonalidad» |
| #383176 | Opción «Forzar nivel/plano de trabajo»: al capturar un componente del modelo se proyecta su posición sobre el plano de trabajo (título exacto del botón en es-ES no verificado) |

Coherencia con lo medido en nudos.md §9.1: los 8 tooltips de Mover (Distancia/Longitud, Desplazamiento XY abs/rel, Longitud y ángulo de salida abs/rel, Ángulo de salida abs/rel) son exactamente #62218, #157223, #62175, #62176, #62173, #62174, #62177, #62178. Esto confirma que el panel es una librería compartida (`enrcppan.dll`, panel de entrada del entorno 2D/3D, familia de procedimientos `enr*`) y que no es específico de Nudos: Barras lo reutiliza.

Textos de estado/ayuda genéricos de `entopane.dll` (la librería de pasos de comandos): #62066 «Introduzca un punto», #62067 «Introduzca el tercer punto», #62068 «Seleccione un elemento», #62069 «Seleccione una posición ó elemento para obtener sus coordenadas», #62070 «Si mueve el cursor alrededor de la posición seleccionada se muestran las distintas disposiciones posibles», #62071 «Seleccione un grupo de elementos», #62075 «Seleccione la nueva posición del elemento», #62076 «Seleccione el ángulo de giro», #62077 «Seleccione la posición del punto», #62081 «Sitúe el cursor sobre el elemento para obtener su descripción», #62082 «Actuar sobre los elementos seleccionados», #62083 «Si no hay ningún elemento seleccionado vuelve al paso anterior», #62084 «Si no hay ningún elemento seleccionado se cancela la acción», #62090 «Si no hay ningún elemento seleccionado, se finaliza la ejecución.», #62087 «Volver al paso anterior», #62085 «Aceptar y comenzar otra vez», #62086 «Comenzar otra vez», #62091 «Introduzca los puntos de la polilínea». Estas son las piezas de las que se ensamblan las barras de estado «- LMB: … - RMB: …» medidas en nudos.md (p. ej. «Volver al paso anterior», «Actuar sobre los elementos seleccionados»); los textos medidos «Seleccione un elemento.», «Introduzca un punto.», «Seleccione la nueva posición del elemento.», «Seleccione un grupo de elementos.» coinciden con #62068, #62066, #62075, #62071.

Qué NO responde el código: cuándo aparece cada icono en Barra/Pilar/Viga, campos concretos por comando, icono gráfico exacto y orden. Está en funciones de `enrcppan.dll` sin analizar (no se decompiló).

## 3. Flujo por comando

Verificado por texto:
- **Barra**: define por nudo inicial y final → dos selecciones de nudos existentes (#64616). Si «Generar nudos en puntos de corte» está activa (#64642 / #64638: «el programa generará automáticamente un nudo cada vez que defina una barra y se detecte que se produce un cruce con otra previamente existente»), se crean nudos en los cruces (y por tanto las barras cruzadas se dividen; detalle de división no leído).
- **Viga**: LMB repetido sobre nudos; entre cada par consecutivo se crea una barra; RMB finaliza (#64883). Varias vigas consecutivas forman una viga continua si es posible (#64882). Cada viga tiene grupo de flecha propio.
- **Pilar**: posición en planta + nivel inicial + nivel final; se crea una pieza por cada par de niveles recorridos (#64850). Sin ≥ 2 niveles definidos no se puede (#65140). Los pilares son verticales con eje x local ascendente (#64985-64988: «La pieza debe ser vertical, con su eje x local en dirección ascendente», «La pieza no está definida entre dos niveles»).
- Pilar y viga son tipos de elemento estructural «pilar»/«viga» (#64022/#64023 usan «tipo '%s'»). La viga es una barra no necesariamente horizontal: las vigas continuas toleran un ángulo máximo con el plano horizontal (#64021 «forma un ángulo con el plano horizontal superior a %s», valor del límite no leído) y su plano xz local debe ser vertical (#64026).
- Solapes/proximidad (avisos de comprobación): #400346 «Las barras '%s' y '%s' están solapadas.», #400347 «El nudo '%s' está demasiado próximo a la barra '%s' (distancia: %s).», #399480 «Los nudos %s y %s están demasiado próximos entre sí (distancia: %s).», #64000 «La longitud de la barra es inferior a la longitud mínima permitida.» (valor numérico no leído), #64502 (importación) «No es posible importar la barra debido a que ya existe otra entre las mismas coordenadas.» (indica que en importación se rechaza barra duplicada entre mismos extremos; en la introducción manual no verificado).
- Borrar un nudo intermedio entre dos barras alineadas fusiona las barras (#64750); borrar barra deja los nudos (#64619).
- Esc / RMB: por convención de la librería de pasos: RMB = «Volver al paso anterior» (#62087) o finalizar/«Actuar sobre los elementos seleccionados» (#62082) según el paso; Viga: RMB finaliza (#64883). Esc: no hay cadena; lo medido en nudos.md §9.2 (Esc sale al estado «Seleccione una opción del menú.»).

## 4. Propiedades de una barra nueva

Evidencia directa:
- **Sin describir**: #65277 «Existen barras sin describir.» y #65174 «Alguna de las piezas que acometen al nudo no está descrita.» Una barra recién introducida no tiene sección/material asignados; hay que usar «Describir sección/perfil/material» (#65390, #64339, #64340). Se mostrará como barra sin descripción (color de dibujo: no verificado). Esto encaja con lo medido en nudos.md (el tooltip de barras describe «IPE 80 (S275 (UNE-EN 10025-2))» solo cuando está descrita).
- Tipos de elemento estructural (#64621, #65228): al menos «genérica» (barras genéricas #65378 «Barras genéricas»; perfil genérico #65223/#65242; «no se realiza ninguna comprobación de resistencia sobre las barras genéricas» #65224; barras genéricas no se asocian a grupos de combinaciones #64572/#64573), «hormigón» (#65226 «Barra de hormigón», #65225), «acero laminado/conformado/armado» (#64025; funciones `ed3c3d_append_pieza_acero_laminado/_conformado/_armado`), madera (#65376, `ed3c3d_append_pieza_madera`), aluminio (`ed3aluminio_crea_por_defecto`; ES del rótulo no verificado), hormigón rectangular/circular/T/L (funciones `ed3c3d_append_pieza_hormigon_*`), «pilar» (`ed3c3d_append_pieza_pilar`). Es decir, el modelo de datos del original distingue barra (entre dos nudos) de pieza (agrupación de barras alineadas) y tipo de pieza (#64552 «Tipos de pieza»).
- **Coeficientes de empotramiento**: #64632 «Por defecto, el programa asigna a cada barra coeficientes de empotramiento unidad.» Rango entre «articulado» y «empotrado» (#64631). Es decir, extremos empotrados por defecto (vinculación interior rígida).
- **Grupo de flecha**: #64636 «Por defecto, cada barra constituye, por sí misma, un grupo de flecha del tipo secante.»
- **Cargas superficiales**: #64684 «Por defecto, todas las barras (excepto los tirantes) pueden soportar cargas superficiales.»
- **Peso propio**: #64002 menciona «generación automática de las cargas de peso propio para esta barra» desactivable por barra (activada por defecto, no verificado).
- **Disposición (giro y enrase)**: desplazamientos en ejes locales y globales (iguales o distintos en origen y extremo: #65015 «Iguales en los dos extremos» / #65016), giro en el plano local yz y punto de enrase del perfil (#64140; #65014 «coordenadas Y y Z del punto de enrase… respecto al sistema definido por los ejes locales»; #65017 desplazamientos globales X, Y, Z de los extremos respecto de los nudos de definición). El algoritmo de posición (#64142): partir de la «pieza de definición» (línea que une los dos nudos), aplicar los desplazamientos globales en ambos extremos, calcular ejes locales con el giro del perfil y aplicar los desplazamientos locales. El valor por defecto del giro y enrase no se ha leído (se espera 0 y enrase al centro de gravedad; no verificado). #64488 «Calcular el ángulo de giro relativo al plano de la ventana activa» y #64486 «Ángulo de giro definido por el usuario», #64487 «Giro relativo al plano de dibujo».
- **Ejes locales**: se dibujan solo si se activa #64252 «Mostrar los ejes locales de los elementos» (#64253 «Permite activar y desactivar el dibujo en pantalla de los ejes locales de los elementos.»; título de la vista #64212 «Ejes locales de la barra»). Por defecto desactivado (no verificado). Eje x local = directriz del nudo inicial al final; #64640 invertirlo intercambia extremos. Pilar: x local ascendente (#64988).
- **Longitud de pandeo**: #46976 «Longitud de pandeo», #64628 «Asignación o cálculo de los coeficientes de esbeltez para el cálculo a pandeo de las barras.», #64080 definición, #64079 «donde 'L' es la longitud de la barra, y '%s' el coeficiente de pandeo». Valor por defecto del coeficiente (no hay cadena): no verificado. #378433 / #378435: actualización automática de coeficientes de pandeo.
- **Dibujo**: #64683 «Las barras desactivadas se visualizan en líneas de trazos»; #184938 «Las barras desconectadas se visualizan como líneas discontinuas»; #64493 «Representar la estructura usando los colores de las capas» (opción); capas: #372863 «Nueva capa», #381052 «Nom de la capa», #381053 «Capas de visualización»: en nudos.md se midió «Capa : Nueva capa 2»: las barras nuevas se asignan a la capa activa. Colores y grosor exactos: no hay cadena (usar medición en capturas: rojo/negro en nudos.md §9.2, celeste al resaltar, negro «goma» al mover).
- **Tooltip de barra** (medido en nudos.md): «Barra N2/N3: L. 4 m» + «Capa : …» + sección; #64570 «L. » es el prefijo «longitud».

## 5. Estructura de datos en el .ed3 y en la API del DLL

- .ed3: no abierto en esta pasada. De nudos.md §9.5: nudos referencian tres líneas de referencia por ids y «referencias a barras/vínculos» como `ff ff ff ff` (-1). Registros de barra sin decodificar.
- API exportada de `ed3dlib.dll` (los nombres de procedimiento sí están conservados, p. ej. `ed3c3d_append_pieza_acero_laminado` 0x1800f4800, `ed3c3d_append_pieza_pilar` 0x1800f56e0, `ed3genport_append_barra` 0x1802f4bb0, `ed3import_se_anyade_barra` 0x18032d330, `ed3obra_borra_pilares` 0x180452dc0, `ed3planta_crea`, `ed3secimp_crea_sin_definir` 0x18060a2c0 / `ed3secimp_crea_definida` 0x18060a1d0 (significado de «secimp» deducido, no verificado: podría ser «sección impuesta»; el par sin_definir/definida encaja con «barra sin describir»). Los tipos de pieza crean con `FUN_1800fb1e0` → `FUN_1800fe640` (parámetros: 2 ids, vectores de desplazamiento, 3 `short`, etc.; 25 parámetros): constructor de pieza de acero laminado con nudos inicial/final, perfil, material, giro, enrase y vinculación. No se decompiló el cuerpo en profundidad (cadena de llamadas muy larga y sin nombres), por lo que los campos con nombre y los valores por defecto no se leyeron.
- `ed3import_se_anyade_barra` (0x18032d330): comprueba con `FUN_18032e230` si procede añadir la barra (extremos válidos) y, si falla, emite el mensaje local de error (#64502 probable) y devuelve false; si procede, crea un array con 1 id (`ebaarray_CreaULong_Con1Elemento`), crea la pieza (`FUN_18046a550`) y la rellena (`FUN_18032ddf0`, `FUN_18032dcc0`) y reajusta (`FUN_180476ac0`): es el camino de importación de texto/DXF, no el de la UI.

## 6. Lo que REA no pudo responder (medir en la app original, con OK del usuario)

1. Barra de estado exacta de Barra, Pilar y Viga en cada paso (texto «Barras - Barra | - LMB: …») y qué se muestra tras el primer clic; Esc y RMB en cada paso.
2. Qué iconos/campos del panel dinámico aparecen en Barra, Pilar y Viga (¿longitud, ángulo, distancia?), icono marcado por defecto y comportamiento tras Intro; si el panel de Barra se abre al clic en vacío en 3D.
3. Cómo se elige el nivel inicial/final del Pilar (¿barra de estado, desplegable, diálogo?), qué pasa sin niveles, si el Pilar crea nudos en cada nivel y la longitud/altura por defecto.
4. Creación automática de nudos (de la barra a un punto sin nudo), división de barras al cruzarse (con «Generar nudos en puntos de corte» activa y desactivada), barra duplicada o de longitud mínima (valor de #64000), mensajes.
5. Sección/material/disposición/ejes locales/vinculación/pandeo por defecto de la barra nueva y su aspecto «sin describir» (color, grosor); valor del giro y enrase.
6. Ángulo máximo de viga con el plano horizontal (valor de #64021).
7. Campos de barra en el .ed3 (decodificar registro) y diferencia pieza/barra a nivel de fichero.
8. Qué ocurre con la numeración/capa activa y con vigas consecutivas que forman viga continua.

## 7. Recomendaciones de implementación

1. Implementar el panel de entrada dinámica como componente compartido (clase única) con 8 modos (Distancia, Longitud, Desplazamiento XY abs/rel, Longitud+ángulo abs/rel, Ángulo abs/rel) y textos exactos de §2; reutilizarlo en Nudos (TK-040), Barra, Pilar y Viga.
2. Modelo: barra = (nudo inicial, nudo final, capa) sin descripción por defecto; pieza = lista de barras alineadas; tipo de elemento estructural = {genérica, hormigón, acero laminado/conformado/armado, madera, aluminio, pilar, viga}. Mostrar «Existen barras sin describir» en la comprobación (#65277).
3. Viga: LMB sobre nudos encadena barras, RMB finaliza (#64883); crear viga continua si conectadas (#64020). Pilar: precondición ≥ 2 niveles (#65140), una barra por par de niveles, eje x ascendente.
4. Defectos con respaldo textual: empotramientos = 1 (#64632), grupo de flecha secante por barra (#64636), peso propio activo, admite cargas superficiales (#64684).
5. Respetar la fusión de barras alineadas al borrar un nudo (#64750) y el uso de «Generar nudos en puntos de corte» (#64638).
6. Reutilizar las cadenas de entopane (#62066-62091) para construir la barra de estado igual que el original y corregir geometria.md (Pilar = #64850, no #64852).
7. Dejar el aspecto «sin describir», colores y grosor como parámetros a calibrar tras medir en el original.
