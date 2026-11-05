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
