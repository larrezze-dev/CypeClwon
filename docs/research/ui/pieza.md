# Geometría > Barras: Pieza, Agrupar/Desagrupar, Mover extremo, Invertir eje 'x' (T-P5-04), pasada REA 1

Estado: pasada REA terminada; NO se lanzó ninguna aplicación ni se abrió ningún .ed3. Etiquetas: [cadena] = texto UTF-16 `<id>texto` literal de `ed3dlib.dll` (es-ES, variante «ratón», no la «mouse» de es-MX); [código] = nombre de procedimiento; «no verificado» = deducción.
Método: volcado de `<id>` UTF-16 de ed3dlib/enrcppan/entopane (los textos largos con segunda línea se leyeron con búsqueda binaria hasta `00 00`, porque el volcado por líneas anterior los truncaba). Los procedimientos internos de ed3dlib no tienen nombre (solo exportados); los manejadores de comando no se localizaron por nombre, por lo que NO hay pseudocódigo de la lógica de estos 4 comandos.

## 1. Modelo conceptual (evidencia textual)

- Barra = segmento entre dos nudos (barras.md). **Pieza = grupo de barras alineadas descritas conjuntamente como una única pieza** (#64624). Una pieza se define por su nudo inicial (de la primera barra) y su nudo final (de la última barra) (#64625). Las propiedades de descripción (perfil, material, disposición/giro/enrase, coeficientes de empotramiento, pandeo) son de la pieza; por eso #64229 exige que las barras tengan la misma disposición.
- La pieza de definición es «la línea imaginaria que une los dos nudos extremos de la pieza» (#64142).
- Una barra por defecto es una pieza de una sola barra (los textos de resultados/listados hablan de «pieza» y de «barras de la pieza», #64982, #199362 «Todas las barras de la pieza deben pertenecer al mismo grupo de flecha»; el grupo de flecha es otro concepto: #64636 cada barra es por sí misma un grupo de flecha secante; #64633/#64635 agrupar barras alineadas para flecha conjunta; NO confundir con Pieza ni con Agrupar).
- Dos significados de «grupo»: (a) pieza = barras alineadas (geometría); (b) **grupo de piezas** = piezas con el mismo perfil y material (Agrupar/Desagrupar, §3). Tipos de pieza: «Pieza estructural» #269899, «Pieza» #57280, «Piezas» #33705.
- Pilar/viga: tipo de elemento estructural. «Pilar» agrupa piezas alineadas verticales (#64853, #64027); «viga continua» agrupa piezas conectadas (#64020). Son editores de Edición de pilares/vigas, no de este grupo.

## 2. Pieza

| Id | Texto |
|---|---|
| #64624 (tooltip) | «Permite describir conjuntamente un grupo de barras alineadas como una única pieza.» |
| #64625 (paso/ayuda) | «Seleccione el nudo inicial de la primera barra y el nudo final de la última barra de las que desea crear una pieza.» |
| #65126 (confirmación) | «¿Desea crear la pieza?» (variante de Pilar: #65127 «¿Desea crear el pilar?») |
| #65316 | «Crear piezas» (rótulo; contexto no verificado: posible opción de generación, cf. #63975 «Crear pieza entre barras alineadas», #63985: las divisiones de cada segmento forman una única pieza) |
| #64226 (error) | «Ya existe una pieza equivalente.» |
| #64227 (error) | «No se ha encontrado un grupo de barras alineadas que conecte los dos nudos seleccionados.» |
| #64229 (error) | «Las barras seleccionadas no tienen la misma disposición (giro y enrase).» |
| #64230 | «Los nudos seleccionados ya pertenecen a la misma ligadura.» (es de Ligaduras, no de Pieza; cuidado con confundir) |

Flujo (deducido de los textos; solo la secuencia de selección y los tres errores están respaldados):
1. LMB sobre el nudo inicial (de la primera barra) y LMB sobre el nudo final (de la última). Entre ambos debe existir una cadena de barras alineadas conectadas (si no: #64227).
2. Posible confirmación «¿Desea crear la pieza?» (#65126): su existencia está en la DLL; si aparece como diálogo Sí/No en este comando, no verificado.
3. Errores: pieza idéntica ya existente (#64226); barras con distinta disposición giro/enrase (#64229).
No hay cadena de estado propia («Barras - Pieza | …»): se compondrá de las genéricas de entopane (§6 de barras.md; «Seleccione un elemento», «Introduzca un punto», etc.; cuál se usa en cada paso no verificado).

Las funciones `ed3c3d_append_pieza_*` (barras.md §4/§5) son constructores de pieza por material, no el comando.

## 3. Agrupar / Desagrupar

| Id | Texto |
|---|---|
| #64626 (Agrupar, tooltip + paso) | «Permite crear grupos de piezas que tendrán asignado el mismo perfil y material, incluso como resultado del proceso de dimensionamiento.\nPara crear un grupo seleccione dos o más piezas y pulse el botón derecho del ratón.» |
| #64627 (Desagrupar) | «Permite independizar piezas de un grupo, de forma que su descripción ya es independiente.\nSeleccione las piezas que desee desagrupar y pulse el botón derecho del ratón.» |
| #65100 | «¿Desea agrupar las piezas seleccionadas?» |
| #65101 | «¿Desea desagrupar las piezas seleccionadas?» |
| #65102 | «Se han eliminado las cartelas incompatibles con las longitudes de las piezas agrupadas.» |

Flujo: seleccionar ≥ 2 piezas (LMB, acumulativo; ventana posible: no verificado) → RMB → pregunta Sí/No (#65100) → las piezas del grupo comparten perfil y material (cambiar la descripción de una cambia las demás; esto es la semántica de «grupo»); tras agrupar se eliminan cartelas incompatibles con las longitudes (#65102, aviso informativo). Desagrupar: seleccionar piezas → RMB → «¿Desea desagrupar…?» (#65101); cada pieza conserva la descripción pero ya es independiente.
Pistas de código (sin leer): `entobasi.dll::ebaobid_set_id_agrupacion1/2`, `ebaobid_get_id_agrupacion1`, `ebaobid_con_id_agrupacion1` (id de agrupación en el identificador de objeto) y `ebarcinf_hay_objeto_capturado_ind_estado_considerando_agrupados` (selección/captura que considera agrupados: al seleccionar una pieza de un grupo es plausible que se capture todo el grupo; no verificado). «Agrupar planos» (#65150-65155, #64029-64034) es otra función (Geometría > Planos), no confundir.

## 4. Mover extremo

| Id | Texto |
|---|---|
| #65388 | «Mover extremo» (título del comando; en otros idiomas Déplacer extrémité, Move end, Ende verschieben) |
| #64645 (tooltip) | «Permite mover el extremo de una barra a una nueva posición.» |
| #65385 / #65386 (pasos, probables) | «Seleccione el punto de referencia para el desplazamiento» / «Seleccione un punto de destino al que desea desplazar el punto de referencia» |
| #65356 | «La longitud de la pieza es insuficiente para aplicar los desplazamientos asignados.» (de Disposición, no de este comando) |
| #64902 | «Extremos de barras» |

Caveat: los ids 65385/65386 son adyacentes a #65388 y encajan con «mover extremo», pero también sirven al grupo «Mover cargas superficiales» (#65381; variante larga #65137 «seleccione el punto que se tomará como referencia para el desplazamiento y, posteriormente, el destino…»). Que Mover extremo de barra use esos dos pasos NO está verificado. El análogo «Mover extremo» de bandas de integración (#65000) sí documenta: LMB sobre el extremo, LMB sobre la nueva posición; probable patrón análogo para barras (no verificado). Y el de Mover nudos (#64745-64747, nudos.md): barras acompañan, RMB cancela.
Preguntas abiertas: nudo propio nuevo vs. mover el nudo (¿qué ocurre con las otras barras que llegan al nudo?: «extremo de barra» sugiere crear/usar otro nudo; no verificado), captura a nudos existentes, panel dinámico (barras.md §2), mensaje si la longitud queda < mínima (#64000), si desalinea una pieza.

## 5. Invertir eje 'x'

| Id | Texto |
|---|---|
| #64640 (tooltip) | «Al pulsar sobre una barra con esta opción activada, se invierte la dirección del eje local X de la barra. De esta forma puede conseguir disposiciones simétricas en la estructura con perfiles no simétricos, como pueden ser los Z o L.» |
| #64641 | «El cambio de eje del perfil invierte el origen y final de la barra. En el caso de tener descritos coeficientes de empotramiento ó introducidas cargas, el programa automáticamente modificará los datos de introducción para que no sufran alteración alguna con el cambio de eje.» |

Reglas: es un modo activado (la frase «con esta opción activada» + «Al pulsar sobre una barra»): cada LMB sobre una barra la invierte inmediatamente (no menciona RMB ni confirmación). Efecto: intercambia nudo inicial y final de la barra (la barra pasa a ir de B a A), y los datos dependientes se reasignan para que la estructura física no cambie: coeficientes de empotramiento (swap inicio/fin), cargas (posición medida desde el nuevo origen: x → L − x, distribuciones inicio/fin intercambiadas, sentido de cargas relativas al eje local), y por extensión giro/enrase/desplazamientos «iguales/distintos en origen y extremo» (#64151/#64152, #65357/#65358 «Origen de la pieza» / «Extremo de la pieza»; el tratamiento del giro es no verificado). Como el comando actúa sobre «barra»: en una pieza de varias barras, si se invierte todo el orden: no verificado.
Relacionado: pilar exige x local ascendente (#64988); #64986 «La directriz de alguna pieza no está orientada en dirección ascendente.»; viga continua exige que el nudo final de una sea el inicial de la siguiente (#64020); el invertir puede hacer fallar estas comprobaciones (aviso: no verificado).

## 6. Marca de tipo de elemento (TK-048)

Textos: #65228 «Tipo de elemento estructural» (campo de Sección, valores genérica/hormigón/acero…, ver barras.md §4); #64022/#64023 «Alguna de las piezas … no está definida como elemento estructural de tipo '%s'.» (donde '%s' ∈ {pilar, viga}); #64233 «Alguna de las piezas entre los nudos seleccionados no es de tipo 'pilar'.»; #64234 «Ya existe un pilar con las mismas piezas.»; #64235 «Para crear el pilar debe seleccionar piezas completas.»; #64027 piezas del pilar alineadas, conectadas por nudos, x local ascendente; #64020-64026 viga continua (misma clase de material, plano xz vertical, ángulo máximo con el horizontal #64021 '%s' no leído). Es decir: pilar/viga son marcas sobre la **pieza** (no sobre la barra); sobre un conjunto de piezas existe además el objeto «pilar»/«viga continua» (Edición de pilares/vigas). Pilar (#64850) crea piezas con marca «pilar»; Viga (#64882) con marca «viga». Para T-P5-04: marcar a nivel de pieza (enum tipo de elemento estructural: genérica, pilar, viga, …), no persistir aún el objeto «pilar continuo».

## 7. .ed3

No se leyó ningún .ed3 ni la estructura de pieza/grupo: no hay evidencia de cómo se serializa pieza, grupo o marca de tipo. Pistas: constructores `ed3c3d_append_pieza_*` (25 parámetros), id de agrupación en el identificador de objeto de `entobasi` (§3). Decodificar requiere una obra del original con (a) una pieza de 2-3 barras, (b) dos piezas agrupadas, (c) una barra con eje invertido, comparando bytes.

## 8. Qué queda sin medir (para pedir al usuario/medir en el original)

1. Barra de estado exacta (LMB/RMB/Esc) de Pieza, Agrupar, Desagrupar, Mover extremo, Invertir eje en cada paso; aspecto del cursor y selección (resalte celeste/naranja) y si hay panel dinámico.
2. Pieza: si aparece diálogo «¿Desea crear la pieza?», cómo se elige entre varias cadenas posibles, y si las barras intermedias dejan de ser seleccionables por separado (tooltip «Barra Nx/Ny» vs «Pieza …»), numeración, cómo se dibuja/etiqueta una pieza.
3. Pieza con nudo intermedio no alineado; con barras de diferente perfil/material (¿error? ¿se unifica con la de la primera barra?); con barras de distinta capa.
4. Agrupar: mecanismo exacto de selección múltiple (LMB acumulativo, ventana), cuál perfil/material prevalece (el de la primera pieza seleccionada: no verificado), cómo se visualiza un grupo, qué ocurre al editar una pieza del grupo; comportamiento con piezas de diferente tipo de material.
5. Desagrupar: si basta una sola pieza y si deja el grupo con el resto.
6. Mover extremo: pasos reales (¿1.er clic en el extremo de la barra/ nudo de la barra y 2.º clic destino?), qué se mueve (nudo o solo el extremo de esa barra creando un nudo nuevo), efecto en otras barras, captura, mínimo de longitud, mensajes.
7. Invertir eje: confirmar tratamiento de cargas y empotramientos con valores asimétricos (comparar antes/después con carga trapecial y empotramiento 0.5/1), giro de sección, y si afecta a todas las barras de la pieza.
8. Qué ids de la barra de estado y de ribbon (rótulos «Pieza», «Agrupar», «Desagrupar», «Mover extremo», «Invertir eje 'x'») usan: los rótulos cortos de botón no aparecen como cadenas propias (solo los de captura 07/geometria.md); #65388 sí «Mover extremo».
9. Formato .ed3 de pieza/grupo/marca.
