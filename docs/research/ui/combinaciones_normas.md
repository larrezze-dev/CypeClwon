# Combinaciones por norma (gamma, psi) y situaciones de proyecto (T-P7-06)

Fecha: 2026-11-10. Vía: **solo REA** (sin lanzar el original). Binarios en `D:\CYPE Ingenieros\Versión 2027\CYPE 3D`. Direcciones = imagen base `0x180000000` (Ghidra). Etiquetas: **[V]** = leído en pseudocódigo/cadenas de la DLL; **[N]** = deducido por el nombre de la función o por analogía (no verificado); **[GUI]** = solo visible en la interfaz (pendiente).

## 1. Mapa de DLL (qué hace cada una) [V]

| DLL | Contenido relevante |
|---|---|
| `sestlim.dll` | Datos de «Estados límite» de la obra: `sestlimge_*` (CYPE 3D / general: hormigón, cimentación, acero conformado, acero laminado, madera, aluminio, terreno, desplazamientos), `sestlimte`, `sestlimgp`, `sestlimha`, `sestlimlam`, `sestlimci`; lectura/escritura (`_lee`, `_graba`), `_crea_por_defecto`, paneles, textos «según norma». Única DLL que referencia `shc*` aparte de `sithipcmb`. |
| `sithipcmb.dll` | «Situaciones de hipótesis y combinaciones»: 221 exports. `shc<norma>_*` = tabla de situaciones de proyecto (nombre + gamma + psi) de cada norma/estado límite; `shcsituac_*` = utilidades y listados; `shcsitusr_*` = situaciones propias del usuario (crea/copia/lee/graba/panel); `shctextos_*` = textos de estado límite. |
| `ctesebas.dll` | Coeficientes CTE DB-SE (`ctsecomb_coef_*`), categorías de uso CTE (`ctsetipo_*`), nieve/viento CTE. |
| `eurocod.dll` | Coeficientes EN 1990 (`eccomb_EN_1990_2002_coef_*`), selección de anejo nacional por norma de material (`eccomb_anejo_segun_norma_*`), pretensado. |
| `ehe.dll` | EHE-08 (`ehe08cmb_*`), EHE-98 (`ehecomb_*`), EH-91 (`eh91comb_*`). |
| `gestcomp.dll` | **Generador de combinaciones** (`gccombs_*`): expansión de situaciones + hipótesis en combinaciones, filtrado, coeficientes instantáneos / temporal / largo plazo. |
| `combs3.dll` | Hipótesis de usuario (`cb3hipusr_*`), entrada `cb3hipusr_combinaciones_sin_sismo_dinamico` (0x1800269a0) y `_con_sismo_dinamico` (0x180026330), que llaman a `FUN_180033280` -> `gestcomp`. Paneles de disposiciones/combinatoria. |
| `combs.dll` | Resultados/esfuerzos por combinación (`cmbtipos_*`, `cypecomb_*`), estabilidad global (P-delta), nombres cortos de naturalezas. |
| `combsanl.dll` | Combinaciones no lineales (`cnlcombs_*`), modal, pushover. |
| `docucmb.dll` | Listados «Combinaciones para estados límite últimos / de servicio», «Listado de coeficientes». |
| `catuso.dll` | Diálogo «Categorías de uso». `ghipocal.dll` = datos de hipótesis (`hipocal_*`, `cypecomb_nombre_corto_naturaleza`). |
| `cynormpais.dll`, `cypenorma.dll` | Lista de países y de normas por material (nombres de norma en 6 idiomas). |

## 2. Situaciones de proyecto: estructura de datos [V parcial]

Constructor común `FUN_18007e030` -> `FUN_180078580` -> `FUN_180078920` -> `FUN_180077df0` (sithipcmb). Orden de argumentos observado en todas las llamadas (`shcctese_elu`, `shceurocod_*fisuracion`, `*flecha*`):

`(nombre, gamma_perm_fav, gamma_perm_desfav, 4 valores de pretensado [pretesa fav/desf, postesa fav/desf], flag, gamma_perm_no_cte_fav, gamma_perm_no_cte_desf, flag, &coefs_sobrecarga, flag, flag, flag,`
`viento{principal, acompanyamiento, gamma_var_fav, gamma_var_desf}, flags(0,1), nieve{principal, acompanyamiento, gamma_fav, gamma_desf}, flags(1,0,1), temperatura{principal, acompanyamiento})`

- «principal» y «acompañamiento» son los coeficientes psi con que **actúa la variable cuando es la principal / cuando es de acompañamiento** (cadenas #153590 «Principal (%s)», #153591 «Acompañamiento (%s)»). En las situaciones persistentes: principal = 1.0, acompañamiento = psi0. En ELS característica: idem. En ELS frecuente: principal = psi1, acompañamiento = psi2. En cuasipermanente y fisuración: ambos = psi2. En accidental: principal = psi1, acompañamiento = psi2. En sísmica: ambos = psi2.
- La sobrecarga se pasa como array por categoría de uso (`stlcatusoob_cte_y_ehe_coefs_sobrecarga`, `stlcatusoob_eurocodigo_coefs_sobrecarga`) con tres punteros-función: «principal», «psi acompañamiento» y gamma fav/desf.
- Validaciones de la GUI de situaciones propias [V cadenas]: #153667 «No puede introducir un coeficiente favorable mayor que el desfavorable.»; #153668 «No puede introducir un coeficiente principal menor que el de acompañamiento.»; #153669 «El valor de los coeficientes de sismo no puede ser cero.»; #153674 «El valor de los coeficientes de las hipótesis accidentales no puede ser cero.»; #153580 «No puede repetir el nombre de una situación de proyecto.»; #153581 «El nombre de una situación de proyecto no puede quedar en blanco.»; #153641/#153642/#153643 «Se debe definir, al menos, una situación con sismo / accidental de incendio / persistente o transitoria.»; #153657 «Las situaciones siguientes no están definidas: %s.»; #156481 «Debe introducir, al menos, una situación de proyecto.»; #156482 «¿Desea borrar la situación de proyecto seleccionada?»; #156483 «Debe seleccionar una situación de proyecto.»; #156484 «No es posible borrar esta situación de proyecto, ya que está siendo usada para la comprobación de otros estados límite.»; #156464/#156465 (sismo / incendio exigidos si hay acciones sísmicas o comprobación de fuego); #156467 «Las situaciones seleccionadas para '%s' y '%s' deben estar definidas con las mismas categorías de uso.»; #156485 «Crear a partir de las situaciones de la norma.».

### 2.1 Nombres de situaciones y textos de la GUI (es-ES, con id)
- Tipos: «Persistentes o transitorias» #41545; «Accidentales» #41544; «Situaciones persistentes o transitorias» #117812; «Situaciones sísmicas» #154753; «Situaciones accidentales» #43543; «Persistente o transitoria» #46225; «Sísmica» #46226; «Accidental no sísmica» #46227; «Accidental» #186738 / #153617; «Accidental de incendio» #153644; «Situación accidental de incendio» #153672; «Situación sísmica» #153671; «Situación accidental» #128125; «Pretensado en vacío» #153676; «Situación de proyecto definida por el usuario» #153579; «Situaciones de proyecto» #52750; «Situaciones de proyecto introducidas por el usuario» #46232; «Situaciones» #46217; «Sin sismo» #46230 / «Con sismo» #46231; «Nombre de la situación» #153660; «Combinación de acciones» #41562; «Particulares» #156477.
- Estados límite: «E.L.U. de rotura. Hormigón / Hormigón en cimentaciones / Pilares mixtos de hormigón y acero / Acero conformado / Acero laminado / Madera / Aluminio / Tensión del terreno» #153629-#153635, #153646; «E.L.S. Tensión del terreno» #153647; «E.L.S. Fisuración. Hormigón / Hormigón en cimentaciones» #153649/#153650; «E.L.S. de fisuración» #153652; «Estado limite último de rotura» #153653; «Estado limite de servicio de fisuración» #153654; «Estado limite de servicio» #153655; «Estados límite últimos» #175771; «Estados límite de servicio» #175636; «Estados límite a verificar» #153525; «Configurar combinaciones para cada estado límite» #153526; ayuda #153527 «Permite establecer unos coeficientes de combinación particulares para cada estado límite.»; «Se utiliza la configuración de las combinaciones establecida en los datos generales.» #156479; «Se establece una configuración particular de las combinaciones para este estado límite.» #156480; «Acciones características» #46218; «Cota de nieve» #153524; tipos de duración/ELS: «Cuasipermanente» #46258, «Frecuente» #46257, «Largo» #63550, «Corto» #153623, «Carac.» #39661, «Cuasi.» #39663, «PI.» #39659, «ALP.» #39660, «PLI.» #39656.
- Etiquetas de coeficientes: «Coeficientes parciales de seguridad» #52743; «Coeficientes parciales de seguridad (%s)» #153585; «Favorable» #153586; «Desfavorable» #153587; «Coeficientes de combinación» #153636; «Introducir coeficientes de mayoración» #153583; «Introducir coeficientes de mayoración y combinación» #153584; «Con coeficientes de combinación» #387482 / «Sin coeficientes de combinación» #387483; naturalezas: «Acción permanente» #153595, «Acción de pretensado» #153596, «Acción variable» #153597, «Acción accidental» #153612, «Acciones variables sin sismo» #153578.
- Ayudas de coeficientes (#153599-#153606, #153608-#153610): «Coeficiente parcial de seguridad de las acciones permanentes / de la acción de pretensado / de la acción variable principal / de las acciones variables de acompañamiento / de la acción sísmica / de la acción accidental», «Coeficiente de combinación de la acción variable principal / de las acciones variables de acompañamiento», «Fracción de las solicitaciones sísmicas a considerar en la dirección ortogonal», «Las solicitaciones obtenidas de los resultados del análisis en cada una de las direcciones ortogonales se combinarán con el %s de los de la otra.», «Para cada situación de proyecto y estado límite los coeficientes a utilizar serán:», «Para las distintas situaciones de proyecto, las combinaciones de acciones se definirán de acuerdo con los siguientes criterios:» #153592.
- Listados (docucmb): «Nombres de las hipótesis» #39149; «Listado de coeficientes» #39151; «Coeficientes para situaciones persistentes o transitorias» #39152; «... y sísmicas» #39153; «... y accidentales» #39154; «... sísmicas y accidentales» #39155; «Coeficientes para situaciones accidentales de incendio» #39156; «Combinaciones para estados límite últimos» #39187; «... de servicio» #39188. Texto de ayuda de listado de situaciones #156478.
- Sísmica ortogonal: #39213/#39214 (ver `hipotesis_carga.md`); sobrerresistencia: `shcasce_7_10_situaciones_sismicas_con_sobrerresistencia`, `sestlimge_coeficientes_acero_laminado_factor_sobrerresistencia`, clases BF/MF [N].
- Hipótesis automáticas combinables: «Considerar combinaciones con viento / sismo» (Cimentación) #45589-#45592; «Considerar efectos de segundo orden» #46224; nombres cortos de naturaleza (combs): «G» #46210 / #286317, «CM» #46211, «Q» #46212, «W» #46213 (viento; id compartido con «SW»?), «Sismo» #53936, «Nieve» #53937, «Viento» #74786, «Retracción» #65414, «Helada» #158443, «Peso y presión de fluidos» #158444, «Empujes del terreno» #153614, «Postesado» #157114, «Carga permanente» #46260, «Sobrecarga de uso» #25163, «Sobrecarga» #46261, «ecc.+» #158430 / «ecc.-» #158431, «Copia de %s» #74963. Regla exacta de nombres de combinación (columna «Comb. n»): **[GUI]** (ver §8).

## 3. Coeficientes CTE DB-SE (`ctesebas.dll`, `ctsecomb_*`) [V]

Todos los valores leídos de las constantes `double` del pseudocódigo (0x3ff599999999999a = 1.35, etc.). Cada función además informa el artículo de la norma («4.2.2»).

### 3.1 Gamma (CTE DB SE 4.2.2)
| Acción | Persistente/transitoria | Sísmica | Accidental |
|---|---|---|---|
| Permanente favorable | 0.80 | 1.00 | 1.00 |
| Permanente desfavorable | 1.35 | 1.00 | 1.00 |
| Variable favorable | 0.00 | 0.00 | 0.00 |
| Variable desfavorable | 1.50 | 1.00 | 1.00 |
| Empuje del terreno favorable | 0.70 | (usa permanente sísmica) | — |
| Empuje del terreno desfavorable | 1.35 | — | — |
| Acción sísmica (+/-) | — | +1.00 / **-1.00** (`acc_pos/neg_sismica`) | — |
| Acción accidental | — | — | 1.00 |

### 3.2 Psi (viento, temperatura)
| Acción | psi0 | psi1 | psi2 |
|---|---|---|---|
| Viento | 0.6 | 0.5 | 0 |
| Temperatura | 0.6 | 0.5 | 0 |

### 3.3 Psi nieve (índice = «Cota de nieve»: 0 = «Altitud superior a 1000 m» #46242, 1 = «Altitud inferior o igual a 1000 m» #46243)
| Índice | psi0 | psi1 | psi2 |
|---|---|---|---|
| 0 (> 1000 m) | 0.7 | 0.5 | 0.2 |
| 1 (<= 1000 m) | 0.5 | 0.2 | 0.0 |

### 3.4 Categorías de uso CTE (`ctsetipo_texto_categoria_uso` formatea «A. %s» ... «G2. %s», «G1. %s. %s»)
| Idx | Rótulo | psi0 | psi1 | psi2 | Coef. «principal» sobrecarga | Coef. principal «G1» | psi0 «G1» |
|---|---|---|---|---|---|---|---|
| 0 | A | 0.7 | 0.5 | 0.3 | 1 | 0 | 0 |
| 1 | B | 0.7 | 0.5 | 0.3 | 1 | 0 | 0 |
| 2 | C | 0.7 | 0.7 | 0.6 | 1 | 0 | 0 |
| 3 | D | 0.7 | 0.7 | 0.6 | 1 | 0 | 0 |
| 4 | E | 0.7 | 0.7 | 0.6 | 1 | 0 | 0 |
| 5 | G2 | 0 | 0 | 0 | 1 | 0 | 0 |
| 6 | G1 | 0 | 0 | 0 | **0** | **1** | 0 |

(El texto de cada categoría sale de recursos `FUN_180006030..FUN_18000ad30` de ctesebas; los ids concretos no están en la tabla de cadenas ya extraída: #52949 «Zonas de tráfico y aparcamiento para vehículos ligeros» y #52950 «Cubiertas accesibles únicamente para mantenimiento» son dos de ellos; #52959 «No concomitante con el resto de acciones variables».)

### 3.5 Situaciones que genera `shcctese_elu` [V]
1. **«persistentes o transitorias»** (nombre desde `FUN_180091f50`): gamma_G = 0.8 / 1.35; sobrecarga: principal=1 (G1: 0), acompañamiento=psi0 sobrecarga por categoría, gamma var 0 / 1.5; viento: principal 1.0, acompañamiento 0.6, gamma 0 / 1.5; nieve: principal 1.0, acompañamiento psi0 nieve; temperatura: 1.0 / 0.6; empuje terreno 0.7 / 1.35; pretensado 1/1/1/1 (ver `ehecomb_coefs_pretensado_EL_ultimos`, todos 1.0).
2. Si existe categoría G1 en la obra: situación adicional **«%s (G1)»** con los coeficientes psi0 de variables a 0 y principal = 1 para G1.
3. **sísmica** (`FUN_180091f70`): gamma permanente 1/1 (las funciones `FUN_18007dfe0/FUN_18007dfc0` reciben `param_4` = factor sísmico y lo aplican al fav/desf), variables: acompañamiento = psi2, sobrecarga psi2 por categoría; sismo +1 / -1.
4. **accidental** (`FUN_180091f10`): gamma 1.0; principal = psi1 (sobrecarga, viento, nieve, temperatura), acompañamiento = psi2; acción accidental 1.0.
5. `shcctese_elu_accidental` (incendio, `FUN_180091f30`): permanentes sísmicas 1.0, principal psi1 / acompañamiento psi2.

Resto de exports CTE (estructura análoga, **[N]** por nombre; los números de psi se obtienen de las mismas funciones): `shcctese_els_flecha_caracteristica/frecuente/cuasipermanente`, `shcctese_acciones_caracteristicas_con_sismo`, `shcctese_acciones_caract_con_sismo_y_accidental`, `shcctese_hormigon_fisuracion`, `shcctese_hormigon_cimentaciones_segun_ehe08/ehe98`, `shcctese_hormigon_segun_ehe98`; genéricos `shcnorma_acciones_caracteristicas_con_sismo/sin_sismo/resistencia_elemento`, `shcnorma_flecha_y_fisuracion_pp/sc`; generador de pórticos/muros `shcgener_combinaciones_marcos_con/sin_empuje_tierras`, `..._muros_con/sin_sismo`.

## 4. Coeficientes Eurocódigo EN 1990 (`eurocod.dll`, `eccomb_EN_1990_2002_*`) [V]

Cada función registra el artículo de EN 1990 (A1.2.2 Tabla A1.1; A1.3.1 Tabla A1.2(B)/(C); A1.3.2 Tabla A1.3; A1.4.1 Tabla A1.4).

### 4.1 Gamma
| Concepto | Valor | Referencia |
|---|---|---|
| Caso B persistente: permanente desfavorable / favorable | 1.35 / 1.00 | Tabla A1.2(B) |
| Caso B: variable desfavorable / favorable | 1.50 / 0.00 | idem |
| Caso C persistente: permanente desf. / fav. | 1.00 / 1.00 | Tabla A1.2(C) |
| Caso C: variable desf. / fav. | 1.30 / 0.00 | idem |
| Accidental y sísmica: permanente fav/desf, variable desf., acción accidental y sismo (+) | 1.00 | Tabla A1.3 |
| Accidental y sísmica: variable favorable | 0.00 | idem |
| Sismo (-) | -1.00 | idem |
| ELS (permanente fav/desf, variable desf.) | 1.00 | Tabla A1.4 |
| ELS variable favorable | 0.00 | idem |
| Pretensado ELS (pretesa/postesa fav/desf) | 1.0 ×4 | A6.4.3 |
| Pretensado ELU anejo 0 o 3..9 / anejo 1 | 1.0 ×4 | A6.4.3 - EN 1992-1-1 A2.4.2.2 |
| Pretensado ELU **anejo 2** (pretesa fav, desf, postesa fav, desf) | 0.9, 1.1, 0.9, 1.1 | idem |

(Casos de la norma: «B» = STR/GEO enfoque 1.0 [conjunto B]; «C» = conjunto C; no hay caso A (EQU) en `eurocod`; `sestlimge_coeficientes_terreno_ELU_ECH_conjunto_B` y `shcceurocod_*_elu_caso_B/C_acc_psi1/psi2` cubren cimentación, ver §6.)

### 4.2 Psi (anejo recomendado)
| Acción | psi0 | psi1 | psi2 |
|---|---|---|---|
| Temperatura | 0.6 | 0.5 | 0 |
| Viento (anejos 0..7) | 0.6 | 0.2 | 0 |
| Viento (anejos 8 y 9 = Malasia/Singapur) | 0.5 | 0.2 | 0 |

### 4.3 Psi sobrecarga por categoría (EN 1990 Tabla A1.1) [V]
Índices de categoría 0..7; cadenas de la GUI en `eurocod` #63885.. «B. Offices», «C. Meeting areas», «D. Shops», «E. Warehouses», «F. Vehicle weight <= 30 kN», «G. 30 kN < vehicle weight <= 160 kN», «H. Roofs» (A = domestic/residential, texto no presente en `eurocod`).
| Idx | Cat. | psi0 | psi1 | psi2 |
|---|---|---|---|---|
| 0 | A | 0.7 | 0.5 | 0.3 |
| 1 | B | 0.7 | 0.5 | 0.3 |
| 2 | C | 0.7 | 0.7 | 0.6 |
| 3 | D | 0.7 | 0.7 | 0.6 |
| 4 | E | 1.0 | 0.9 | 0.8 |
| 5 | F | 0.7 | 0.7 | 0.6 |
| 6 | G | 0.7 | 0.5 | 0.3 |
| 7 | H | 0 | 0 | 0 |
Variante Malasia/Singapur (`..._MALASIA_SINGAPUR_coef_simult_psi0_sobrecarga`, 0x1800197d0): psi0 = 0.7 para todas salvo E = 1.0 (incluida H = 0.7).

### 4.4 Psi nieve (índice 0 = «Finland, Iceland, Norway, Sweden» #63892; 1 = «Remaining CEN Member States, H > 1000 m» #63893; 2 = «Remaining CEN Member States, H <= 1000 m» #63894)
| Idx | psi0 | psi1 | psi2 |
|---|---|---|---|
| 0 | 0.7 | 0.5 | 0.2 |
| 1 | 0.7 | 0.5 | 0.2 |
| 2 | 0.5 | 0.2 | 0.0 |

### 4.5 Anejo nacional según norma de material [V]
`eccomb_anejo_segun_norma_<material>(idNorma)` devuelve el índice de anejo (0 = valores recomendados). Los ids de norma coinciden con el orden de los combos de la GUI (ver `dialogs_nueva_obra_wizard.md` §3.1; **la correspondencia id -> norma concreta no está verificada**, solo los ids).
- **Hormigón** (ids 1..54): 0 salvo 11 -> 1; 15 -> 4; 19 -> 5; 22 -> 6; 31 -> 3; 32 -> 7; 36 -> 2; 45 -> 8; 46 -> 9.
- **Acero conformado** (ids 1..34): 0 salvo 6 -> 1; 10 -> 6; 11 -> 4; 16 -> 2; 25 -> 8; 26 -> 9.
- Acero laminado / aluminio / madera: `eccomb_anejo_segun_norma_acero_laminado` (0x180019d80), `_aluminio` (0x180019e60), `_madera` (0x180019fb0): **no decompiladas** (ver §8).
- `eccomb_son_anejos_compatibles(a,b)`: anejos 0,1,3..9 compatibles entre sí; el anejo 2 solo es compatible consigo mismo.
- Anejos 8 y 9 usan psi0 viento 0.5 y psi0 sobrecarga de Malasia/Singapur (la correspondencia anejo -> país es [N]: «MALASIA_SINGAPUR» en el nombre de la función).

## 5. Hormigón EHE-08 / Código Estructural (`ehe.dll`, `ehe08cmb_*`, art. 12.1) [V]
| Concepto | Valor |
|---|---|
| Permanente desfavorable sit. persistente | 1.35 |
| Permanente favorable sit. persistente | 1.00 |
| Permanente «no constante» desfavorable sit. persistente | 1.50 |
| Permanente «no constante» favorable sit. persistente | 1.00 |
| Variable desfavorable persistente / favorable | 1.50 / 0.00 |
| Permanentes y variables, sit. accidental (fav/desf), acción accidental +/- | 1.00 / 1.00; +1 / -1 |
| Cimentación (CTE DB-SE-C 2.4.2) desfavorable persistente / extraordinaria | 1.60 / 1.00 |
| Pretensado ELU (EHE-08 y EHE-98) | 1.0 ×4 |
`shcctese_elu` usa para el hormigón los valores `*_no_cte_*` (1.00 / 1.50) en el parámetro «permanente no constante», y para permanente «CTE» 0.80 / 1.35.

## 6. Resto de normas: tablas de situaciones exportadas por `sithipcmb` [V nombres; valores no extraídos]
Cada export es una constructora de situaciones (ELU/ELS) para una norma; los coeficientes salen de las DLL de cada norma (`eccomb_*`, `ctsecomb_*`, `ehecomb_*`, `eh91comb_*`, `dincomb_*`, `ntcicomb_*`, `nb97comb_*`, `sanscomb_*`, ...). Pendiente extraer sus valores (§8):
ACI 318 (08, 11, 11+ASCE 7-22, 99, 99 Chile, APNB 1225001 Bolivia, NSE-18, NCh430, NSR-10 y terreno, NTE E.060 y terreno), AISC LRFD y 360-05/-10, ASCE 7-05/-10/-22 (fisuración, flecha caract./cuasi/frec., sismo con sobrerresistencia, ASD y ASD incendio), AS/NZS (`shcasnzs_ELU`), BAEL-91 (RNV-65, RPA-RNV, RPS-CPC, ELU, ELS frecuentes/cuasipermanentes/raras), BS 5950 y british (pers./trans. y sísmicas, servicio), CIRSOC 201 (82, 02, fisuración), CSA S16-01, CTE (§3), DIN 1055-100 (accidental, caso B), EA-95, EH-91 (ELU, fisuración), EHE-08/EHE-98 (+fisuración), Eurocódigo (accidental psi1/psi2, caso B y C acc. psi1/psi2, fisuración, flecha caract./cuasi/frec.), IS 456 / IS 800 (`shcis456_*`, `shcis800_*`), NB1-78, NBR 6118 (03, 25), NBR 7190-97 (+incendio), NBR 8681 (84, 03, 25), NBR 8800 (86, 06, 08, 25), NTC (ntc, ntc04, ntc17, ntc23 CFE/NTC), NTCI, NTE060-2009, RNC-07-13, RSA, SANS 10160, SP 20 (SP 20.13330), UBC 97.
Por material, el programa decide la tabla en `sestlimge_coeficientes_<material>` / `_situaciones_hormigon` y los textos en `sestlimge_texto_<material>_segun_norma` (p. ej. «E.L.U. de rotura. Acero laminado: Código Estructural» en la cabecera del diálogo Estados límite, `dialogs_estados_limite.md`).

## 7. Generador de combinaciones (`gestcomp.dll`, módulo `gccombs`) [V parcial]
Flujo: `cb3hipusr_combinaciones_{sin,con}_sismo_dinamico` (combs3) -> `FUN_180033280` -> `FUN_180032200` (construye la lista de hipótesis tipadas) -> `gccombs_coefsComb` / `gccombs_coefsComb_temporal` / `gccombs_coefsComb_largo_plazo` (3 variantes de coeficientes: instantáneo, temporal [a plazo infinito] y activa largo plazo; mismas que los textos #39232-#39235 «Instantánea, por tipo de hipótesis simple», «Instantánea, por tipo de combinación de acciones», «A plazo infinito...», «Activa a largo plazo...») -> `gccombs_destruye_combinaciones`.
- Tipos de hipótesis que se añaden (`FUN_180008cd0(..., tipo, ...)`): **1 = variable** (`gccombs_append_hipotesis_variable`; el parámetro de simultaneidad se recibe en % y se divide por 100 si está activo), **3 = terreno** (`..._terreno`; coeficiente fijo 1.0), **4 = accidental** (`..._accidental`), permanente (`FUN_180008460`), sísmica X/Y (`FUN_180008710`), sismo incompatible (`FUN_180008a30`), viento por direcciones.
- **Viento por direcciones** (`gccombs_append_hipotesis_viento_direcciones`): 4 hipótesis de dirección; si el porcentaje de excentricidad (`param_20`) es >= 1e-5 se generan **8 patrones** de excentricidades (cada dirección con excentricidad + y - sobre la hipótesis ortogonal; cadenas #158430/#158431 «ecc.+/ecc.-»), si no, 4 patrones (una dirección cada vez). Coherente con #39176 «No se permite seleccionar hipótesis con excentricidad positiva y negativa en la misma combinación.»
- Filtrado: `gccombs_filtra_combinaciones(n, combs)`: reordena/deduplica; si `n < 2` no filtra; si hay >= 500 combinaciones **solo filtra si existe la variable de entorno `GCCOMBS_FILTRAR`** (en caso contrario usa 0 = sin filtrar). Se marca `estan_filtradas`. Un `error_interno` impide filtrar dos veces.
- La expansión núcleo por situación es `FUN_1800040f0` (gestcomp, llamada desde `FUN_180005d60` una vez por tipo de situación, con filtrado `FUN_1800082c0` antes y después) y la lista de patrones por naturaleza `FUN_180007320`. **Sin decompilar** (§8).
- Aviso de repetición: #39174 «La combinación "%s", definida en "%s", está repetida.»; #377002 «Hay hipótesis repetidas en la misma combinación.»; #39177 «Cada hipótesis del conjunto '%s' debe estar incluida en alguna de las combinaciones definidas.»
- Tipos de disposición (ver `hipotesis_carga.md`): Simultáneas / Incompatibles / Compatibles / Separada (#39142-#39148, #39189-#39193); sismo estático no se combina entre sí salvo tipo XY (#39198); sismo dinámico/por coeficientes no se combina con sismo estático adicional (#39200).
- **Fórmulas** (norma, **[N]** no verificadas en binario, solo coherentes con las constantes anteriores): ELU persistente = sum(gamma_G,j·G_j) + gamma_P·P + gamma_Q,1·Q_1 + sum(gamma_Q,i·psi0,i·Q_i); accidental = sum(G) + P + A_d + psi1,1·Q_1 + sum(psi2,i·Q_i); sísmica = sum(G) + P + A_E + sum(psi2,i·Q_i); ELS característica = sum(G) + P + Q_1 + sum(psi0,i·Q_i); frecuente = sum(G)+P+psi1,1·Q_1+sum(psi2,i·Q_i); cuasipermanente = sum(G)+P+sum(psi2,i·Q_i). Variación fav/desf: cada permanente se toma con gamma_fav o gamma_desf (se generan todas las combinaciones fav/desf por hipótesis) [N].
- Combinaciones no lineales (`combsanl`): «Generar combinaciones para las distintas situaciones de proyecto» #378336, «Generar combinaciones a partir de los resultados del cálculo lineal» #378337, «Sólo combinaciones lineales» #378166, tipos de combinación #377825-#377838 (resistencia en situación de incendio, fisuración bajo cargas de larga duración, equilibrio de cimentación, tensiones en el terreno E.L.U./E.L.S.), contadores «Se han generado '%lu' combinaciones.» #378563, «Se han actualizado '%lu' combinaciones.» #378564.

### 7.1 Segundo paso REA sobre `gestcomp` (2026-11-10) [V]
- **Orquestación** (`FUN_180005d60`): por cada «tipo de hipótesis» (lista `FUN_180007320` -> `FUN_180004980`) llama a `FUN_1800040f0`, que decide el modo: si el tipo es no variable o la situación solo tiene coeficiente «largo» (`FUN_180009c20`/`FUN_180009730`), solo permanentes (`FUN_180004770` -> `FUN_180003200` con `0xffffffff` y coeficientes 0); si no, llama a `FUN_180002d80` una vez por cada uno de **tres juegos de coeficientes** (índice 0, 1, 2 = tres «duraciones»/roles; se omite el juego cuyo coeficiente de actuación es nulo, `FUN_180002850/28b0/2910`; `error_interno` si ambos coeficientes de seguridad l y c son nulos). Tras cada tipo: ordenar + filtrar (`FUN_1800082c0`), y al final `FUN_1800066d0`.
- **Coeficiente efectivo de una variable en una combinación** (`FUN_180003200`): `coef = g_l·p_l·f + g_c·p_c·(1 - f)` con `f` = fracción de larga duración (`fraccion_largo_plazo < 1`, comprobado) y (g_l,p_l) / (g_c,p_c) = gamma·psi del juego «largo» y «corto». Es lo que justifica las variantes instantáneo / temporal / largo plazo (`gccombs_coefsComb*`).
- **Tres formas de expansión** según `param_1` de `FUN_180003200`: **0** hipótesis única (`FUN_180003da0` = lista de 1 id -> forma 1); **1** conjunto de hipótesis tratadas como un grupo con un solo coeficiente (`FUN_180003ec0`: copia de la lista, `FUN_1800098e0`, `FUN_180006710`, concatena); **2** grupos de hipótesis alternativas (`FUN_1800035e0`): para cada grupo se crea la entrada con coeficiente 0 (sin actuar) y, si `param_4 == 1` (variable), con **`+coef`** y, si `param_15 == 1` (alternancia de signo, p. ej. sismo X/Y o viento), también con **`-coef`**, donde el coeficiente se multiplica por el factor de disposición `param_14` (fracción ortogonal XY); después se combinan con los grupos simultáneos (`FUN_180006250`).
- Tipo de situación -> «dominio de signo»: accidental/terreno (tipos 4/3) usan `FUN_180009f80` (modo 0 si tipo == 1, 2 en los demás: `FUN_180008cd0`).
- **Filtrado** (`FUN_1800082c0`): construye el vector de coeficientes de cada combinación (`FUN_180007250`), ordena (`FUN_180006950`: orden lexicográfico por coeficientes enteros con `FUN_180006770`; si empatan, por `param_1[4]`, luego por `indice_combinacion`, nunca iguales), elimina duplicados por búsqueda dicotómica de bloques iguales (`FUN_180002af0`, modo 1 en la primera pasada y 0 en las siguientes, conservando `bloque - (modo != 1)` entradas, `FUN_180009630`) y repite una vez por situación (`FUN_180004070`). Es decir: **dedup por vector de coeficientes**, no solo por nombre. Se elimina además la comprobación de unicidad con `GCCOMBS_FILTRAR` (§7).
- **Nombre de combinación**: `cmbtipos_get_nombre_combinacion_opc` devuelve el nombre opcional guardado en la estructura si existe, si no el campo de respaldo (`+0x40` / `+0x48`): el nombre es un **texto almacenado por combinación** (no calculado en lectura). La cadena concreta («Comb. n», prefijos G/CM/Q/W...) se compone en la generación y **no** se ha localizado; ver §10.
- **Orden/numeración**: la lista final se numera por posición tras el orden lexicográfico anterior (clave `indice_combinacion` como desempate), separada por situación (persistente/transitoria, sísmica, accidental) y, dentro de cada una, por los tipos en el orden del array de tipos de hipótesis [N en el orden exacto entre tipos].

## 8. Países y normas (`cynormpais`, `cypenorma`) [V]
- Países #39318.. (España #39318, UE #53891, USA #53890, Reino Unido #53892, Venezuela #53895, Sudáfrica #53896, Rusia #53900, «Método general» #53887, y 80+ ids #129769-#129970 y #199314, #390661-#390663: Argelia, Argentina, Australia, Austria, Bélgica, Bolivia, Brasil, Brunei, Bulgaria, Camboya, Camerún, Canadá, Cabo Verde, China, Colombia, Rep. del Congo, Croacia, Chipre, Rep. Checa, Dinamarca, Rep. Dominicana, Ecuador, Estonia, Islas Feroe, Finlandia, Francia, Gabón, Georgia, Alemania, Grecia, Groenlandia, Hungría, Islandia, India, Indonesia, Irlanda, Israel, Italia, Jordania, Kazajistán, Letonia, Líbano, Lituania, Luxemburgo, Malasia, México, Mongolia, Marruecos, Mozambique, Nueva Zelanda, Noruega, Omán, Pakistán, Panamá, Perú, Filipinas, Polonia, Rumanía, Arabia Saudita, Serbia, Eslovaquia, Eslovenia, Suecia, Suiza, Siria, Tayikistán, Tailandia, Túnez, Turquía, Turkmenistán, Ucrania, Emiratos Árabes Unidos, Uzbekistán, Países Bajos, Madagascar, Mauricio, Benín).
- Nombres de norma por material ya listados en `dialogs_nueva_obra_wizard.md` §3.1 (verificado en la GUI). En `cypenorma`: «Criterio de la norma EHE-08» #54035, «Criterio del CTE DB-SE-C» #54036, «Criterio del Código Estructural» #401227, «Selección de norma» (`stbcfgnormsw` #317715); «La naturaleza '%s' no es admisible para las normas seleccionadas. Para poder dimensionar y comprobar correctamente los elementos de la obra debe eliminar las hipótesis de esta naturaleza.» #39185.

## 9. Valores por defecto en la obra nueva (de `dialogs_estados_limite.md`, GUI verificada) y relación con el código
Normas: Hormigón EC2; Acero conformado CTE DB SE-A; Acero laminado Código Estructural (radio **CTE** por defecto o **EC**); Madera CTE DB SE-M; Aluminio EC9. Combinaciones por defecto: Hormigón/Cimentación/Aluminio/Terreno = «EC - Altitud inferior o igual a 1000 m»; Acero conformado/laminado/Madera = «CTE - Altitud inferior o igual a 1000 m»; Desplazamientos = «Acciones características». Concuerda con: índice de nieve 1 (= <= 1000 m) en §3.3 y §4.4; el lápiz abre `shcsitusr_panel` / `sestlimge_panel_estados_limite_*` [N].

## 10. A medir en el original (medido el 2026-11-11, ver §12; lo no medido queda al final de §12)
0. (Actualizado: el algoritmo núcleo ya está en §7.1. Lo que sigue es lo que REA no ha cerrado: texto de nombres, orden entre tipos, psi de normas no tratadas, textos de categorías.)
1. **Regla de nombres de combinaciones** en listados («Comb. 1», «PP+...», etc.) y en el combo «Combinación seleccionada» (#64401, #65498). Capturar el listado de combinaciones de una obra con PP + CM + SCU + V + N + S en CTE y en EC.
2. **Contenido de los diálogos** de edición de combinaciones (lápiz y cada botón de «Estados límite a verificar»): tablas de gamma/psi mostradas, columnas y rótulos exactos; diálogo «Situaciones de proyecto definidas por el usuario» (`catuso` #46232, `shcsitusr_panel`), altas/ediciones, orden de tabulación.
3. **Lista completa del combo «Cota de nieve»/«Nieve»** de cada estado límite (aparte de «Altitud inferior o igual a 1000 m» / «superior a 1000 m») y su variante EC (3 opciones de §4.4).
4. **Número y orden de combinaciones generadas** para una obra mínima (p. ej. pórtico con PP, SCU, viento X+/-, nieve): contrastar con la expansión fav/desf y con la reducción de `gccombs_filtra_combinaciones` (§7).
5. **Radio «CTE / EC»** del acero laminado con Código Estructural: confirmar que cambia el conjunto de situaciones a las de §3 o §4.
6. **Efecto del anejo nacional** (anejos 1..9 de §4.5): comprobar en la GUI qué norma de hormigón corresponde a cada id (11, 15, 19, 22, 31, 32, 36, 45, 46) y los psi resultantes.
7. **Situación sísmica**: valor del factor `param_4` (`FUN_18007dfe0/dfc0`) y las combinaciones de sismo XY con fracción ortogonal (#39214); acciones accidentales de incendio (Código Estructural).
8. **ELS por norma**: valores de `shcctese_els_flecha_*`, `shcnorma_flecha_y_fisuracion_pp/sc` y del listado de «Desplazamientos / Acciones características» (psi aplicados a sobrecarga y viento).
9. Pendientes de más REA (no requieren GUI): decompilar `FUN_1800040f0`, `FUN_180008cd0`, `FUN_180007320`, `FUN_1800082c0` (gestcomp) para la expansión exacta fav/desf, orden y filtrado; `eccomb_anejo_segun_norma_acero_laminado/aluminio/madera`; la construcción del ELU persistente de EC en `sestlim.dll` (`sestlimge_situaciones_hormigon`); tablas de psi de las demás normas de §6.

## 11. Vía de investigación (para `docs/research`)
- Cadenas: `docs/research/ui/data/strings_merged_latin.tsv` (ids de `sithipcmb`, `combs3`, `combs`, `combsanl`, `docucmb`, `sestlim`, `catuso`, `ctesebas`, `eurocod`, `ehe`, `cynormpais`, `cypenorma`).
- Código: exports PE parseados con un script de lectura de la tabla de exportación (solo lectura) y pseudocódigo REA (`batch_decompile`) de `sithipcmb`, `ctesebas`, `eurocod`, `ehe`, `combs3`, `gestcomp`.
- Sin lanzar el original ni modificar ficheros fuera del proyecto.


## 12. Medición en el original (2026-11-11, T-P7-06) [V = medido en pantalla o en exportación del original]
Obra de trabajo: copia en memoria de `docs/sample_project/flecha_work.ed3` (no se grabó; Alt+F4 > No; el fichero y su `.ed$` se restauraron con git). Hipótesis: PP (auto), Q 1 (SCU), viento V(0°) H1, V(0°) H2, V(90°) H1, V(180°) H1, V(270°) H1 y nieve N(EI) (ya existentes), S 1, S 2 (sismo simple) y A 1 (accidental) añadidas. Capturas `docs/screenshots/cmb_*.png`; exportación del listado «Situaciones» en `docs/research/original_export/cmb_sit_ec_h_gt1000.txt` (UTF-16, ancho 200) y las anteriores `s1hi.txt`, `s2hi.txt` (CTE) y `s4_desplazamientos.txt`.

### 12.1 Dónde se ven las combinaciones y cómo se llaman [V]
- Cálculo > Esfuerzos (también Desplazamientos, Reacciones) abre la ventana flotante «Propiedades - Esfuerzos» (maximizable) con la sección **«Comb. seleccionada»** = dos combos: (1) tipo: `Hipótesis simple | Desplazamientos | Acero laminado y armado | Acero conformado | Madera | Aluminio | Hormigón`; (2) lista de combinaciones del tipo. En «Hipótesis simple» la lista es `Peso propio, Q 1, V(0°) H1, V(0°) H2, V(90°) H1, V(180°) H1, V(270°) H1, N(EI), S 1, A 1` (referencia tal como se escribió, con espacio). Los combos se manejan con aclick y las flechas ARRIBA/ABAJO/INICIO/FIN (el popup no admite rueda).
- **No existe «Comb. n».** El nombre de una combinación es la **fórmula**: términos unidos con «+», cada término `coef·REF` con punto medio U+00B7, coeficiente omitido si vale 1, «PP» para el peso propio y la referencia **sin espacios** (`Q 1` -> `Q1`, `V(0°) H1` -> `V(0°)H1`, `S 1` -> `S1`); coeficientes con los decimales mínimos (`0.8`, `1.35`, `1.5`, `0.9`, `1.05`, `0.75`, `0.2`, `0.5`); términos en el orden PP, Q, V, N, S, A; sismo como `-S1` / `+S1`. Los términos con coeficiente 0 no aparecen. Ejemplos: `1.35·PP+1.5·Q1+0.9·V(270°)H1+1.05·N(EI)`, `0.8·PP+1.5·N(EI)`, `PP-S1`, `PP+0.5·V(0°)H1+A1`, `PP+0.2·N(EI)+A1`.
- No hay numeración ni separación visible por situación: la lista del tipo contiene en orden las persistentes, luego las sísmicas y luego las accidentales.

### 12.2 Número y orden de combinaciones (obra mínima) [V]
- ELU persistente con PP + Q1 (categoría con psi0 = 0) + 5 hipótesis de viento alternativas + N: **58** combinaciones tanto con EC (nieve > 1000 m: 1.05 = 1.5·0.7) como con CTE (<= 1000 m: 0.75 = 1.5·0.5). Hormigón, Aluminio y Acero laminado dan la misma lista con la misma norma. Con S1 (+2) y A1 (+7 con CTE) la lista del acero laminado CTE tiene **67**; con S2 y EC termina en `PP-S1, PP+S1, PP-S2, PP+S2, PP+A1`.
- Conteo 58 = 24 (sin nieve) + 12 (nieve principal) + 22 (nieve acompañante): permanentes con fav/desf (**fav primero**: `PP` antes que `1.35·PP`); cada variable ausente / principal / acompañante; el viento es **un único grupo de alternativas** (una dirección a la vez, solo signo +, **sin combinaciones con signo -**); no existe combinación sin variable principal (la nieve sola entra como principal); Q1 con psi0 = 0 como acompañante no genera combinaciones nuevas (se deduplica).
- **Orden**: no es lexicográfico por el valor del coeficiente. Es por índice de juego de coeficientes de cada hipótesis, con significación decreciente **N > V > Q > PP**: primero las combinaciones sin nieve (V: ninguno, V(0°)H1, V(0°)H2, V(90°)H1, V(180°)H1, V(270°)H1; dentro de cada V: sin Q, con Q; dentro, PP fav y PP desf), luego nieve principal (`1.5·N`, con V ausente o acompañante 0.9), luego nieve acompañante (`0.75·N` / `1.05·N` con Q1 principal, V principal, o Q1 principal + V acompañante). Es decir ausente < principal < acompañante y fav < desf. Coherente con gestcomp (§7.1 y §12.6: vectores de índices enteros, ordenación por índices, dedup).
- Sismo: `PP-S1, PP+S1, PP-S2, PP+S2` (cada sismo por separado, índice 0 = -1 y 1 = +1; sin S1+S2); gamma_E = 1; los psi2 nulos no aparecen. Accidental CTE (psi1 principal): `PP+A1, PP+0.5·V(0°)H1+A1, ... V(270°)H1, PP+0.2·N(EI)+A1` (7). Accidental EC (psi2 principal): solo `PP+A1`.
- «Desplazamientos» (Acciones características): **todas las variables con coeficiente 1, sin principal ni psi, todos los subconjuntos** (viento = una alternativa): 24 combinaciones `PP, PP+Q1, PP+V(0°)H1, PP+Q1+V(0°)H1, ... PP+N(EI), PP+Q1+N(EI), PP+V(0°)H1+N(EI), PP+Q1+V(0°)H1+N(EI), ...` (orden N > V > Q). Con sismo: `PP-S1, PP+Q1-S1, PP+S1, PP+Q1+S1, PP-S2, PP+Q1-S2, PP+S2, PP+Q1+S2` (viento y nieve no aparecen con S en esta obra).
- Los tipos «Hormigón», «Aluminio», etc. contienen solo ELU en estas obras (no apareció lista de fisuración).

### 12.3 Visor «Situaciones» (lápiz de Estados límite) [V]
- El lápiz abre el visor **«Situaciones»** (ventana ~775x270 redimensionable y maximizable; barra: ajuste/zoom, navegación `|< < [n] > >|` con «1 de 15», **Compartir, Exportar ▾, Configuración (rueda), Imprimir..., Cerrar**). Exportar ▾: «Exportar listado», DOCX (Word2007 - OfficeOpenXML), texto, HTML, RTF. «Exportar en formato texto»: ruta (defecto `D:\Coding\CypeClwon\docs\research\original_export\Situaciones.txt`, botón carpeta) y «Ancho de la página (en caracteres)» **120**; al terminar, Información «Exportación finalizada.». Marca de agua «Producido por una versión educativa de CYPE».
- Contenido por estado límite: título (p. ej. «E.L.U. de rotura. Hormigón: Eurocódigo 2»), «Para las distintas situaciones de proyecto, las combinaciones de acciones se definirán de acuerdo con los siguientes criterios:», fórmulas de las 3 situaciones, «Donde:» con la leyenda (Gk, Pk, Qk, AE, Ad, gamma, psi p,1 / psi a,i) y, por situación, tabla con cabeceras **«Coeficientes parciales de seguridad (g)»: Favorable | Desfavorable** y **«Coeficientes de combinación (y)»: Principal (yp) | Acompañamiento (ya)**. Filas: Carga permanente (G), Pretensado (PST), Retracción (R) [solo CTE / Código Estructural], Sobrecarga (Q - Uso A..H en EC; A..E, G1, G2 en CTE), Viento (Q), Nieve (Q), Temperatura (T), Empujes del terreno (H), Sismo (E) [solo sísmica], Accidental (A) [solo accidental]. Situaciones: «Persistente o transitoria», «Persistente o transitoria (G1)» (CTE con G1), «Sísmica», «Accidental», «Accidental de incendio» (CTE y EC acero conformado/laminado y madera).
- **Valores EC** (nieve > 1000 m). Persistente: G 1.000/1.350; PST 1/1; SCU gamma 0/1.5, principal 1.0, acompañamiento A,B,C,D,F,G 0.7, E 1.0, H 0.0; viento 0/1.5, 1.0, 0.6; nieve 0/1.5, 1.0, 0.7; temperatura 0/1.5, 1.0, 0.6; empujes 1.0/1.35. Sísmica: G 1/1, variables 0/1.0, principal = acompañamiento = psi2 (SCU A 0.3, B 0.3, C 0.6, D 0.6, E 0.8, F 0.6, G 0.3, H 0; viento 0; nieve 0.2; temperatura 0), Sismo (E) -1.000/1.000, principal 1.000, acompañamiento 0.000. **Accidental EC: principal = acompañamiento = psi2** (mismos valores que la sísmica), Accidental (A) 1/1. Tensiones del terreno (A1 - Conjunto B): igual que Hormigón; **(A2 - Conjunto C)**: G 1/1, variables 0/1.3 (psi como EC), empujes 1/1. «E.L.S. Fisuración. Hormigón: Eurocódigo 2» = «Cuasipermanente»: G 1/1, variables 0/1.0, principal = acompañamiento = psi2.
- **Valores CTE** (`s2hi.txt`; nieve con psi0 0.7 y psi2 0.2 en ese ejemplo). Persistente: G 0.800/1.350, PST 1/1, SCU 0/1.5, principal 1.0, acompañamiento A-E 0.7, G1 0.0, G2 0.0 (principal 1.0), **Retracción 1.000/1.500** (1/1 en sísmica y accidental), viento/nieve/temperatura 0/1.5 (1.0; 0.6/0.7/0.6), empujes 0.7/1.35. «Persistente o transitoria (G1)»: G1 principal 1.0, resto 0. Sísmica: G 1/1, psi2 SCU A,B 0.3, C,D,E 0.6, G1/G2 0, nieve 0.2, Sismo -1/1. **Accidental y accidental de incendio CTE: principal = psi1** (A,B 0.5; C,D,E 0.7; viento 0.5; nieve 0.5; temperatura 0.5), **acompañamiento = psi2** (0.3; 0.6; viento 0; nieve 0.2; temperatura 0); el incendio no tiene fila de acción accidental.
- **Desplazamientos** («Acciones características», `s4_desplazamientos.txt`): solo columnas Favorable | Desfavorable, sin psi: G 1/1, PST 1/1, Sobrecarga 0/1, Viento 0/1, Nieve 0/1, Empujes 1/1; Sísmica igual con la fila Viento vacía y Sismo (E) -1/1. **gamma de ELS para flecha en CTE = 1.0 sin psi** (misma tabla que en EC).

### 12.4 Diálogos de Estados límite y de situaciones [V]
- **Estados límite** (3 variantes vistas). (a) Obra EC: líneas «Hormigón: Eurocódigo 2», «Hormigón en cimentaciones: Eurocódigo 2», «E.L.U. de rotura. Acero conformado / Acero laminado: Eurocódigos 3 y 4», «... Madera: Eurocódigo 5», «... Aluminio: Eurocódigo 9», **Tensiones sobre el terreno** con combo «Nieve» y lápiz, **Desplazamientos / Acciones características** con lápiz. (b) Obra CTE (botón «CTE» de Selección de normas): «Hormigón: Código Estructural», «Hormigón en cimentaciones: Código Estructural / CTE DB-SE C» con radios **CTE | EC | «Hormigón: EC, Acero: CTE»** (el tercero marcado) y «Cota de nieve»; Acero conformado/laminado y Madera CTE con «Cota de nieve»; «Aluminio: Eurocódigo 9» con «Nieve»; Tensiones sobre el terreno y Desplazamientos «Acciones características». (c) Obra Código Estructural (botón «C.E.»): los mismos radios aparecen bajo **Acero laminado: Código Estructural**; al marcar EC el rótulo cambia de «Cota de nieve» a «Nieve» (misma lista de 2). Botón «Configurar combinaciones para cada estado límite», Aceptar/Cancelar.
- **Listas de nieve**: normas EC (EC2/EC3...): `Finlandia, Islandia, Noruega, Suecia` | `Resto de los Estados miembro del CEN, H > 1000 m` | `Resto de los Estados miembro del CEN, H <= 1000 m` (3 opciones). CTE, Código Estructural y Aluminio en obra CTE: `Altitud superior a 1000 m` | `Altitud inferior o igual a 1000 m` (2 opciones). **El radio CTE / EC del acero laminado cambia el conjunto de situaciones**: con EC la lista de combinaciones empieza `PP, 1.35·PP` (PP fav 1.0) y el accidental usa psi2; con CTE `0.8·PP, 1.35·PP` y accidental psi1.
- **Estados límite a verificar** (icono de ayuda en el título): 8 botones (Hormigón, Cimentación, Acero conformado, Acero laminado, Madera, Aluminio, Tensiones sobre el terreno, Desplazamientos) con el valor vigente a su derecha («CTE - Altitud inferior o igual a 1000 m», «EC - Altitud superior a 1000 m», «Acciones características»). Cada botón abre un diálogo con el estado límite como título: radio **«Datos generales»** (texto «Nieve: ...» o «Acciones características» + lápiz = visor de 12.3) y radio **«Situaciones de proyecto introducidas por el usuario»** con combo (vacío) y barra de iconos (nuevo +, editar, copiar, otros dos y flecha izquierda «Crear a partir de las situaciones de la norma» #156485) y lápiz; Aceptar/Cancelar. No se identificaron por tooltip los iconos centrales.
- **Crear - [Situaciones]**: «Referencia» (texto), barra (+, copiar, aspa), lista «Situaciones», Aceptar/Cancelar. «+» abre **Coeficientes de combinación**: radios «Introducir coeficientes de mayoración» (fórmula con gamma; defecto) / «Introducir coeficientes de mayoración y combinación» (fórmula con psi); en Desplazamientos aparece igual. Después **Categorías de uso** (barra +, copiar, aspa; tabla «Categoría de uso» con celda editable, Intro confirma). Después **«Situación de proyecto definida por el usuario»**.
- **Situación de proyecto definida por el usuario** (~490x450): «Nombre de la situación»; combo de tipo `Situación persistente o transitoria | Situación accidental | Situación sísmica | Pretensado en vacío` (igual en Desplazamientos); cabeceras «Coeficientes parciales de seguridad» (Favorable | Desfavorable) y «Coeficientes de combinación» (Principal | Acompañamiento; solo con mayoración y combinación). Filas: Carga permanente, Pretensado, Retracción, **Sobrecarga (Uso n)** (una por categoría), Viento, Nieve, Temperatura, Empujes del terreno. Defecto: todo 1.000 salvo favorable de las variables 0.000; celdas «-» inhabilitadas en principal/acompañamiento de permanentes. **Orden de tabulación**: Nombre -> combo de tipo -> por filas de arriba abajo y de izquierda a derecha (Favorable, Desfavorable, Principal, Acompañamiento) saltando las celdas inhabilitadas; al entrar en una celda se selecciona todo (teclear sustituye). Rango de cada celda: tooltip «Debe estar entre 0.000 y 10.000 .». Aceptar valida en este orden: 1.º Aviso «No puede introducir un coeficiente favorable mayor que el desfavorable.», 2.º Aviso «No puede introducir un coeficiente principal menor que el de acompañamiento.» (confirman #153667/#153668). Nombre vacío o repetido: no probado (#153580/#153581).
- **Selección de normas** (clic en la barra «Normas:» de Datos generales): combos Hormigón, Acero conformado, Acero laminado, Madera, Aluminio, Cimentación (solo con CTE/CE) y botones **«← C.E.»** y **«← CTE»** (tooltip «Asignar normas correspondientes al Código Técnico de la Edificación»). CTE asigna: Hormigón Código Estructural (España), Acero conformado y laminado CTE DB SE-A (España), Madera CTE DB SE-M (España), Aluminio Eurocódigo 9, Cimentación «Criterio del CTE DB-SE-C». C.E. asigna: Hormigón Código Estructural, Acero conformado Eurocódigos 3 y 4 (España), Acero laminado Código Estructural (España), Madera CTE DB SE-M, Aluminio EC9, Cimentación «Criterio del Código Estructural». Al aceptar: Aviso «Al cambiar la norma correspondiente a un tipo de material (acero laminado, madera, etc.), la selección de los materiales, para las barras de dicho tipo de material, se inicializa a su valor por defecto.» y se abre **Categorías de uso**. Con CTE: casillas «A. Zonas residenciales», «B. Zonas administrativas», «C. Zonas de acceso al público», «D. Zonas comerciales», «E. Zonas de tráfico y aparcamiento para vehículos ligeros», «G1. Cubiertas accesibles únicamente para mantenimiento. No concomitante con el resto de acciones variables», «G2. Cubiertas accesibles únicamente para mantenimiento»; la categoría EC «H. Cubiertas» pasa a G2. Con normas mixtas: lista Ref./Descripción + un combo de categoría por grupo de normas.
- **Calcular** (Cálculo > Calcular): diálogo «Cálculo» con radios «No dimensionar perfiles» (defecto) / «Dimensionamiento rápido de perfiles» / «Dimensionamiento óptimo de perfiles» y casillas «Comprobar las barras» (marcada) y «Considerar la dimensión finita de los nudos».
- Factor sísmico (punto 7): con sismo simple el coeficiente del nombre es 1 (`PP-S1`, `PP+S1`); la fracción ortogonal del tipo XY no se midió (en la segunda hipótesis de sismo los radios «Tipo de sismo estático» Simple/XY aparecen inhabilitados, con «Simple» marcado).

### 12.5 Discrepancias con `src/core/combinations.cpp`
1. **Nombres**: el generador pone «Comb. n»; el original usa la fórmula (12.1).
2. **Viento con signo**: el generador crea +1 y -1 por cada hipótesis de viento; el original solo +1 (las direcciones son hipótesis distintas). El ± solo está medido para sismo.
3. **Accidental EC**: original principal = acompañamiento = psi2; el generador usa psi1 (correcto solo para CTE).
4. **Retracción (CTE)**: original 1.0/1.5 persistente y 1/1 en sísmica/accidental; el generador usa 0.8/1.35. Empujes EC caso B: original 1.0/1.35.
5. **Orden**: el generador ordena por valor de los coeficientes; el original por índice de juego (N > V > Q > PP; ausente < principal < acompañante; fav < desf; sismo - antes que +), y la lista visible por tipo no separa situaciones.
6. **Desplazamientos**: el original usa todas las variables a 1.0 y todos los subconjuntos (más variantes con sismo ±, Q1 acompaña); el generador crea ELS característica con principal + psi0.
7. Con las correcciones 2 y 6 el conteo del generador (fav/desf x principal/acompañante) coincide con las 58 del original.

### 12.6 REA completado en gestcomp (2026-11-11) [V]
- `FUN_180002970` (`gccombs.c` línea 0x478): para cada hipótesis del grupo crea un nuevo **índice de juego de coeficientes** (`FUN_180006fd0(...)`) en `coefs_mayoracion_hipotesis[i]`, exige que todas queden con el mismo índice nuevo y lo devuelve. `FUN_1800098e0` = `FUN_180002970` + `FUN_180009aa0`, que escribe ese índice en la posición de cada hipótesis del grupo en **cada combinación** (`indices_coefs_combinacion`; `ULONG_MAX` = no actúa; error interno si ya tenía valor). Una combinación = vector de índices por hipótesis, -1 = ausente (explica el orden de 12.2).
- `FUN_180006710` = `FUN_180009000` (posiciones j de las combinaciones para las que `FUN_180007d50(...) == 0`) + `FUN_1800079b0` (ordena esa lista y **borra** las combinaciones de mayor a menor posición): filtro por predicado.
- `FUN_180006250`: pliega los **grupos** en orden; por grupo comprueba `tipo_actuacion_grupo` (`gccombs_reduccion_t`) y `actua_hipotesis_comb_grupo`; tipo 1 -> `FUN_180006140` (con el juego `param_6` o `param_5` según `FUN_180009880`), otro -> `FUN_180006670`; la lista resultante sustituye a la anterior (producto cartesiano acumulado). Aserciones `num_grupos == arr_NumElemsEnum(tipo_actuacion_grupo)` (líneas 0x732/0x733).
- Pendiente (REA): tablas psi de las demás normas de §6 (ACI, AISC, ASCE, BAEL, BS, NBR, NTC, DIN, SANS, IS...) y `FUN_180007d50`, `FUN_180006140`, `FUN_180006670`, `FUN_180006fd0`, `FUN_180009880`, `FUN_1800027f0`.

### 12.7 Implementado en `src/core/combinations.{h,cpp}` (T-P7-06, 2026-11-11) [V = fijado con tests]
- Corregidas las discrepancias 1-7 de 12.5. Tests `CombinationsOriginal.*` (`tests/combinations_tests.cpp`): 58 persistentes EC y CTE (24 + 12 + 22), orden y nombres, 67 con S1 y A1 (CTE), 24 desplazamientos (+8 con S1, S2), nombres `PP-S1`, radio CTE/EC.
- **Cambios de API para el frontend**: `Combination::name` es la fórmula (ya no «Comb. n», sin numeración); nuevo `Combination::indices` (índice de juego por hipótesis, -1 = no actúa); nueva `combinationFormulaName(set, coefs)`; `ServiceCharacteristic` = «Desplazamientos» (todas las variables a 1.0, sin principal ni psi; con sismo añade las variantes ±S sin viento ni nieve); viento = un grupo de alternativas solo con signo +; `snowZoneFromText` reconoce las listas EC («H > 1000 m», «Finlandia...» = Nordic).
- **[N] / sin medir**: orden de los términos del nombre para CM, T, R, H; nieve con psi2 no nulo (EC H > 1000 m, psi2 = 0.2) en sísmica/accidental: el generador añade `PP+0.2·N(EI)-S1` etc., pero la lista medida de EC termina en `PP-S1, PP+S1, PP-S2, PP+S2, PP+A1` sin ese término (¿medida con otra cota de nieve?); exclusión de viento y nieve en desplazamientos con sismo solo medida en la obra de referencia; sismo XY (fracción ortogonal) y disposiciones de carga no expandidos.
