# Describir barras: capturas manuales del usuario («Perfiles (1..169).png»)

Fuente: 169 capturas de pantalla completas del original CYPE 3D v2027 (Geometría > Barras > Sección / Material), guardadas a mano por el usuario el 2026-11-05 (16:50-17:52) en `docs/screenshots/`. Etiqueta de fuente: **[CAP]** = leído en la captura (transcripción literal). «No visto» = ninguna captura lo muestra. Ninguna aplicación fue lanzada para este informe.

**Orden real.** El orden numérico coincide con el cronológico salvo la captura **(1)** (la última, 17:52:20) y **(2)** (la primera, 16:50:28). La (2) NO es del original: es la lista de «Capturas que necesito de ti» (texto de la petición al usuario).
**Datos largos en TSV** (`docs/research/ui/data/`, UTF-8, cabecera + una fila por elemento, columna `captura` = números de «Perfiles (n)»): `describir_perfiles_armado_ipc.tsv` (85), `describir_perfiles_conformado_u.tsv` (14), `describir_perfiles_aluminio_doble_t.tsv` (11), `describir_perfiles_madera.tsv` (836), `describir_perfiles_tirante.tsv` (619), `describir_series_combos.tsv` (43), `describir_material_combos.tsv` (51).

## 1. Mapa captura -> contenido (por secuencias)

| Capturas | Contenido [CAP] |
|---|---|
| (2) | Texto de la petición (no es del original). |
| (3) | Armado (icono 2 de material, Doble T simétrica armada): combo «Serie de perfiles» abierto: **solo «IPC»**. |
| (4)-(9) | Armado: combo «Perfil» abierto y desplazado de arriba abajo (IPC 533x203x62.6 ... IPC 2032x508x601.4): 85 perfiles, TSV armado. (7) -> (8) no se solapan (último de (7) «IPC 1575x406x276.7», primero de (8) «IPC 1575x406x316.7», es la selección resaltada): se asume contigüidad. |
| (10) | Conformado (icono 3): fila 1, 3.ª forma (U conformado rigidizado) seleccionada: serie «**C**», perfil «CF-60x2.0». Tooltip flotante «U conformado simple» sobre la forma 2 de la fila 2. «Datos adicionales > Disposición» (7 radios, ver §3). |
| (11) | Conformado, forma «U conformado simple» (fila 2, col. 2) seleccionada: serie «**U**», perfil «UF-60x3». |
| (12) | Conformado U: combo de serie abierto: **solo «U»**. |
| (13) | Conformado U: combo de perfil abierto: 14 perfiles UF (TSV). |
| (14), (15) | Aluminio (icono 5, 10 formas): tooltip «Doble T» sobre la 1.ª forma; serie «**(EP) I**», perfil «I-50x80x6»; Disposición: solo «Perfil simple». En (15) el combo de serie tiene el foco pero **está cerrado** (lista no vista). |
| (16) | Aluminio: combo de perfil abierto: 11 perfiles (TSV aluminio). |
| (17)-(19) | Madera (icono 4), forma «Redondo macizo»: tooltip (17); combo de serie = **solo «Redondo»** (18); combo de perfil (19): Ø100, Ø120, Ø130, Ø140, Ø150, Ø180, Ø200 (7). |
| (20)-(24) | Madera, forma «Cuadrado macizo» (tooltip en (20); serie por defecto «Maciza», perfil «S-100x100»): combo de serie (21): **Maciza, DUO/TRIO, Laminada**; perfiles (22) Maciza: S-100x100, 120x120, 140x140, 160x160, 180x180, 200x200, 220x220, 240x240, 260x260, 280x280; (23) DUO/TRIO: DUO-100x100, DUO-120x120, DUO-140x140, DUO-160x160, TRIO-180x180, TRIO-200x200, TRIO-240x240; (24) Laminada: GL-160x160, GL-180x180, GL-200x200, GL-220x220, GL-240x240, GL-260x260. |
| (25)-(29) | Madera, forma «Rectángulo macizo» (tooltip en (25); serie por defecto «Cabios/Viguetas», perfil «100x70»): combo de serie abierto con scroll (26)-(28): **Cabios/Viguetas; Maciza h100, h120, h140, h160, h180, h200, h220, h240, h260, h280; DUO/TRIO h100, h120, h140, h160, h180, h200, h220, h240, h260, h270; Laminada b60, b80, b100, b120, b140, b160, b180, b200, b220, b240, b260** (32 entradas; fin de lista en (28), scrollbar al final); (29) perfil de Cabios/Viguetas: 100x70, 120x80, 130x80, 140x80, 150x100, 150x130, 150x140, 180x130, 180x140, 200x130, 200x140 (11). |
| (30)-(39) | Rectángulo macizo, series «Maciza h100»...«Maciza h280»: un combo de perfil abierto por serie (TSV madera; ver §2.2). |
| (40)-(49) | Rectángulo macizo, series «DUO/TRIO h100»...«DUO/TRIO h270»: un combo por serie (TSV). |
| (50)-(94) | Rectángulo macizo, series «Laminada b60»...«Laminada b260»: 4 a 5 capturas por serie, el combo de perfil desplazado (GL-160..440, 440..720, 720/740..1000/1020, 1000..1280): b60 = (50)-(53), b80 = (54)-(57), b100 = (58)-(61), b120 = (62)-(66), b140 = (67)-(70), b160 = (71)-(74), b180 = (75)-(78), b200 = (79)-(82), b220 = (83)-(86), b240 = (87)-(90), b260 = (91)-(94). |
| (95)-(97) | Redondo macizo otra vez (tooltip, combo de serie «Redondo», combo de perfil Ø): duplicado de (17)-(19). |
| (98), (99) | Cuadrado macizo (tooltip «Cuadrado macizo» con serie «Laminada», perfil «GL-160x160»); combo de perfil de Laminada (duplicado de (24)). |
| (100) | Tooltip «Rectángulo macizo» con serie «Laminada b260», perfil «GL-1000x260». |
| (101), (102) | Madera, 4.ª forma, tooltip «**Rectangular, canto variable**»: serie «Rectangular, canto vari...» (texto truncado en el combo), perfil «RV-60»; Datos adicionales: **Disposición** (radio «Perfil simple») y grupo **«Canto del perfil»**: «Canto inicial» **100.0** mm, «Canto final» **100.0** mm. (102): combo de perfil: RV-60, RV-80, RV-100, RV-120, RV-140, RV-160, RV-180, RV-200, RV-220, RV-240, RV-260 (11). |
| (103)-(117) | **Tirante**, forma «Angular simétrico» (tooltip (103)): serie «L», perfil «L 35 x 35 x 4»; combo de serie (104) = **solo «L»**; combo de perfil desplazado (105)-(117): 179 perfiles (TSV tirante, forma «Angular simétrico»). |
| (118)-(122) | Tirante, forma «Angular» (tooltip (118), perfil «L 100 x 65 x 7»): combo de serie (119) = **solo «L»**; perfiles (120)-(122): 31 (TSV). |
| (123)-(149) | Tirante, forma «Platabanda» (tooltip (123); serie «Pletinas», perfil «FL 20 x 4»): combo de serie (124) = **solo «Pletinas»**; perfiles (125)-(149): FL 20 x 4 ... FL 180 x 18 (331, TSV). Esta es la serie por defecto que ya figuraba en el análisis previo. |
| (150)-(156) | Tirante, forma «Redondo macizo» (tooltip (150): serie «R», perfil «R 10»): serie (151) = **solo «R»**; perfiles (152)-(156): R 10 ... R 150 (70). |
| (157)-(159) | Tirante, forma «Cuadrado macizo» (tooltip (157): serie «Barras cuadradas», perfil «SQ 90 x 90»): serie (158) = **solo «Barras cuadradas»**; perfiles (159): SQ 90 x 90, 100 x 100, 110 x 110, 120 x 120, 130 x 130, 140 x 140, 150 x 150, 160 x 160 (8). |
| (160)-(163) | **Describir material**, grupo «**Acero laminado**» (el título del grupo es «Acero laminado»): (160) estado inicial (radio «Material seleccionado en datos generales (S275 (UNE-EN 10025-2))» marcado, «Seleccionar material para la barra»); (161)-(163) combo del segundo radio abierto (ver §4). Botones Aceptar / Cancelar. |
| (164)-(166) | Describir material, grupo «**Hormigón para vigas de forjado**» (NO «Hormigón»): (164) «Material seleccionado en datos generales (C25/30)»; (165), (166) combo «**Tipo**» abierto (ver §4). |
| (167) | Diálogo «Describir» (Genérico, madera, Cuadrado macizo, serie Maciza, perfil S-100x100) con el aviso encima: **«Perfiles de madera»** (Aceptar): «Clase de servicio» radios 1 (marcado) / 2 / 3 con el texto «Clase de servicio 1: se caracteriza por un contenido de humedad en los materiales correspondiente a una temperatura de 20°C y una humedad relativa del aire que sólo exceda el 65% unas pocas semanas al año.» y casilla «Comprobar la resistencia al fuego (CTE DB SI. Anejo E)» (desmarcada). Tras Aceptar del 1.er diálogo. |
| (168), (169), (1) | Describir material, grupo «**Madera**» (radio «Material seleccionado en datos generales (C24)»): combo de tipo de madera (168) y combo «Clase resistente» (169) y (1) (ver §4). |

**No hay ninguna captura del diálogo (i) «Propiedades del perfil seleccionado» ni de datos E/G/ν.** Se verificó repasando las 169 una a una.

## 2. Listas de combos [CAP]

### 2.1 Series de perfiles
| Material / forma | Serie (por defecto = 1.ª) | Combo de serie abierto | Perfil por defecto |
|---|---|---|---|
| Armado / Doble T simétrica armada | IPC | **IPC** (única) | IPC 533x203x62.6 |
| Conformado / U conformado simple | U | **U** (única) | UF-60x3 |
| Conformado / U conformado rigidizado (fila 1, col. 3) | C | no abierto | CF-60x2.0 |
| Aluminio / Doble T | (EP) I | **no visto** | I-50x80x6 |
| Madera / Redondo macizo | Redondo | **Redondo** (única) | Ø100 |
| Madera / Cuadrado macizo | Maciza | **Maciza, DUO/TRIO, Laminada** | S-100x100 |
| Madera / Rectángulo macizo | Cabios/Viguetas | 32 entradas (ver mapa (25)-(29)) | 100x70 |
| Madera / Rectangular, canto variable | «Rectangular, canto vari...» (truncada) | **no visto** | RV-60 |
| Tirante / Angular simétrico | L | **L** (única) | L 35 x 35 x 4 |
| Tirante / Angular | L | **L** (única) | L 100 x 65 x 7 (a la hora de la captura) |
| Tirante / Platabanda | Pletinas | **Pletinas** (única) | FL 20 x 4 |
| Tirante / Redondo macizo | R | **R** (única) | R 10 |
| Tirante / Cuadrado macizo | Barras cuadradas | **Barras cuadradas** (única) | SQ 90 x 90 |
Todos los combos de serie se abren con la entrada actual resaltada y no tienen scroll salvo el de Rectángulo macizo (32 entradas, scroll vertical).

### 2.2 Perfiles (combo «Perfil»)
- **Armado IPC**: 85 perfiles, el nombre = «IPC » + AxBxP (alto x ancho x peso kg/m con 1 decimal; p. ej. «IPC 533x203x62.6», «IPC 2032x508x601.4»). Orden: por alto creciente, luego ancho (203, 305, 406, 508) y peso: ver TSV. Los 15 primeros: 533x203x62.6, 533x203x72.5, 533x305x77.9, 533x305x92.9, 533x305x107.9, 610x203x67.4, 610x203x77.2, 610x305x82.6, 610x305x97.6, 610x305x112.6, 686x203x72.1, 686x203x82.0, 686x305x87.4, 686x305x102.4, 686x305x117.4. Aviso: a partir de (7) a (8) no hay elemento solapado (contigüidad asumida).
- **Conformado U (UF)**: UF-60x3, UF-60x4, UF-80x3, UF-80x4, UF-80x5, UF-100x3, UF-100x4, UF-100x5, UF-120x4, UF-120x5, UF-120x6, UF-140x4, UF-140x5, UF-140x6.
- **Aluminio (EP) I**: I-50x80x6, I-50x80x8, I-50x108x5, I-50x108x8, I-60x80x4, I-60x80x6, I-60x80x8, I-82x46.5x3, I-100x100x10, I-150x200x7, I-150x200x13.
- **Madera**: Redondo Ø100, Ø120, Ø130, Ø140, Ø150, Ø180, Ø200; Cuadrado: tres series (§1); Rectángulo: `Cabios/Viguetas` (11) y 31 series de Rectángulo macizo: «Maciza hN» (N = 100...280, prefijo «S-NxB», B en 40, 50, 60, 80, 100, 120...N; para h260 y h280 B empieza en 60), «DUO/TRIO hN» (N = 100...260 y 270; «DUO-NxB» B = 60, 80, 100, 120, 140, 160 (para h270: 120, 140, 160), más «TRIO-NxB» B = 180 (h180); 180, 200, 240 (h200, h220, h240, h260); 180, 200 (h270)) y «Laminada bB» (B = 60...260 paso 20; perfiles «GL-AxB» con A = 160...1280 paso 20, 57 por serie; las capturas de b60 no muestran GL-740...GL-840: paso 20 supuesto para esos 6 elementos, el resto de series (b80...b260) sí los muestran). Rectangular de canto variable: RV-60 ... RV-260 (paso 20, 11).
- **Tirante**: L (angular simétrico): 179 perfiles «L a x a x t» desde «L 35 x 35 x 4» hasta «L 300 x 300 x 35» (fracciones con punto: «L 45 x 45 x 4.5», «L 63 x 63 x 6.5»); L (angular): 31 perfiles «L a x b x t» desde «L 100 x 65 x 7» hasta «L 250 x 90 x 16»; Pletinas: «FL b x t» de «FL 20 x 4» a «FL 180 x 18»; R: «R 10» ... «R 22.25», «R 23.6», ... «R 150» con decimales (22.25, 23.6, 24.5, 26.7, 29.5, 29.7, 34.4, 35.7, 39.2, 49.2, 55.8); SQ: 8 perfiles. Todo en el TSV.
- Los nombres de perfil usan «x» minúscula con espacios solo en los tirantes («L 35 x 35 x 4»); en madera/armado/conformado/aluminio sin espacios.

### 2.3 Estado de los iconos en tirante
5 formas (tooltips [CAP]): **Angular simétrico**, **Angular**, **Platabanda**, **Redondo macizo**, **Cuadrado macizo**. Cada una con su serie única y la barra de herramientas del visor «CÁLCULO DE TIRANTES EN CYPE 3D» («1 de 3», Imprimir..., engranaje).

## 3. Datos adicionales [CAP]
- Conformado: «Disposición» con 7 radios: Perfil simple (marcado), Doble en cajón soldado, Doble en cajón con presillas, Doble en cajón unión genérica, Doble en I unión soldada, Doble en I con presillas, Doble en I unión genérica (confirma §5 de `describir_barras.md`).
- Aluminio, madera: solo «Perfil simple». Madera Rectangular canto variable: «Perfil simple» + grupo «Canto del perfil» con «Canto inicial» y «Canto final» (ambos **100.0 mm**, 1 decimal, unidad mm).

## 4. Describir material [CAP]
- **Acero laminado** (grupo con cabecera «Acero laminado»; combo sin etiqueta bajo el radio): 18 entradas, sin separadores: S235, S275, S355, S450, S275M, S355M, S420M, S460M, S235 (UNE-EN 10025-2), S275 (UNE-EN 10025-2), S355 (UNE-EN 10025-2), S450 (UNE-EN 10025-2), S275M (UNE-EN 10025-4), S355M (UNE-EN 10025-4), S420M (UNE-EN 10025-4), S460M (UNE-EN 10025-4), S355 HISTAR®, S460 HISTAR®. Al marcar «Seleccionar material para la barra» el combo aparece con **S235** (1.ª) y el radio primero ya no está en negrita. Radio 1: «Material seleccionado en datos generales (S275 (UNE-EN 10025-2))».
- **Hormigón**: el grupo se llama «**Hormigón para vigas de forjado**» (no «Hormigón»). Radio 1: «Material seleccionado en datos generales (C25/30)»; combo «**Tipo**» (defecto C12/15, 1.º): C12/15, C16/20, C20/25, C25/30, C30/37, C35/45, C40/50, C45/55, C50/60, Bc 15, Bc 20, Bc 25, Bc 30, Bc 35, Bc 40, Bc 50, Bc 60 (17; «Bc 45» no existe; fin de lista: la barra de scroll está al final en (166), «Bc 60» es el último visible).
- **Madera**: grupo «Madera», radio 1: «Material seleccionado en datos generales (C24)»; al marcar el 2.º aparece un combo de **tipo** (sin etiqueta; defecto «Aserrada, procedente de coníferas o chopos»): **Aserrada, procedente de coníferas o chopos; Aserrada, procedente de especies frondosas; Laminada encolada, homogénea; Laminada encolada, combinada**, y debajo «**Clase resistente**» (defecto C24 en el radio 1; con el 2.º radio la caja mostró «C24» al abrirse y «C35» en la 2.ª captura): para «coníferas o chopos»: **C14, C16, C18, C20, C22, C24, C27, C30, C35, C40, C45, C50** (12). Las clases de los otros 3 tipos no se capturaron.

## 5. Diálogo (i) «Propiedades del perfil seleccionado» y TK-063
- (i): **sin capturas** (madera, aluminio, conformado, armado: ninguna). Solo existe la medición previa de IPE 80 (`orig_describir_info_ipe80.png`).
- TK-063 (E, G, ν, dilatación, peso de hormigón y madera): **ninguna captura los muestra** (el diálogo «Perfiles de madera» solo trae clase de servicio y resistencia al fuego; «Describir material» solo nombra clases). Sin datos nuevos.

## 6. Comparación con lo pedido (2026-11-05)
Cubierto: series armado, conformado (U), madera redondo/cuadrado/rect (y todos los perfiles de cada serie de rect), perfil madera redondo, tirante (4 formas más Pletinas), Describir material acero y hormigón, combo de tipo y clase de madera (solo coníferas/chopos), «Perfiles de madera».
**Faltan exactamente:**
1. **Los 4 diálogos (i)**: madera (Redondo Ø100), aluminio, conformado, armado (`orig_describir_info_madera.png`, `..._aluminio.png`, `..._conformado.png`, `..._armado.png`): 0 de 4.
2. **Serie de perfiles de aluminio** (`orig_describir_dd_serie_aluminio.png`): el combo no llegó a abrirse ((15) lo muestra cerrado). Solo se sabe que la serie activa es «(EP) I».
3. **Serie de madera Rectangular de canto variable** (`..._rectvar`): combo no abierto y nombre truncado «Rectangular, canto vari...» (el nombre completo no se ve).
4. **Describir material de madera: «Clase resistente» de los otros 3 tipos** (frondosas, laminada encolada homogénea, laminada encolada combinada) y las etiquetas del combo de tipo.
5. (opcional) serie de conformado «C» (U conformado rigidizado) y las series de las otras 14 formas de conformado y 8 de armado/aluminio: no pedidas.
6. Opcional pedido: aviso del genérico con A = 25 / Avy = 100; tooltips de «Tipo» de viga en otras familias: no hay capturas.

## 7. Segunda tanda (2026-11-05 18:04-18:12): diálogos (i), combos de aluminio y canto variable, clases de madera
Datos: `data/describir_info_perfiles.tsv` (44 filas de los 4 diálogos (i)), `data/describir_combos_nuevos.tsv`, `data/ec5_clases_madera_rea.tsv` (tabla REA, ver 7.4). Etiquetas: [CAP] captura, [REA] código del original, [WEB] fuente externa, [CALC] verificación numérica propia (Python).

### 7.1 Diálogos (i) «Propiedades del perfil seleccionado» [CAP]
Ventana roja con dibujo acotado a la izquierda y caja gris de filas «etiqueta: valor unidad» (valores en negrita), botón Aceptar. Las filas varían por forma (literal, con sus decimales):

| Forma / serie / perfil | Filas (de arriba abajo) |
|---|---|
| Madera / Redondo / Redondo / Ø100 (`info_madera.png (1)`; la `(2)` es solo el combo de perfil ya conocido) | Diámetro 100.0 mm; Área 78.54 cm2; Inercia a flexión 490.87 cm4; Módulo plástico 166.67 cm3; Inercia a torsión 981.75 cm4 |
| Aluminio / Doble T / (EP) I / I-50x80x6 | Canto total 50.0 mm; Anchura del ala 80.0 mm; Espesor del alma 6.0 mm; Espesor del ala 6.0 mm; Área 11.88 cm2; Inercia a flexión Iyy 49.50 cm4; Módulo plástico Zyy 23.29 cm3; Inercia a flexión Izz 51.27 cm4; Módulo plástico Zzz 19.54 cm3; Inercia a torsión 1.43 cm4; Módulo de alabeo 248.14 cm6; Peso 3.21 kg/m |
| Conformado / U conformado simple / U / UF-60x3 | Canto total 60.0 mm; Anchura del ala 30.0 mm; Espesor 3.0 mm; Radio de acuerdo interior 3.0 mm; Área 3.30 cm2; Coordenada 'y' del centro de gravedad -6.1 mm; Inercia a flexión Iyy 17.50 cm4; Inercia a flexión Izz 2.82 cm4; Inercia a torsión 0.10 cm4; Área de cortante y 1.42 cm2; Área de cortante z 1.42 cm2; Módulo de alabeo 14.78 cm6; Peso 2.59 kg/m |
| Armado / Doble T simétrica armada / IPC / IPC 533x203x62.6 | Canto exterior del alma 533.4 mm; Anchura del ala 203.2 mm; Espesor del alma 7.9 mm; Espesor del ala 9.5 mm; Área 79.25 cm2; Inercia a flexión Iyy 35455.70 cm4; Inercia a flexión Izz 1330.56 cm4; Inercia a torsión 20.07 cm4; Área de cortante y 28.96 cm2; Área de cortante z 36.57 cm2; Módulo de alabeo 912999.76 cm6; Módulo plástico Zyy 1533.94 cm3; Módulo plástico Zzz 204.15 cm3; Peso 62.21 kg/m |

Decimales: longitudes 1 decimal (mm); áreas, inercias, módulos y peso 2 decimales; coordenada del CG 1 decimal. Superíndices (cm², cm⁴, cm³, cm⁶) en la captura. Cotas del dibujo: aluminio 80 / 50 / 6 / 6 (enteros); conformado 30 / 60 / 3; armado 203.2 / 533.4 / 9.5 / 7.9. Notas: el armado dice «Canto EXTERIOR del alma» (en el cálculo es el alto total) mientras aluminio y conformado dicen «Canto total»; el conformado no muestra módulos plásticos y el aluminio no muestra áreas de cortante; el armado tiene 14 filas, el aluminio 12, el conformado 13, la madera redonda 5. El dibujo del armado se muestra sobre la pestaña con «Datos adicionales» (Disposición: Perfil simple / Con losa de hormigón; casilla «Hay rigidizadores de alma»). IPE 80 ya existía: `orig_describir_info_ipe80.png`.

### 7.2 Cómo se calculan las propiedades [CALC]
**Doble T armada y aluminio (exacto a 2 decimales), con h = canto, b = ancho de ala, tw, tf:**
- A = 2·b·tf + (h − 2·tf)·tw -> armado 79.2456 cm2 (muestra 79.25), aluminio 11.88.
- Iyy = [b·h³ − (b − tw)(h − 2tf)³]/12 -> 35455.70 cm4 y 49.50 (exactos).
- Izz = [2·tf·b³ + (h − 2tf)·tw³]/12 -> 1330.56 y 51.27.
- It = [2·b·tf³ + (h − 2tf)·tw³]/3 (suma de rectángulos finos) -> 20.07 y 1.43.
- Alabeo Iw = Izz·(h − tf)²/4 -> 912999.76 cm6 (exacto, 8 cifras) y 248.14.
- Zyy = b·tf·(h − tf) + tw·(h − 2tf)²/4 -> 1533.94 y 23.29; Zzz = tf·b²/2 + (h − 2tf)·tw²/4 -> 204.15 y 19.54.
- Armado: Avy = 0.75·(2·b·tf) = 1.5·b·tf = 28.956 -> 28.96; Avz = 0.9·(h − 2tf)·tw = 36.574 -> 36.57 (los coeficientes 0.75 y 0.9 se deducen y coinciden a 2 decimales con UN solo perfil).
- Peso: armado = A·0.785 (ρ = 7850 kg/m3; 62.21 kg/m, NO el 62.6 del nombre); aluminio = A·0.27 (ρ = 2700 kg/m3; 3.2076 -> 3.21).
- **No deducible de la designación:** tw y tf. «IPC 533x203x62.6» da h = 533.4 (21 in), b = 203.2 (8 in) y un peso nominal 62.6 distinto del calculado; tw 7.9 y tf 9.5 solo se conocen del (i). Hace falta una tabla de espesores por designación (85 IPC sin datos). Aluminio: «I-50x80x6» = h 50, b 80, t 6 (tw = tf = 6 en este único caso; no se sabe si es general para las otras 10).

**Conformado UF-60x3 (aproximado, no exacto):** modelo de línea media con t = 3, radio interior 3 (radio medio 4.5), ala de 30 medida desde la cara exterior del alma: A = 3.304 cm2 (3.30 OK), peso 2.594 (2.59 OK), It = L·t³/3 = 0.099 (0.10 OK), y_g = x_cg − b/2 = 8.90 − 15 = −6.10 mm (OK: distancia del CG al centro de la anchura de 30 mm). Pero Iyy sale 17.54 (muestra 17.50) e Izz 2.834 (2.82): error 0.2-0.5 %; ninguna discretización del arco en n tramos reproduce ambos (n = 3 da Izz 2.822, Iyy se ajustaría con n≈4.5). Avy = Avz = 1.42 cm2 sin explicar (alma recta 48 mm -> 1.44); alabeo 14.78 cm6 no reproducido (la fórmula cerrada con esquinas vivas da 16.4). Conclusión: **A, peso, CG e It se pueden calcular; Iyy/Izz/Avy/Avz/Iw solo exactos tabulando** (o aceptando ≈0.5 %). Tampoco se conoce la regla de b y radio por designación (UF-60x3: b = 30 = h/2, r = t; un único dato).

**Madera redondo:** círculo exacto: A = πd²/4, I = πd⁴/64, Z = d³/6, It = πd⁴/32 = 2·I.

### 7.3 Combos nuevos [CAP]
- **Serie de aluminio** (forma Doble T): una sola entrada, **«(EP) I»**. Perfiles (11): I-50x80x6, I-50x80x8, I-50x108x5, I-50x108x8, I-60x80x4, I-60x80x6, I-60x80x8, I-82x46.5x3, I-100x100x10, I-150x200x7, I-150x200x13 (igual que `describir_perfiles_aluminio_doble_t.tsv`).
- **Serie de madera Rectangular de canto variable**: nombre completo **«Rectangular, canto variable»**, única entrada. Perfiles (11): RV-60, RV-80, RV-100, RV-120, RV-140, RV-160, RV-180, RV-200, RV-220, RV-240, RV-260. Datos adicionales: «Disposición: Perfil simple»; «Canto del perfil»: Canto inicial 100.0 mm, Canto final 100.0 mm.

### 7.4 Clase resistente de madera (Describir material) [REA] + [WEB]
Hallado SIN GUI en `ec5.dll` (`ec5tipos_texto_clase` @0x180002330, `ec5tipos_es_clase_resistente_poco_frecuente` @0x180002100, tablas por clase @0x180001000/1290/1530/17d0/1ba0/1e50) y `pzmadera.dll` (selector `i_datos_madera_ec5_t`, defecto `CONIFERAS_C24`, identificadores `CONIFERAS_*`, `FRONDOSAS_*`, `HOMOGENEAS_*`, `COMBINADAS_*` @0x180038f10-0x1800391b8; etiquetas de tipo ids 141095-141098). Enumeración única de 36 clases (índice 0-35) **[REA, exacta]**:
- **Aserrada, procedente de coníferas o chopos (12):** C14, C16, C18, C20, C22, C24, C27, C30, C35, C40, C45, C50 (coincide 1:1 con el combo capturado).
- **Aserrada, procedente de especies frondosas (8):** D18, D24, D30, D35, D40, D50, D60, D70.
- **Laminada encolada, homogénea (8):** GL20h, GL22h, GL24h, GL26h, GL28h, GL30h, GL32h, GL36h.
- **Laminada encolada, combinada (8):** GL20c, GL22c, GL24c, GL26c, GL28c, GL30c, GL32c, GL36c.
Certeza: lista y orden por tipo exactos (REA). Que el combo de cada tipo muestre todas (sin filtrar) se infiere de la coincidencia 12/12 en coníferas; no visto en pantalla. Marca «poco frecuente» [REA]: frecuentes = C18, C20, C22, C24, D18, D24, D40, GL24h, GL28h, GL28c; el resto poco frecuentes (uso exacto en la GUI no confirmado, quizá aviso).
**Propiedades por clase [REA]** (`data/ec5_clases_madera_rea.tsv`: E0,mean GPa, G mean GPa, ρ mean kg/m3): hay DOS juegos, elegidos por un parámetro `short` de la función 0x180007f40 (valor 1 -> segundo juego). El juego «segundo» (E_f1290, G_f17d0, R_f1e50) coincide exactamente con CTE DB SE-M Anejo E [WEB normatia.com]: p. ej. C27 11.5/0.72/450, D30 12/0.75/640, D35 12/0.75/650, D40 13/0.81/660, GL24h E 11.6 G 0.72, GL24c G 0.59, GL36c 14.7/0.85. El juego «primero» (E_f1000, G_f1530, R_f1ba0) difiere (C27 12/0.75, frondosas D30-D40, laminadas con G 0.65 casi constante, GL24h E 11.5), parece una edición EN 338/EN 14080 más moderna; sin confirmar. No resuelto cuál usa el diálogo por defecto (falta mirar quién llama con parámetro 1). Los ρ de laminada son de CYPE (el CTE no da ρ medio). EN 338:2016 incluye además D27, D45, D55, D65, D75, D80 que CYPE no ofrece. fm,k, ft,k, fc,k etc.: no están en estas tablas.

### 7.5 TK-063 / TK-074
- **Madera:** E0,mean, G, ρ en 7.4 [REA]; ν y dilatación no hallados.
- **Hormigón:** búsqueda estática de dobles/enteros (27264, 31, 31000, 31476) en `datmater.dll`, `ed3dlib.dll`, `nbrmadera.dll`, `ec2comp.dll`, `basidmat.dll`: sin tabla reconocible. **No resuelto**; probablemente fórmula de norma en otra DLL (EHE/EC2 sin explorar).

### 7.6 Cierre por REA de los huecos (2026-11-05, solo estático) [REA]
**Tablas de perfiles (hueco 2): RESUELTO.** Están en `perfil3.dll` como código (constantes inmediatas), no como fichero: función `FUN_18002cfe0` @0x18002cfe0 (85 IPC), `FUN_18003b720` @0x18003b720 (serie «(EP) I», 11), `FUN_1801fde40` @0x1801fde40 (serie «U» UF, 14), `FUN_1801fcdc0` @0x1801fcdc0 (serie «C» CF, 36). Volcados (valores doubles decodificados, certeza REA exacta):
- `data/describir_perfiles_ipc_rea.tsv`: designación, h, b, tw, tf (mm), A (cm2) e Iyy (cm4) tabulados (solo en 4 filas de control; A tabulada = calculada exacta, Iyy tabulada redondeada a entero) y todas las propiedades calculadas con las fórmulas de 7.2. Orden de argumentos en el original: (nombre, h, b, tw, tf, A, Iyy). h = 21, 24, 27, 30, 33, 42, 46, 50, 54, 58, 62, 66, 70, 75, 80 in x 25.4 (533.4 ... 2032), b = 203.2, 304.8, 406.4, 508. IPC 533x203x62.6: tw 7.9, tf 9.5 (coincide con el (i)). El peso del (i) es A·0.785, no el nominal del nombre. Los A e Iyy tabulados coinciden con la fórmula (control en 4 perfiles, p. ej. IPC 2032x508x601.4: A 761.29, Iyy 5 827 116 cm4 tab. frente a 5 827 128.96 calc.); el diálogo (i) muestra el valor CALCULADO (35455.70) y no el tabulado (35455).
- `data/describir_perfiles_aluminio_rea.tsv`: (nombre, h, b, tw, tf, Iyy, Izz) tabulados; en los 11 perfiles tw = tf = t y las Iyy/Izz tabuladas coinciden con la fórmula a 2 decimales; peso = A·0.27 (ρ = 2700). Todo lo mostrado en el (i) se calcula (A, Iyy, Izz, Zyy, Zzz, It, Iw, peso).
- `data/describir_perfiles_conformado_u_rea.tsv` (UF): (nombre, h, b = h/2, t, r_interior, A, Iyy) TABULADOS. UF-60x3: A 3.30, Iyy 17.50 (valor tabulado = el del (i); por eso mi modelo de línea media daba 17.54). Radio interior r = 3, 6, 8, 10 para t = 3, 4, 5, 6. Izz, It, Avy, Avz, alabeo y CG del (i) no están tabulados en este bloque (solo A e Iyy): calculados por el original con una fórmula no reproducida (Izz 2.82 frente a 2.83 mío).
- `data/describir_perfiles_conformado_c_rea.tsv` (CF, serie «C»): (nombre, h, b, c_labio, t, r_interior, A, Iyy, Izz) con la interpretación de las 3 últimas columnas deducida por analogía con UF (A, Iyy, Izz), no comprobada con una captura. 36 perfiles (CF-60x2.0 ... CF-300x4.0); en el combo de perfil de esta serie ya se vio solo el primero.
**Hormigón (hueco 3): RESUELTO para E, ν, dilatación y peso.** `datmater.dll` (`datmater_lee_datmater_hormigon_con_biblioteca_usuario_opcional` @0x180011c80) carga por norma un bloque de texto embebido (`datmatNN_datos()`, ~50 bloques, todos en `dmatndll.dll`; 17 tipos con «Bc» = bloque «Norma Eurocódigo 2», 0x5b070). Contenido (`data/hormigon_eurocodigo2_rea.tsv`): C12/15..C50/60 (sin C28/35 y C32/40) y Bc 15..Bc 60, con fck, γc y **E (MPa)**: C12/15 27085, C16/20 28608, C20/25 29962, **C25/30 31476**, C30/37 32837, C35/45 34077, C40/50 35220, C45/55 36283, C50/60 37278, Bc 15 27287 ... Bc 60 36283 (coincide con 22·(fcm/10)^0.3 GPa, fcm = fck + 8: C25/30 -> 31.476). Los 17 tipos son exactamente la lista capturada en «Describir material» (certeza alta de que ese es el bloque que usa el diálogo; no se ha mirado qué norma del proyecto selecciona qué bloque). **ν = 0.2** (`datmater_modulo_poisson_hormigon`, constante), **dilatación α = 1.0e-5 1/°C** (`datmater_coeficiente_dilatacion_termica_hormigon`, 0x3ee4f8b588e368f1), **peso específico = 2.5 T/m3** (fijo: se carga 0x4004000000000000 en todos los tipos). G = E/(2(1+ν)) (`cypemat_modulo_cortadura`, el nombre y su uso para acero). Unidades internas kp/cm2 (E en MPa x 10.1937), `Ec_T_por_m2 = E x 10` (comprobación 9000 < Ec < 6e6). Corrige TK-074: E de C25/30 = 31476 MPa (no 31000 ni el valor de la tabla 3.1).
**Madera (hueco 4): RESUELTO en lo esencial.** `eurocod5_E0medio(clase, short)` @ec5.dll 0x180006e30 se llama desde `pzmadera.dll` (0x18000e710 E0, 0x18000e970 E0+G) con `norma` = param_3: **1, 4, 5 -> EC5 con segundo argumento 0 (juego «primero»)**, **2 -> NBR 7190 (Brasil)**, **3 -> CTE con segundo argumento 1 (juego «segundo» = CTE DB SE-M Anejo E)**, 6 y 7 -> madera definida por el usuario (`FUN_180026e30`). El módulo de Poisson se DERIVA: ν = E0/(2·G) − 1 (`cypemat_modulo_poisson`, basicos.dll, error si ≤ 0): da ≈ 6.97 para C24, ≈ 7.00 para casi todas las clases (columnas `nu_*` del TSV; valor poco físico pero es lo que calcula el código; no se sabe si el 3D lo usa). Unidades: E, G en kN/mm2 (GPa) -> T/m2 (`magnitud_kN_mm2_a_Tn_m2`). **No resuelto por REA:** qué valor de `norma` pasa el proyecto español por defecto (probable: 3 = CTE, que además da la lista por defecto C24; sin confirmar el mapeo número -> norma). Se recomienda implementar ambos juegos y elegir CTE (juego «segundo») para norma Española.

### 7.7 Integración en el clon (2026-11-05)
Ver `describir_barras.md` §16: IPC, aluminio, UF y CF calculables (exacto / aproximado / deducido según el caso), (i) con las filas medidas, hormigón con E exacto, clases de los 4 tipos de madera y los dos juegos de propiedades.
