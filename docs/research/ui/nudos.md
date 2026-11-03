# Geometría > Nudos (T-P5-02): hallazgos de código (REA)

Estado: FINAL de la vía REA (la vía ejecución del original no se ha usado). Fuente: ed3dlib.dll (strings_ed3dlib.tsv) y REA/Ghidra. Etiquetas: [cadena], [recurso], [pseudocódigo]; "no verificado" indica deducción.

## 1. Botones del grupo Nudos [cadena]
| Botón | Id texto | Descripción |
|---|---|---|
| Nuevo | #64743 | "Introducir un nuevo nudo en la estructura." |
| Mover | #64744 | "Cambiar de posición un nudo, independientemente de si está acotado o no." Pasos: #64745 "Seleccione un nudo con el botón izquierdo del ratón. Después, mueva el puntero del ratón sin presionar ningún botón. Podrá observar que las barras que llegan al nudo se mueven con él."; #64746 "Seleccione la nueva posición del nudo que se desplaza, igual que si estuviera introduciendo uno nuevo."; #64747 "Si no desea moverlo, pulse el botón derecho del ratón y se cancelará la operación." |
| Borrar | #64748 | "Permite borrar un nudo individualmente o una selección de nudos." #64749 "Seleccione los nudos que desea borrar y, a continuación, pulse el botón derecho del ratón." #64750 "Al borrar un nudo también se borran todas las barras que están conectadas a dicho nudo. La única excepción ocurre cuando se borra un nudo intermedio entre dos barras alineadas: en éste caso, el programa crea una nueva barra resultante de la unión entre las dos anteriores." |

Otros textos relacionados: #65123 "Seleccione los nudos a desplazar" (mover elementos); #64336 "Nudo" (etiqueta); #38501 "Nudos"; #64773 "Nudos y barras"; #65291 "Referencias de los nudos"; #65418 "Editar nudos" (probablemente otro grupo, no verificado); #64619/#64620 (borrar una barra deja sus nudos; aconseja la opción "Nudos - Borrar" que borra nudo y barras).

## 2. Introducción de un nudo [cadena, ed3dlib.dll 0x180b44000]
Texto completo #64743 (versión EN, literal): "Enter a new node in the structure. In a 3D view, click, using the left mouse button, on a reference line or a bar. If the window corresponds to a 2D view, it is possible to click using the left mouse button on any point of the plane. Additionally, a node can be entered using cartesian coordinates by directly typing the coordinates on the calculator."
Es decir: en vista 3D el clic izquierdo debe ser sobre una línea de referencia o una barra (el nudo cae en esa línea/barra); en vista 2D (planta/alzado) vale cualquier punto del plano; además, por coordenadas tecleadas en la calculadora.

### Calculadora
Texto de ayuda: "Además, puede introducir un nudo por coordenadas cartesianas tecleando directamente sus coordenadas en la calculadora." (alterna a "Inserire un nuovo nodo nella struttura" en la tabla). El nudo se introduce como un punto cualquiera del visor (mismo mecanismo que "Barra": #33765 Nudo inicial / #33766 Nudo final). Detalles de la calculadora: ver sección 6 (pendiente).

## 3. Comportamiento [cadena]
- Líneas de referencia generadas al introducir nudos: #64734 "Opciones de visibilidad para las líneas de referencia generadas al introducir nuevos nudos en la estructura."; #64035 (agrupar visibilidad por grupo de nudos); #64241 "Activa o desactiva las líneas de referencia que no tienen ningún nudo."; #64236 "Sólo las que tienen nudo".
- Cruces: #64642 "Generar nudos en puntos de corte"; #64638 "Si activa esta opción, el programa generará automáticamente un nudo cada vez que defina una barra y se detecte que se produce un cruce con otra previamente existente." (afecta a la introducción de barras, no a Nudos > Nuevo).
- Nudo sin barra: #64010 "Se ha encontrado un nudo que no está conectado a ninguna barra ni a ninguna lámina." (error de cálculo). #400345 "El nudo no es necesario ya que no tiene cargas ni vinculación exterior." (aviso de nudo superfluo).
- Mensajes sin hallar uno específico de "nudo duplicado/coincidente": no verificado.

## 4. Serialización .ed3
No verificado por REA (no se ha abierto el formato; ver docs/research/ui/obra.md y el .ed3 de muestra en docs/sample_project).

## 5. Pseudocódigo / recursos
- [recurso] ed3dlib.dll no tiene sección PE de recursos (inspect_pe_resources: 0 recursos). No hay DIALOG/templates en esta DLL; las cadenas son literales UTF-16 "<id>texto" en .data (grupos por idioma).
- [cadena] El texto #64743 está en 0x180b44000; sus xrefs (29, entre 0x1803f6bf4 y 0x1803f70bc) pertenecen a FUN_1803f6ba0 (1346 bytes), una función de búsqueda/tabla de cadenas por id, no el manejador del comando. Los manejadores de Nuevo/Borrar/Mover no se localizan por xrefs de cadena porque los ids de comando se despachan por número, no por puntero a texto. No se han localizado ni leído funciones de Nuevo/Borrar/Mover: tolerancias y orden de comprobaciones NO VERIFICADOS.
- Nota: ed3dlib.dll tarda ~300 s en analizarse en Ghidra (hay que subir ghidra.startup_timeout_ms; ya puesto a 1200000 en esta sesión).

## 6. Lo que REA no responde (medir en el original, con OK del usuario)
1. Campos, formato y defaults de la calculadora (X/Y/Z, separadores, relativas/absolutas, teclas Enter/Esc), y su barra de estado/prompt durante Nuevo.
2. Snap exacto en 3D (línea de referencia vs barra), tolerancia de coincidencia, qué ocurre al crear un nudo en posición ya ocupada (aviso o ignorado).
3. Creación de líneas de referencia nuevas al introducir un nudo y su visibilidad por defecto (#64734: diálogo de opciones, campos no verificados).
4. Numeración, color y etiqueta de nudos (#64336 "Nudo") y color de nudo seleccionado/en movimiento.
5. Borrar: confirmación, fusión de barras alineadas (criterio de alineación), qué pasa con nudos con cargas/vínculos.
6. Mover: restricción a plano/eje, efecto sobre cotas (el original dice que mueve "esté acotado o no"), mensaje al mover sobre otro nudo.
7. Nombre de campos de nudo en el .ed3.

## 8. Medido en el original (2026-11-03, obra docs/sample_project/pl_base.ed3; capturas n0-n11, c1-c5 en docs/screenshots)

Ribbon Geometría > grupo Nudos (coordenadas de ventana maximizada 1920x1032): Nuevo (69,75), Borrar (123,68), Mover (123,97). El botón activo queda con el fondo azul claro (modo persistente hasta ESC / otro botón).

**Estructura de pl_base.ed3:** 4 nudos N1 (0,0,0 con apoyo), N2 (0,0,3), N3 y N4; los nudos se rotulan «N1», «N2»... en verde, a la derecha y encima del nudo, SOLO mientras hay un comando de Nudos activo (en la vista normal no hay rótulos).

**Nuevo**
- Barra de estado: «Nudos - Nuevo |  - LMB: Introduzca un punto.» (el 1.er segmento va en negrita-normal: «Nudos - Nuevo», barra vertical, texto).
- Un tooltip junto al cursor muestra las coordenadas con 3 decimales: «188.107, -185.671, 153.795 m» (fondo gris oscuro, texto blanco, posición según la opción «Posición de los tooltips»).
- Al pulsar una tecla numérica con el puntero en el visor se abre la ventana modal «Calculadora» (borde rojo, 322x286 px aprox.): campos «Coordenada X», «Coordenada Y», «Coordenada Z»; el campo activo se ve en gris; teclado numérico con 9 8 7 6 5 4 3 2 1 0 . =  + +/- MC - ??? M- * C M+ / AC MR, botón de retroceso y botón Intro (grande). La primera cifra pulsada se pasa a X.
- **Tab NO cambia de campo.** Las cifras se acumulan en el campo activo («5»,«6»,«7» seguidas dan 567). **Intro** confirma el campo y pasa X -> Y -> Z; el Intro del campo Z crea el nudo y cierra la calculadora (la calculadora vuelve a abrirse al teclear otra cifra; el comando sigue activo).
- Crear un nudo en (5,6,7) añade **líneas de referencia** nuevas por ese punto (naranja, ejes X, Y y Z) en la vista.
- Teclear dos veces el mismo punto: sin mensaje ni cambio visible (el segundo no añade nada).

**Borrar**
- Barra de estado: «Nudos - Borrar |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.»
- LMB sobre un nudo lo marca: cuadrado naranja y tooltip en la esquina superior derecha del visor «N2 (0, 0, 3)» (formato «N<id> (x, y, z)», coordenadas sin decimales si son enteras).
- RMB borra de inmediato, **sin confirmación**: se van el nudo, todas las barras conectadas, los rótulos «N»... y las líneas de referencia sobrantes que solo pasaban por él. Los nudos que quedan sin barra siguen existiendo (el apoyo N1 quedó dibujado).

**Borde rojo de los diálogos (pendiente del backlog):** medido sobre la ventana «Nueva obra» (537x285): 5 px a izquierda, derecha y abajo = 1 px #841E14 (132,30,20) + 4 px #B1291C (177,41,28); la barra de título es #B1291C de unos 31 px (1 px #841E14 arriba) con el rótulo blanco centrado y una cruz a la derecha; interior #F0F0F0.

**Pendiente de medir:** Mover (botón no probado; comportamiento esperado por 64745-64747), borrado de nudo intermedio con fusión de barras (obra sin esa geometría), clic con LMB en el visor para crear nudo (necesita snap a línea de referencia), longitud de las verticales de la rejilla (TK-038), campos de nudo en el .ed3 (guardar y leer).


## 9. Medido en el original, segunda pasada (2026-11-03; obras nm_base.ed3 = copia de pl_base, pl_grid4.ed3, NM_EMPTY.ED3; capturas docs/screenshots/nm_*)

Método: `tools/drive_original.ps1` con `fshot` (pantalla completa, para ver tooltips), obra abierta con `-AppArgs`. Pitfall nuevo: los pasos `click:x,y` son relativos a la ventana superior del proceso; con un diálogo abierto (asistente, Estados límite) hay que usar `aclick:x,y` (coordenadas absolutas de pantalla) y un primer clic en el título del diálogo para activarlo. Coordenadas en nm_base: N1 (0,0,0) apoyo, N2 (0,0,3), N3 (4,0,3), N4 (4,0,0) apoyo; barras N1/N2, N2/N3, N3/N4. El nudo «N» se numera por orden de creación (N5, N6...).

### 9.1 Panel de introducción dinámica (HALLAZGO IMPORTANTE, afecta a Nuevo y Mover)
En una vista 3D **el LMB sobre un punto «no definitivo» no fija el nudo**: abre un pequeño panel flotante (ventana propia con barra de título roja (como los diálogos) y cruz de cierre, ~250x120 px, junto al punto clicado; el campo tiene el foco con el valor seleccionado) donde se puede teclear un valor; **Intro confirma** (con el valor propuesto, que coincide con el punto clicado) y fija el nudo. El panel no sigue al ratón; Esc lo cierra sin fijar (el comando queda en la fase en que estaba); un clic fuera del panel lo descarta.
- Clic sobre **línea de referencia** o **barra** (con un nudo de referencia en ella): panel de **un solo icono «Distancia»** (tooltip «Distancia») + campo «Distancia ... m» (distancia a lo largo de la línea desde el nudo de referencia, 3 decimales). En Nuevo: Intro crea el nudo. Ejemplo medido: clic en la barra N2/N3 a 1.996 m -> panel «Distancia 1.996 m» -> Intro -> nudo N6 en (1.996, 0, 3) y **la barra se divide** en N2/N6 y N6/N3 (tooltip posterior «Barra N6/N3: L. 2.004 m»).
- En **Mover (fase 2)** el panel tiene **8 iconos** en 4 pares (tooltips literales): «Distancia» / «Longitud» (el segundo marcado por defecto), «Desplazamiento XY absoluto» / «Desplazamiento XY relativo», «Longitud y ángulo de salida absoluto» / «Longitud y ángulo de salida relativo» (el primero marcado), «Ángulo de salida absoluto» / «Ángulo de salida relativo»; campo «Longitud ... m» (p. ej. 2.343). Teclear «3» + Intro movió el nudo 3 m en la dirección del cursor (de (0,0,3) a (2.826, -1.006, 3.000), ángulo 340.40°): la longitud se mide en el plano horizontal del nudo (Z constante). Intro sin teclear fija el punto clicado ((2.207, -0.786, 3.000) para una longitud mostrada de 2.343).
- Clic sobre un **nudo existente** en Mover fase 2: **fija directamente sin panel** (ver 9.2). En Nuevo clic sobre un nudo existente: no hace nada.
- Clic sobre el **vacío**: en **Nuevo** no ocurre nada (ni nudo ni panel; Intro tampoco). En **Mover fase 2** abre el panel «Longitud» descrito arriba (punto sin snap).
- En una **obra sin nudos** (NM_EMPTY.ED3: solo las 3 líneas de referencia del origen, X, Y y Z): clic sobre una línea de referencia en Nuevo **crea el nudo de inmediato, sin panel** (no hay nudo de referencia para medir la distancia): clic sobre el eje Z vertical creó N1 en (0, 0, 2.486), es decir el punto del cursor proyectado sobre la línea (sin redondeo a la rejilla). Clic sobre el vacío: nada.

### 9.2 Mover
Barra de estado exacta (texto literal, con el primer segmento seguido de « | »):
- Fase 0 (activado, antes de mover el ratón; el tooltip de ayuda del botón es visible): «Nudos - Mover |  - LMB: Seleccione un elemento.» (el texto está ya al pulsar el botón).
- Fase 1 hasta elegir el nudo: la misma. Con el cursor en el visor aparece el tooltip de coordenadas junto al cursor (fondo gris oscuro, texto blanco, 3 decimales, p. ej. «186.554, -187.642, 153.283 m»; sin snap las coordenadas son las del rayo sobre un plano lejano; el tooltip va con ligero retardo respecto al cursor). Al pasar el cursor sobre una barra se resalta en celeste y aparece arriba a la derecha un tooltip de tres líneas: «Barra N2/N3: L. 4 m», «Capa : Nueva capa 2», «IPE 80 (S275 (UNE-EN 10025-2))» (en comandos de Barras: «Pieza (N2/N3): L. 4 m»).
- Tras el 1.er clic sobre un nudo: «Nudos - Mover |  - LMB: Seleccione la nueva posición del elemento. - RMB: Volver al paso anterior.»
- Selección: el nudo elegido se dibuja con **cuadrado naranja** y un **aspa naranja larga** (línea horizontal de ~130 px y vertical) centrada en él; el nudo en movimiento se muestra como cuadrado negro vacío en la posición del cursor.
- **Las barras siguen al cursor**: las barras que llegan al nudo se dibujan en **negro** (goma) hasta la posición actual; las originales (rojo/negro) siguen dibujadas hasta confirmar. Se muestra además una cota naranja con texto girado: «2.2789 m < 340.37°» (distancia horizontal al nudo original y ángulo respecto a +X, 2 decimales en el ángulo, 4 en la distancia), línea discontinua paralela y marcas de extremo.
- Sobre una línea de referencia (Z... a lo largo de ella) la línea se resalta en **celeste**, el cursor hace snap y aparece la cota «1.8652 m < 270°» (distancia al nudo y ángulo) con tooltip de coordenadas (0.000, -1.885, 3.000 m); en Nuevo la cota es azul claro «1.8652 m» con un marcador de reloj de arena en el punto de snap.
- **Rótulos «N»: NO se ven en Mover** (sí en Nuevo y Borrar). Tras Esc en Nuevo los rótulos siguieron visibles en la captura tomada 1.6 s después (probable retardo de repintado, no concluyente).
- **Fijar**: tras confirmar (Intro en el panel, o clic en un nudo) el comando **sigue activo en la fase 1** («...Seleccione un elemento.») y las líneas de referencia se recalculan (las de la posición antigua desaparecen si ya no pasa ningún nudo por ellas).
- **RMB**: en fase 2 «Volver al paso anterior»: vuelve a la fase 1 (barra «LMB: Seleccione un elemento.») dejando el nudo resaltado con tooltip «N2 (0, 0, 3)» arriba a la derecha; un segundo RMB en fase 1 no sale del comando (siguió «Nudos - Mover |  - LMB: Seleccione un elemento.» y se quitó el resaltado). **No** cancela el comando; la salida es Esc (barra «Seleccione una opción del menú.») o elegir otro botón.
- **Calculadora en fase 2: sí.** Pulsar una cifra con un nudo elegido abre «Calculadora» (X = cifra tecleada; **Y y Z se rellenan con las coordenadas actuales del nudo** (0 y 3); Intro avanza X -> Y -> Z y el Intro de Z mueve el nudo). Medido: teclear 5, 2, 7 movió el nudo a (5, 2, 7). En Nuevo la calculadora rellena Y y Z con el último punto (el del tooltip/nudo previo): con el cursor sobre (0, 0, 2.486) mostró «1 / 0 / 2.486».
- **Mover sobre otro nudo: sin mensaje**; los dos nudos se **fusionan** (el nudo movido desaparece: rótulos N1, N3, N4 tras mover N2 a N3). La barra N2/N3, que queda de longitud cero, **desaparece**; la barra N1/N2 pasa a ser N1/N3 (diagonal). El apoyo N1 se redibuja girado (símbolo de apoyo orientado según la barra).
- Mover un nudo sobre una barra: panel «Longitud» con cota vertical azul «1.4190 m» (distancia desde el nudo de la barra) además de la cota naranja polar «4.3011 m < 21.57°».

### 9.3 Borrar: fusión de barras alineadas
- Estado al pulsar el botón (antes de mover el ratón): «Nudos - Borrar |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.» (visible desde la activación). En Borrar el cursor **no muestra tooltip de coordenadas** (ni sobre el vacío ni sobre barras: no hay resalte de barras); solo, al seleccionar, el tooltip arriba a la derecha «N5 (1.996, 0, 3)». En Borrar las barras se dibujan más gruesas.
- Prueba: barra N2/N3 dividida con Nuevo en N5 (1.996, 0, 3); Borrar N5 + RMB -> **se crea una barra unión** y el nudo desaparece; sin confirmación. Al pasar el cursor en Nuevo, la barra resultante se llama **«Barra N2/N3: L. 4 m», «Capa : Nueva capa 2», «IPE 80 (S275 (UNE-EN 10025-2))»**: ids de nudos extremos N2/N3 (las barras no tienen número propio, se nombran por sus nudos), capa y perfil conservados (ambas mitades tenían la misma capa y sección; **no medido** qué ocurre con secciones/materiales distintos: el intento con Barras > Sección no abrió el diálogo con LMB + RMB en 2.5 s).
- Tooltip de pieza: tras dividir, el comando Barras > Sección muestra la unión de barras alineadas como una sola «Pieza (N2/N3): L. 4 m».

### 9.4 Nuevo: resumen de activación
- Al activar Nuevo (antes de mover el ratón) la barra de estado ya dice «Nudos - Nuevo |  - LMB: Introduzca un punto.» (se mantiene en todo el comando, también con el panel o la calculadora abiertos).
- El tooltip de coordenadas junto al cursor aparece en cuanto el cursor entra en el visor. **Esc lo oculta** (sale del comando: barra «Seleccione una opción del menú.» y el tooltip desaparece).
- Sin snap (vacío): coordenadas del tipo «186.164, -187.991, 153.333 m» (cualquier valor; clic sin efecto).
- Snap a línea de referencia: resalte celeste de la línea, marcador de reloj de arena, cota azul «1.8652 m», tooltip «0.000, -1.865, 3.000 m». Snap a barra: igual, cota «1.9957 m» (distancia desde el nudo inicial). Snap a nudo: se resalta la barra que llega al nudo, tooltip con las coordenadas del nudo, y el clic no crea nada.
- Crear con Intro del panel sobre línea de referencia (nudo N5 en (0, -1.865, 3)) añade además **líneas de referencia** por el nuevo nudo (naranja, 3 direcciones).

### 9.5 Serialización en .ed3 (obra nueva sin nudos NM_EMPTY.ED3, 786 512 bytes, frente a la misma obra con dos nudos creados con la calculadora (1,2,3) y (4,5,6), 790 144 bytes: +3 632 bytes)
Formato del original: binario propio (cabecera «CYPE-2027-a», cadenas UTF-16, secuencia de campos con tags de 1 byte y enteros de 32 bits little endian; texto «ESUL_01_x64», «Estructura»... cerca del final). Comparación byte a byte: solo difieren 7 bytes en la cabecera (zona de metadatos/aleatorios), 6 bytes de contadores (en 783957: `07 00 00 00 00 00` -> `0f 00 00 00 00 02`) y el **bloque de geometría** a partir del offset 784008 (2 504 -> 6 136 bytes); el resto (perfiles, normativa, combinaciones, cola ESUL) es idéntico.
- **Los nudos no guardan coordenadas propias: se definen por intersección de tres líneas de referencia** (una por eje). Las **líneas de referencia** se guardan en tres listas (X, Y, Z) con: contador `u32`, y por línea `03` + `id u32` + `double` (coordenada) + `00 00 01 01`. Obra vacía: una línea por eje en 0 (ids 0, 1, 2). Con dos nudos: X = {0, 1.0 (id 7), 4.0 (id 11)}, Y = {0, 2.0 (id 8), 5.0 (id 12)}, Z = {0, 3.0 (id 9), 6.0 (id 13)} (los dobles 0x3FF0...=1, 0x4000...=2, 0x4008...=3, 0x4010...=4, 0x4014...=5, 0x4018...=6).
- **Registro de nudo**: contador `u32` (=2), por nudo `08` + `id u32` (10 y 14: los ids de líneas y nudos salen de un contador común, los ids 3..6 y 10 se consumen por otros objetos) + campos de estado + **tres `u32` con los ids de las líneas de referencia X, Y, Z** (`07 08 09` para el primero, `0b 0c 0d` para el segundo) + referencias a barras/vínculos como `ff ff ff ff...` (-1 = sin enlace). Los apoyos y barras no se decodificaron.
- Consecuencias para el clon: (1) al borrar un nudo se retiran las líneas de referencia que solo él usaba (coincide con 8: «líneas de referencia sobrantes»); (2) las líneas de referencia son entidades con id (y flags, probablemente visibilidad y grupo) y el nudo las referencia. No se decodificaron los flags `01 01`, el bloque de 3x(8d 5a 91 07 d7 e8 c3 40 = 10193.679...) (extensión por defecto del dibujo de las líneas, posiblemente) ni los registros de barras.
- El original crea además un respaldo `nm_empty0.ed$` al guardar sobre un fichero existente.

### 9.6 TK-038: verticales (Z) de la rejilla
Obra pl_grid4.ed3 (rejilla 4x4 en 0, 3, 6, 9; niveles 0 y 3; nudos N1..N4 con z entre 0 y 3). Se añadieron con la calculadora N5 (0, 0, 7) y N6 (9, 9, -2).
- Antes (z entre 0 y 3): las verticales tienen 3 m (≈185 px frente a 185 px de la columna N1-N2), con una marca «x» en cada extremo.
- Después: la vertical pasada por el cursor (x = 4 de las líneas de los nudos, y = 3 de la rejilla) mide **9 m**: **de z = -2 a z = 7**, o sea de la **mínima a la máxima cota de los nudos** (medido en píxeles con la escala del visor: 48.8 px/m, extremos en y = 275 (z = 7) y 718 (z = -2) frente a 275.8 y 715 esperados). **No dependen de los niveles** (0 y 3 sin cambios).
- **Dónde hay verticales**: en las columnas de x en pantalla observadas (733, 852, 892, 971, 1011, 1091, 1131, 1210, 1250, 1330, 1449 px) que corresponden a todas las sumas x+y con x ∈ {0, 3, 6, 9, 4} e y ∈ {0, 3, 6, 9}, es decir **en cada intersección entre una línea en X y otra en Y, ya sean de la rejilla o de las líneas de referencia de los nudos** (x = 4 viene de N3/N4).
- Las rejillas horizontales (líneas X e Y) se dibujan en **cada cota distinta de nudo** (se vieron cuatro copias para z = -2, 0, 3, 7); la copia de nivel sin nudos no se comprobó.

### 9.7 Lo que quedó sin medir
- Criterio de fusión al borrar cuando las barras unidas tienen sección/material distintos (Barras > Sección no abrió diálogo con LMB + RMB).
- Tolerancia de coincidencia (crear un nudo sobre otro con la calculadora no muestra mensaje y no añade nada: ya medido en §8; la tolerancia numérica sigue sin medir) y de alineación.
- Significado exacto de los 8 iconos del panel de Mover (solo tooltips; no se probó cada modo: absoluto/relativo, XY, ángulo) y de los iconos del panel «Distancia» (solo un icono, tooltip «Distancia»).
- Efecto de Mover/Borrar sobre cotas, vínculos y cargas del nudo.
- Decodificación completa de los registros de nudo/barra/línea en el .ed3 (flags y bloque de extensión).
- Tercer RMB en Mover fase 1 (¿sale del comando?) y el clic fuera del panel (la captura coincidió con un parpadeo de pantalla negra).
