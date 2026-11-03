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
