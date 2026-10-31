# Pestaña Carga

Jerarquía y estado: [captura] `07_ribbon_tabs.png` fila 3 (obra vacía: solo Hipótesis vista y Escalas habilitados). Cadenas: [cadena] ed3dlib.dll.

## Índice
| Grupo | Comandos |
|---|---|
| Visualización | Hipótesis vista (#65380, en: "Visible loadcase"), Escalas (#64337, #64652 "Permite indicar la escala de dibujo para las cargas introducidas", #64478 "Escalas de cargas por tipos") |
| Nudos | Desplazamientos prescritos, Nueva, Editar, Borrar, Hipótesis |
| Barras | Nueva, Borrar, Editar, Peso propio, Hipótesis |
| Láminas | Nueva, Borrar, Editar, Peso propio, Hipótesis |
| Paños | Nuevo, Borrar, Editar, Mover, Dividir, Dirección de reparto |
| Cargas superficiales | Nueva, Borrar, Editar, Copiar, Mover, Asignar, Hipótesis |

## Datos verificados por cadena
**Cargas en barras** (#64344 "Introducir cargas sobre barras", #64571, #64653 "Permite introducir cargas sobre un grupo de barras", #64647 editar, #64654 "Permite modificar los datos de las cargas ya introducidas", #64656 "Permite eliminar una o varias cargas vistas en pantalla"). Tipos: Carga puntual (#64218), Uniforme (valor absoluto #64213), En faja (#64214, #38950), Triangular derecha / izquierda (#64215, #64216; #38932/38933 "Triangular Der./Izq."), Trapecial (#64217; "Valor absoluto de la carga en el extremo final" #64224), Momento puntual (#64330), Incremento de temperatura variable (#64219), "Carga variable en el eje x/y local" (#64220/64221, #64592/64593). Texto de ayuda de signos: "Cargas uniformes, en faja, triangulares y trapeciales: %s. Momentos puntuales: %s. Incrementos de temperatura: %s." (#64914-64916; el parámetro %s lo rellena el programa: [no verificado]); "Cargas triangulares: '%s' es el valor máximo de la carga. '%s' no se utiliza." #64910. Ejes y dirección: "Ejes sobre los que se define la dirección de la carga" #65395; "Ejes globales / Globales / Locales" #65396-65398; "Definir dirección" #64320; "Vector director" #64318; "Ángulo en el plano 'XZ' local" #64321; "Dirección de la carga respecto al eje 'Z' local" #64322; "Sentido positivo de la carga" #65051. Validación: "No puede introducir cargas de valor nulo." #65050; "Se han borrado cargas incompatibles con la longitud de la barra." #65192.

**Cargas en nudos** (#64345, #64649, #64521; "Cargas sobre nudos" #64897). Desplazamientos prescritos (#64088-64091): "Asignar un desplazamiento o un giro a un nudo con coacciones exteriores."; tabla cuya primera columna son las hipótesis simples; unidades en cabecera; solo en nudos con coacciones exteriores. Campos: Desplazamientos / giros prescritos en ejes globales (#64533, #64534).

**Cargas en láminas** (#65400 "Cargas en láminas", #64422, #388089 "Carga en lámina"): Uniforme, Variable, Temperatura, Retracción [web].

**Peso propio** (#25165): "Permite activar / desactivar la generación automática de la carga de peso propio." #64694; "Generación de las cargas de peso propio de las barras / láminas" #64697, #64702; mensaje #64002 "Se ha desactivado la generación automática de las cargas de peso propio para esta barra."

**Paños** (#64123-64131; #38805 "Paños"; #76172 "Paño"): paño horizontal (paralelo a XY global) solo admite cargas uniformes; ilimitadas cargas por tipo e hipótesis; columna no editable informativa; formato de lista de cargas; "Permite modificar la lista de cargas aplicadas a los paños seleccionados." #64670; "Uniformes / Variables con la altura" #65057/#65058; "Valor en la cota inferior" #65053 ("Valor de la carga en el punto más bajo de la superficie." #65055 / "…más alto…" #65056); en hipótesis distintas de viento una carga es positiva si su sentido es vertical [texto truncado: #64131, completar]. Dirección de reparto (#399487-399489: "Paralela al lado seleccionado" / "Perpendicular al lado seleccionado"; #65132). Validaciones: "Existen paños solapados." #64204; "Los puntos indicados no permiten definir correctamente un paño." #65130/#65133; "Seleccione un paño" #65135.

**Cargas superficiales** (#38869; #38954 "Carga superficial"): se edita por hipótesis (#64127); la vista puede filtrarse a una hipótesis (#64128); uniformes o variables en altura (#64129); "¿Desea crear un paño asociado a la nueva carga superficial?" #63986; "Activar/desactivar asignación de cargas superficiales" #64665; "Mover cargas superficiales" #65381; "Mover también las cargas aplicadas sobre la misma zona pertenecientes a otras hipótesis" #65139; "Seleccione una carga superficial" #65136; "Seleccione los vértices a desplazar" #65138.

**Hipótesis**: "Hipótesis simple" #64399; "Se van a borrar todas las cargas definidas en la hipótesis." #64905; "Se van a borrar las combinaciones no lineales y de pandeo en las que interviene la hipótesis." #380751; "Se van a borrar todas las cargas y combinaciones no lineales y de pandeo asociadas a la hipótesis." #380752. La definición de hipótesis/combinaciones se hace en Datos generales (no en esta pestaña) [web: persistente, transitoria, sísmica, accidental, incendio; γ y ψ].

## No verificado
Campos exactos, orden, unidades por defecto y valores iniciales de cada diálogo de carga; botón "Hipótesis" de cada grupo (se infiere que asigna/filtra por hipótesis; **no verificado**); signos por defecto.
