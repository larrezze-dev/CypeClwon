# Edición de pilares y vigas (T-P5-08) - hallazgos REA estáticos sobre `ed3dlib.dll`

Fuente: solo REA/Ghidra sobre `ed3dlib.dll` (sha256 ef10cf81...dead). No se lanzó ninguna aplicación. Etiquetas: [REA] verificado en pseudocódigo; [CAD] cadena del volcado `data/strings_merged_latin.tsv`; [PROB] inferencia razonada, no leída directamente; [NV] no verificado.

## 0. Cómo se enlazan mensajes y código (útil para futuras búsquedas)
- Cada mensaje es un «descriptor» `FUN_xxx(undefined8* p)` que rellena un registro `{ "ed3arpieza.msg", nº de mensaje, 7, punteros por idioma "<id>texto" }` (p. ej. `FUN_18005a910` = #64021, nº 0x1b9; `FUN_18005d8d0` = #64026, nº 0x284). El código no carga el id (0xFA15) como inmediato: llama a `nousar_msglocal_original_u(<descriptor>)`. Para hallar al usuario de un mensaje: `search_strings "<id>"` -> `xrefs` de la cadena inglesa (apunta al descriptor) -> `xrefs` del descriptor (llamadores). Los nombres de módulo de origen quedan en cadenas `ed3arpieza.c`, `ed3pilar.c`, `ed3pieza.c`.

## 1. Comprobaciones de viga continua y pilar continuo
### Viga continua (geometría por pieza) - `FUN_1800648e0` (llamada por `FUN_180068840` @0x180068840) [REA]
1. Obtiene los 3 ejes locales de la pieza (`FUN_1804db150`: triples x, y, z).
2. **#64021**: `FUN_1800638c0` calcula el ángulo entre el vertical (0,0,1) y el eje local z; si `|ángulo| - 0,0001 >= radian(80)` -> error con `%s`. Valor del límite: `radian(0x4054000000000000)` = **80°** respecto al plano horizontal (equivalente a que el eje z local se desvíe >= 80° de la vertical). El `%s` se formatea con `convers_texto_u(rad80, 0xf)`; el texto mostrado exacto (80 o 1,396...) [NV].
3. **#64026** (`FUN_18005d8d0`): n = normalizado((0,0,1) x eje x local); ángulo entre n y el eje y local; si > 90° -> 180° - ángulo; **error si >= radian(0,5) = 0,5°** (constante 0x3fe0000000000000). Es decir, el plano xz local debe ser vertical con tolerancia 0,5°.
4. Después, por pieza, `FUN_1804e40c0(pieza, tipo)`: si falla -> #64022 («ya no está definida como elemento estructural de tipo '%s'», rama param_5==1, descriptor `FUN_180059910`) o #64023 («Alguna de las piezas seleccionadas no está definida... tipo '%s'», `FUN_18005c8d0`); el reparto 64022/64023 por descriptor es [PROB] (ids de esos descriptores no leídos).
5. Orden en el bucle de `FUN_180068840`: para cada id de pieza: geometría (64021, 64026) -> tipo (64022/64023). Para al primer fallo (no acumula). Lista de ids vacía = error interno `num_ids_piezas > 0`.
- Mismo material (#64025) y mismo hormigón (#64024): sus descriptores existen (0x180980620/0x180980070) pero no se siguió su llamador [NV orden].

### Encadenado/conectividad (#64020 viga, #64027 pilar) - `FUN_1800678f0` (@0x1800678f0), `FUN_1800645e0` [REA]
- `FUN_1800678f0(piezas, esViga, &ordenadas, &msg)`: toma la 1.ª pieza como cabeza (nudo ini/fin), y mientras queden piezas busca una que comparta el nudo **final** (la añade al final) o el **inicial** (la inserta delante) mediante `FUN_180067dc0`; si la pieza comparte el nudo en el sentido contrario (orientación invertida, `local_3c==1`) o no se encuentra ninguna pieza que encaje -> error. Mensaje: `esViga==1` -> **#64020** (`FUN_18005c380`); si no -> **#64027** (`FUN_18005be30`). Máximo 1000 iteraciones (`num_iteraciones < 1000`). Es un orden topológico de cadena, no exige orden de selección.
- `FUN_1800645e0` (comprobación de alineación pieza a pieza, usa #64020 en 3 puntos, líneas 0xc50/0xc5c/0xc62): error si `FUN_1800682e0` (pieza incompatible) , si no comparten nudo (`FUN_1800681a0`), si el nudo común ya fue usado (`arr_ExisteULong`) o si `FUN_180067f60` (alineación) falla.

### Pilar (geometría por pieza) - `FUN_18079f150` en `ed3pilar.c` y `FUN_1804ccce0` -> `FUN_1804f86d0` [REA]
- Por pieza: `FUN_1804ccce0` (verticalidad) y si falla se emite el mensaje del descriptor `FUN_18079ba50` = **#64988** («La pieza debe ser vertical, con su eje x local en dirección ascendente»); si pasa, `FUN_1804dca20` devuelve otro texto (p. ej. #64987 «no está definida entre dos niveles»; [PROB]).
- `FUN_1804f86d0` (valores = coordenada del nudo + excentricidad de cada extremo): **|x_ini - x_fin| < 0,0005 y |y_ini - y_fin| < 0,0005** (unidades de longitud de la obra, m) para ser vertical (mensaje del descriptor `FUN_1804e7cc0`, [PROB] #64985); **z_fin + dz_fin > z_ini + dz_ini** (ascendente; si no, descriptor `FUN_1804e8ca0`, [PROB] #64986); y los puntos 2D de los extremos iguales con tolerancia **1e-4** (0x3f1a36e2eb1c432d) (si no, mismo mensaje no-vertical).
- Crear pilar continuo (#64233 «no es de tipo pilar», #64234 «Ya existe un pilar con las mismas piezas», #64235 «debe seleccionar piezas completas», #64853/#64854 ayuda): descriptores en 0x180a58510/0x180a58d60; llamadores no seguidos [NV]. La ayuda indica seleccionar nudo inicial de la 1.ª pieza y nudo final de la última.
- #64989 «El nudo no está contenido dentro del canto del forjado» y #64990 «El pilar no puede formar parte de un recuadro arriostrado»: textos presentes (0x180d2c550, 0x180d2c930); condición exacta [NV].

## 2. Referencias y numeración
- Editar referencia (#65436 / ayuda #64859), «Numerar» (#64860, ayuda #64861 «numerar consecutivamente todos los pilares de la obra a partir de una referencia inicial dada»). [CAD]
- Panel «Referencia inicial» (#64972, descriptor `FUN_1804b1ad0`, usado por `FUN_1804accd0`): editor de texto de **máximo 30 caracteres (0x1e)** (`pnpanel_referencia_u_con_edittext_u(titulo, param, 0x1e, ...)`); el botón Aceptar se valida con `FUN_1804ab570` -> `FUN_1804b3b60` (cuerpo no leído). Mensaje de rechazo **#64971 «La referencia no puede quedar vacía.»** y **#64974 «El prefijo no puede quedar vacío.»**; «Referencia base» #64975. [REA+CAD]
- Orden por defecto de la numeración de pilares: **#65039 «Los pilares se numerarán de menor a mayor coordenada Y y, a igualdad de coordenada Y, de menor a mayor coordenada X.»** [CAD]. Algoritmo de incremento de la referencia (p. ej. «P1» -> «P2», sufijos numéricos) [NV: no leído].
- Numeración de pórticos (#64888-64893): opciones «Numerar primero los pórticos paralelos al eje X global» / «... Y global» y sentido: X en sentido +Y (#64890) o -Y (#64891); Y en +X (#64892) o -X (#64893); «Numeración por niveles» (#64896); «No numerar» (#59909), «Numeración consecutiva de la obra» (#59911). Es el diálogo de datos generales de numeración (prefijo en datos generales de la obra, #64879/#64881). Precedencia/algoritmo exacto [NV]. Prefijo genérico #33058 «Prefijo».
- Referencia de viga: #64878/#64879 (editar referencias de vigas), #64880/#64881 (de vigas continuas): se usa una referencia distinta de la asignada por la numeración automática, si está activada; el prefijo se edita en datos generales de la obra. [CAD]

## 3. Flecha límite de vigas
- Menú: «Flecha límite de vigas» #65422 (ayuda #64874: seleccionar uno o más grupos de flecha que contengan vigas y botón derecho; las propiedades editadas se asignan a todos; los grupos de flecha son los que se usan para las comprobaciones). Título corto #64343 «Flecha Límite». [CAD]
- Cuatro valores: #64290 «Flecha límite máxima absoluta», #64291 «... máxima relativa», #64292 «... activa absoluta», #64293 «... activa relativa» (descriptores usados por `FUN_180262...` en 0x1802624c1-0x180262988, función de tabla de mensajes ya identificada pero no decompilada). Semántica: #64036-64046 (flecha máxima = máximo valor absoluto; flecha activa = diferencia entre máxima y mínima; relativa = relación luz/flecha). Rangos y valores por defecto (p. ej. L/300, L/400): **[NV] no leídos**; requieren decompilar la función que usa `FUN_1802624xx` o medirse en el original.
- Mensaje relacionado #65094 «Todas las barras del grupo de flecha de esta pieza deben estar contenidas en la viga continua.» y #64228 «Ya existe un grupo de flecha equivalente.» [CAD]

## 4. Editar nudos / Editar forjados adyacentes (ayudas)
- **Editar nudos** (#64865/#64866): edita la geometría de un nudo de una viga continua; seleccionar uno o varios nudos + botón derecho; la geometría afecta a la luz libre de las vigas del editor de armaduras y a la longitud de las armaduras. Campos del diálogo: [NV].
- **Editar forjados adyacentes** (#64867-64869): describe los forjados adyacentes a las barras de las vigas; define el **canto de las vigas planas** y los forjados que se representan en el plano de despiece de vigas; se tiene en cuenta en la resistencia a incendio de vigas metálicas. Seleccionar una o más vigas + botón derecho. «Canto del forjado» #64306; «Resto de vigas» #65430. Campos y rangos: [NV].
- **Editar canto del forjado** (pilar, #65437 / ayuda #64855): canto del forjado en un nudo de un pilar = altura libre entre dos niveles; sirve para usar uno distinto del automático; seleccionar nudos (clic o ventana) + botón derecho (#64856).

## 5. Viga bajo forjado / defaults Pilar-Viga
- «Viga bajo forjado» (`seccviga`, #56456) no se localiza en `ed3dlib.dll` (ver `describir_barras.md` §17). No se hallaron valores por defecto de Pilar/Viga en el código recorrido [NV].

## 6. Mensajes de ayuda/estado de comandos (cadenas, es-ES)
- Pilar #64850; Crear pilar continuo #64852-64854; Editar canto forjado #64855-64856; armados de pilares #64857, bloqueo #64858; viga #64882/#64883 («Introduzca con el botón izquierdo del ratón los nudos... Pulse el derecho para finalizar»); Crear viga continua #64862-64864 (requisitos: conectadas, misma clase de material, xz vertical); Editar nudos #64865-64866; forjados #64867-64869; armados #64870, bloqueo #64871, asignar armaduras #64872-64873; flecha #64874; actualizar resultados #64849 / #64875; «Edición de vigas» #64876; mensajes de error de obra #65438 («Algunos de los pilares... no están correctamente definidos. Puede usar la opción %s del menú %s»). Textos completos en `data/strings_merged_latin.tsv`.

## Pendiente (no verificado)
Constante textual de #64021 (80 vs 1,396...); orden relativo 64024/64025 frente a 64021-64023; condiciones de #64233-64235/#64989/#64990; campos y rangos de los diálogos Editar nudos, Editar forjados adyacentes y Flecha límite; algoritmo de incremento de referencia y de numeración de pórticos; cuerpo de `FUN_1804b3b60`.
