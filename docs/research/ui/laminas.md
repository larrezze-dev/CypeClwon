# Grupo Láminas (pestaña Geometría) - hallazgos REA estáticos sobre `ed3dlib.dll`

Fuente: solo REA/Ghidra sobre `ed3dlib.dll` (sha256 ef10cf81...dead) + volcado `data/strings_merged_latin.tsv`. No se lanzó ninguna aplicación. Etiquetas: [REA] pseudocódigo leído; [CAD] cadena es-ES; [PROB] inferencia razonada; [NV] no verificado. Método de enlace mensaje->código: ver `edicion_pilares_vigas.md` §0 (nota: `procedure_pseudo_code` acepta también una dirección interior a la función, p. ej. la referencia al descriptor).

## 1. Tooltips y ayudas por comando (es-ES) [CAD]
| Comando | Id | Texto |
|---|---|---|
| Nueva | #64705 / #64706 | «Permite crear una nueva lámina.» / «...introduzca con el botón izquierdo del ratón los puntos que definen el perímetro de la lámina y, a continuación, pulse el botón derecho del ratón.» |
| (conexión) | #64707 | La conexión con barras/otras láminas es **explícita por aristas**: para conectar una lámina con una barra, una arista de la lámina debe tener los mismos nudos extremos que la barra; un nudo está conectado a una lámina si forma parte de alguna de sus aristas. |
| Borrar | #64710 | Seleccionar láminas (clic o rectángulo) y botón derecho. |
| Mover | #64708 / #64709 / #65123 | «mueve los nudos del perímetro»: 1) clic en la lámina; 2) clic en los nudos a desplazar («Seleccione los nudos a desplazar») y botón derecho; 3) dos puntos que fijan dirección y magnitud. |
| Disposición | #64713 | «ajustar la posición de varias láminas a la vez con respecto a su plano de definición». Selección: #64711 / geometría #64712 (campos [NV]). |
| Sección | #65122 «Sección de la lámina» | Campos (espesor...) [NV]. Discretización: #64714 «parámetros para configurar la discretización». |
| Material | #64718 | «editar el material de un grupo de láminas». Opciones #94482-94486: Láminas de hormigón / acero / madera / aluminio / material genérico; genérico isótropo #375014 / ortótropo #374884. Asignación automática #65118 «para todas las láminas» y #65119 «para todas las que no han sido asignadas» (fuente de esta asignación [NV]). |
| Unir | #64724 | Unir láminas con alguna arista común y **en el mismo plano**; si hay planos agrupados une las homólogas; seleccionar con clic/rectángulo + botón derecho. |
| Dividir | #64725 / #64726 | Dos puntos definen el segmento de división + botón derecho. «Las láminas se dividen completamente aunque el segmento penetre en ellas de forma parcial.» Con planos agrupados divide las que cortan el segmento homólogo. |
| Invertir eje 'z' | #64715 / #392575 | «invertir la dirección del eje Z local de un grupo de láminas». |
| Definir eje 'x' | #64716 / #64717 / #392576 | «definir la dirección del eje X local de una lámina»; se elige **la arista** de la lámina que define la dirección X (clic izquierdo sobre la arista). |
| Ejes locales | #392577 | Visualización (opción «Mostrar los ejes locales de los elementos», ver captura). Arista Z/X local: ver §4. |
| Vinculación interior/exterior de aristas | #64719-64723 | Fuera del ribbon de este grupo (menú contextual/edición); interior = conexión con barras/láminas adyacentes; exterior = coacciones globales; nudo con aristas de vinculaciones exteriores distintas = no se aplica ninguna y se avisa. |
| Bandas de integración | #64728-64730 | Edición de bandas (varias láminas a la vez; solo se usa la geometría de la primera y luego se copian). |
| Discretización | #64301-64304 | «Tamaño máximo de los lados de un triángulo»; valor por defecto textual **«Determinado por el programa»** (#64304). Malla de triángulos con compatibilidad entre aristas vecinas (#64303). |
| Peso propio | #64702 | Generación de cargas de peso propio de las láminas (paso del cálculo). |

## 2. Reglas de láminas planas: 3/4 nudos y coplanaridad
- Una lámina se define con **N puntos de perímetro** introducidos con clic izquierdo y cerrados con botón derecho (#64706). Error de definición: **#65121** «Los puntos introducidos no permiten definir correctamente la lámina.» [CAD]. El número mínimo/máximo de nudos (3/4) **no consta en las cadenas**; el formato de texto (§3 de `importar.md`) habla de «los números de nudo que componen la lámina» sin límite, y los errores del importador son «lámina con nudos repetidos» (#106887). Por tanto se acepta polígono de N lados: [NV] límite superior. La malla es de triángulos (#64302).
- **Coplanaridad (unir)** [REA] `FUN_180039b10` (ed3arlamef.c, línea 0x51c): recibe el array `laminas_unir` (`ed3lamef_t`, **>= 2** elementos, si no `error_interno("arr_NumElemsPunteroST(laminas_unir, ed3lamef_t) >= 2")`), puntero de salida `coords_nudos_3d_lamina_union` y puntero de mensaje.
  1. `FUN_18003c480` (comprobación de coplanaridad): recopila los puntos 3D de todas las láminas (`FUN_18003b550`) y llama a `ejes2d_punto3D_forman_ejes_con_precision(puntos, 0x3f50624dd2f1a9fc)`; la constante es **0,001** (en unidades de longitud de la obra). Si no son coplanares -> mensaje **#64004** «Las láminas seleccionadas no están contenidas en el mismo plano.» (descriptor `FUN_18003a880`).
  2. `FUN_18003b880` calcula el contorno 2D de la unión como `Cnt2D` (conjunto de polígonos). **`num_poligonos` debe ser exactamente 1** (si 0 -> error interno `num_poligonos > 0`); si es >= 2 (hueco o regiones disjuntas) -> **#64005** «No es posible unir las láminas seleccionadas porque no delimitan una región cerrada sin huecos.» (descriptor `FUN_18003adc0`, usado en 0x180039c55).
  3. Si hay 1 polígono, `FUN_180334d90` devuelve las coordenadas 3D de los nudos del contorno de la lámina resultante.
- **#64232** «Debe seleccionar, al menos, dos láminas situadas en el mismo plano para poder realizar la unión.» (validación previa de selección, caller [NV]). **#65120** «Se han producido las siguientes incidencias al unir las láminas seleccionadas» (cabecera de la lista de incidencias; las incidencias concretas distintas de #64004/#64005 [NV]).
- Mensajes de geometría de lámina usados en el cálculo/Reparar (ver `herramientas.md` §3): #400344 «El nudo '%s' está demasiado próximo a la arista (distancia: %s).»; #64006 «Se han encontrado láminas que contienen tirantes.»; #64010 nudo no conectado a ninguna barra ni lámina.

## 3. Dividir / Unir con planos agrupados
Texto #64724/#64726: con planos agrupados (grupo de planos paralelos) la operación se replica en las láminas homólogas de los otros planos del grupo. Implementación del homólogo [NV].

## 4. Ejes locales, valores por defecto
- Reglas de ejes: eje local z = normal de la lámina (se invierte con «Invertir eje z»); eje x local definido por una arista elegida (#64717). Convención por defecto de x (p. ej. proyección del eje X global) [NV, no hallada].
- Espesor, material por defecto, vinculaciones por defecto: [NV] (el valor por defecto del tamaño máximo de triángulo es «Determinado por el programa», #64304).
- Referencia de láminas: #64296 «Referencias de láminas» (numeración en datos generales).
- Funciones relacionadas en `ed3dlib.dll`: `ed3obra_hay_laminas_de_hormigon|aluminio|con_acero_laminado|con_acero_conformado` (0x1804686f0, 0x1804686b0, 0x180468670, 0x180468630) y `ed3dobra_estan_todas_las_laminas_ef_asignadas_a_planta` (0x180442910; #64001/ asignar planta, #64727 «láminas que pertenecen a las plantas seleccionadas»).
- Solver de láminas: `esport3d_crea_lamina`, `cyoslamina_crea_material_isotropo|ortotropo`, `cyoslamina_crea_vinc_int_arista_libre|articulada|empotrada|generica` (CALCESF/CYOPENSEES, no analizados).

## Pendiente (no verificado)
Campos y rangos del diálogo «Sección de la lámina» y «Disposición»; número máximo de nudos por lámina; convención por defecto del eje x local; incidencias adicionales de #65120; algoritmo de Dividir (corte de polígono con segmento) y función del callers de #64232.
