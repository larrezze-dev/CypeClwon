# Medidas en el original: leyes, deformada, envolventes, apoyos (2026-11-12, RE-ANALYST, solo observacion)

Marcas: **[V]** medido en el original (captura en `docs/screenshots/rm_*.png`), **[N]** no logrado / no concluyente. Obra: copia de `docs/sample_project/rp_c1.ed3` (voladizo N1 (0,0,0) - N2 (4,0,0), L = 4 m, apoyo en N1 con muelles, cargas de nudo en N2; Describir genérico A = 100 cm2, Iyy = 1e4, Izz = 5e3, It = 8e3) abierta con `tools/vh_open.ps1`, calculada sin comprobar. Resultados de la barra: N = +10, Vy = +10, Vz = -10 kN (constantes), My = -40 en x = 0 (lineal a 0 en x = 4), Mz = +40 en x = 0 (lineal), Mt = 0. Conducción: `tools/da.sh` (attach), proyección alzado/planta con el botón (401,212) de la barra izquierda (1.a fila: alzado Z arriba/X derecha; 2.a icono: planta).

## 1. Escala de las leyes de esfuerzos: h[m] = escala x valor[tonelada-fuerza] [V]
Calibrado en alzado con N1 y N2 conocidos (4 m = 1065.5 px, 266.4 px/m; el anillo magenta junto al nudo NO está en el nudo, queda +19 px a la derecha). Altura de la ley en el empotramiento:

| Ley | Valor | Escala | Altura medida | m por unidad de la escala |
|---|---|---|---|---|
| My | 40 kN·m | 0.100 | 108.5 px = 0.407 m | 0.1019 |
| Vz | 10 kN | 1.000 | 272 px = 1.022 m | 0.1022 |
| Vz | 10 kN | 0.500 (a 321 px/m, otra sesión de vista) | proporcional exacto (x2.0 al duplicar) | idem |
| N | 10 kN | 1.000 | 271.5 px = 1.020 m | 0.1020 |
| Vy (planta) | 10 kN | 0.100 | 33 px de 325 px/m | 0.101 |
| Mz (planta) | 40 kN·m | 0.100 | 131 px | 0.102 |

La longitud dibujada es lineal en la escala (My 0.1 -> 130.5 px, 0.5 -> 655 px: x5.02; Vz 0.5 -> 167 px, 1.0 -> 334 px: x2.00) y el factor es **0.102 m = 1/9.81 m por unidad de kN y por unidad de escala**, es decir: **altura (m) = escala x valor en T (tonelada-fuerza) = escala x valor_kN / 9.81** (momentos: kN·m / 9.81). Coherente con el almacén interno en T/m (`resultados_rea_pasada2.md` §1). No es 0.1 m exacto: 2 % por encima.

## 2. Deformada [V]
Deformada con escala 0.100: N1 desplazado Dx = 6666.667 mm (tooltip) se dibuja a **177.5 px = 0.6663 m a 266.4 px/m** = 0.1 x 6.667 m: **desplazamiento dibujado = escala x desplazamiento (m)**, factor exacto 1.000 (error 0.1 %). La pendiente de la polilínea coincide con (N2' - N1'). Con el valor 100 (defecto) el extremo sale de la vista y el tramo exterior se dibuja discontinuo (puntos). No se midió la escala 1 ni 100 directamente (el desplazamiento de esta obra es de metros), pero la linealidad en m está confirmada. Rótulo `D mín.: 6666.667 mm` / `X: 0.000 m`.

## 3. Direccion y signo de las leyes (barra local x = +X global, z = +Z, y = +Y) [V]
| Ley | Valor en la obra | Se dibuja hacia | Regla |
|---|---|---|---|
| N | +10 | +z local | positivo hacia +z (plano x-z) |
| Vy | +10 | +y local (planta, hacia arriba) | positivo hacia +y (plano x-y) |
| Vz | -10 | -z local | positivo hacia +z (plano x-z) |
| My | -40 | +z local | **positivo hacia -z** (plano x-z) |
| Mz | +40 | -y local | **positivo hacia -y** (plano x-y) |
Es decir: axil y cortantes se dibujan hacia el semieje local positivo; los momentos hacia el negativo (lado traccionado convencional). Plano de dibujo: N, Vz, My en x-z; Vy, Mz en x-y. Mt: no medido (la obra no tiene torsor) [N].

## 4. Rotulos «Dibujar valores maximos y minimos» [V]
Texto en negrita del color de la ley, dos lineas `<Ley> máx.|mín.: <valor> <ud>` / `X: <abscisa> m`, con marcador de cota. Medidos: `My mín.: -40.00 kN·m / X: 0.000 m`, `Vz mín.: -10.000 kN / X: 0.000 m` y `Vz máx.: -10.000 kN / X: 4.000 m` (ley constante: mín en el extremo inicial y máx en el final), `Vy máx.: 10.000 kN / X: 2.000 m` y `Vy mín.: 10.000 kN / X: 4.000 m` (constante: la abscisa de cada rótulo es arbitraria), `N mín.: 10.000 kN / X: 0.000 m`, `Mz máx.: 40.00 kN·m / X: 0.000 m`, `D mín.: 6666.667 mm / X: 0.000 m`. Momentos 2 decimales, esfuerzos 3, flechas mm 3.
Flechas de rp_c1 (escala 100, grupo secante): **`Fxy mín.: -3.906 mm` (X: 1.750 m) [V]** (confirma la estimación de §4 de `resultados_rea_pasada2.md`), `Fxz máx.: 1.953 mm` y `F máx.: 4.367 mm` en el mismo X (rótulos solapados en la captura `rm_b_flecha.png`).

## 5. Casillas del panel Esfuerzos (vista isométrica, solo My) [V]
- **«Dibujar las leyes con relleno» desmarcada**: desaparece el relleno amarillo y queda solo el contorno (línea de la ley + las dos verticales de los extremos). `rm_c2.png`.
- **«Ver la estructura» desmarcada**: desaparece la línea negra de la barra; las leyes y los ejes locales siguen. `rm_c3.png`.
- **«Ver valores máximos y mínimos»**: marcada no cambia el dibujo (`rm_c1.png`); está habilitada solo con una ley seleccionada y «Dibujar valores máximos y mínimos» desmarcada (con varias leyes y con N/Vz aparece en gris); probablemente actúa sobre el globo de consulta [N].
- **«Ejes sobre el plano de la ventana»** marcada (tooltip: «Muestra las gráficas de esfuerzos sobre un sistema de coordenadas tal que su eje 'X' coincide con el eje local 'X' de la barra, el eje 'Z' está contenido en el plano de la ventana y el eje 'Y' es perpendicular a dicho plano. De esta manera, las gráficas de esfuerzos que se visualizan mantienen la continuidad en un plano determinado.»): **sin cambio visible** con una barra horizontal en iso, alzado y planta (My y Mz probadas; `rm_c4`, `rm_c6`, `rm_c8`, `rm_c10`). Solo tiene sentido con barras de orientación distinta cuyo eje z local no esté en el plano de la ventana: no medido [N].

## 6. Envolventes sin combinaciones [V]
- Esfuerzos > «Envolventes» con My marcada y la obra con solo la hipótesis «Peso propio»: **no dibuja nada** (`rm_d_env0.png`); desaparece el grupo «Comb. seleccionada»; la fila «Deformada (D)» no existe en este modo.
- Desplazamientos > «Envolventes»: los combos quedan deshabilitados (gris) y el globo de una barra muestra **pares (mín, máx)** por componente: `Dx: (6666.667, 6666.667) mm`, `Dy: (0.000, 0.000) mm`, `Gy: (1574.835, 1574.835) mRad`, N2 `Dz: (-6309.621, -6309.621) mm` (`rm_d_env1.png`). Con una sola hipótesis mín = máx. Es decir, hay un conjunto de combinaciones implícito (aquí la única «Peso propio»); el formato del globo en envolvente es «(mín, máx) unidad».

## 7. Símbolo de apoyo en Reacciones [V]
El hatch del empotramiento (alzado) es **azul turquesa sin globo** (núcleo `#009ADC`, antialias `#38BCE8`) y **marrón claro mientras el nudo tiene globo de reacciones** (núcleo `#B09A84`, antialias `#E8BC8F`, capturas `rm_e_react.png` / `rm_e_react2.png`). Condición: nudo apoyado con globo activo (primer clic en el nudo); el segundo clic oculta el globo y vuelve el azul. En Esfuerzos el símbolo es negro/gris fino.

## 8. TK-112: combinaciones no lineales «Resistencia» segun las normas [V]
Obra con apoyo elastico Dz (K = 1000, Sentido Positivo). Diálogo «Combinaciones no lineales» (maximizado con el boton del titulo, `rm_a_nl1.png`): columnas **Referencia | Actúa | Escalones de carga (10) | Tolerancia (combo, 1.e-8) | Iteraciones (25) | Reintentos (3)**; a la derecha «Tipos de combinación en los que se usa» (tabla Referencia | Usar: Resistencia, Resistencia en situación de incendio, Fisuración, Fisuración bajo cargas de larga duración, Desplazamientos, Equilibrio de los elementos de cimentación, Tensiones en el terreno (E.L.U.), Tensiones en el terreno (E.L.S.)) y «Factores de combinación» (Hipótesis | Factor).
Obra > Datos generales > «Normas» (botón con el texto de las normas) abre «Selección de normas» (combos Hormigón, Acero conformado, Acero laminado, Madera, Aluminio [+ Cimentación al pulsar CTE]; botones «C.E.» y «CTE»; aviso al aceptar «Al cambiar la norma correspondiente a un tipo de material ..., la selección de los materiales ... se inicializa a su valor por defecto.»; con CTE aparece ademas «Categorías de uso» A-E, G1, G2 sin marcar).
| Normas | Combinaciones no lineales generadas (tras Calcular, «Actualizar... tras el análisis lineal») | Usar |
|---|---|---|
| Eurocódigos 2, 3 y 4, 5, 9 (defecto) | `PP` (1.0) y `1.35·PP` | PP: Resistencia + Equilibrio + Tensiones E.L.U.; 1.35·PP: Resistencia + Equilibrio |
| Hormigón = Código Estructural, acero laminado/conformado = CTE DB SE-A, madera = CTE DB SE-M, aluminio = Eurocódigo 9, cimentación = CTE DB-SE-C | `PP`, `1.35·PP`, `0.8·PP`, `1.6·PP` | PP: Resistencia + Equilibrio + Tensiones E.L.U.; 1.35·PP: Resistencia + Equilibrio; **0.8·PP: solo Resistencia**; **1.6·PP: solo Equilibrio** |
Conclusión: «Resistencia» NO es una familia unica: es la **union (sin duplicados) de los casos de «Resistencia» de las normas de todos los materiales** (peso propio favorable 1.0 EC2/EC3/EC9/CE, desfavorable 1.35, favorable CTE acero/madera 0.8); el 1.6 solo entra en Equilibrio (cimentación). Con la unica hipotesis «Peso propio» el cambio de CTE respecto a EC añade 0.8·PP (Resistencia) y 1.6·PP (Equilibrio). No se probó el efecto por familia aislada (aluminio sin variante CTE) [N].

## 9. TK-111(c): dos muelles con Sentido interdependientes [V]
Voladizo N1-N2 (L = 4 m, genérico A = 100, Iyy = 1e4, γ = 0.01), carga -Z 10 kN en N2. N1: Dx, Dy Fijo, **Dz Elástico K = 1000 Positivo**, giros Fijo; N2: Dx, Dy Fijo, **Dz Elástico K = 1000 Negativo**, giros Libre. Calculo sin errores ni avisos (convergió: informe solo «La obra ha sido calculada sin realizar la comprobación de las barras.»). Lineal (envolvente/hipótesis simple, ambos bilaterales): Dz N1 = -3.302, N2 = -6.698 mm (suman 10 kN/K). **No lineal `[1] PP`: Dz N1 = -10.001 mm, Dz N2 = -10.000 mm, Gy = 0, Gx = Gz = 0**: el muelle de N1 (Positivo, solo actúa con Dz > 0) se retira porque N1 baja, el de N2 (Negativo) carga los 10 kN y la barra se traslada como sólido rígido (el 0.001 mm es el peso propio). Es el resultado del conjunto activo final {N2}: el original resuelve la interdependencia retirando el muelle que no recibe el signo de su Sentido. Capturas `rm_g_nl1.png`, `rm_g_rep.png`. Apoyo con Sentido en giros no medido [N].

## 10. Aluminio: peso en Describir [V parcial]
Aluminio extruido (icono «Perfil de aluminio extruido», serie «(EP) I», perfil I-50x80x6): el (i) da **Área 11.88 cm2 y Peso 3.21 kg/m** (`rm_h_info_c.png`) => 3.21 / 11.88e-4 = 2702 kg/m3, es decir **ρ = 2700 kg/m3** (11.88e-4·2700 = 3.2076 -> 3.21; 2710 daria 3.22). No hay campo editable «Peso específico» en los diálogos de aluminio (extruido: solo «Disposición: Perfil simple»; «Sección especial de aluminio»: editor gráfico sin campo de peso). Con el patrón del original (T/m3 x 9.81) el peso específico equivalente es **2.7 · 9.81 = 26.487 kN/m3**, no 27.0 (el clon usa 27.0): no se verificó el peso propio calculado de una barra de aluminio en un calculo (solo la masa del (i)) [N].

## 11. No logrado
- **TK-111(a)** (placa 4x4 con una arista «Empotrado» y tres «Articulación», flecha/Dz centro, reacciones, peso por defecto de la lámina 24.525): **no medido**; la selección de aristas individuales de lámina y la regla del original para nudos esquina con aristas de vinculación distinta (#64723: «no se aplicará ninguna de ellas») exigían una sesión nueva con `m6_lam.txt`; queda para la siguiente pasada (el valor 24.525 ya esta [V] por aritmética en `resultados_rea_pasada2.md` §1).
- Mt (torsor) sin medir (la obra no tiene torsor); signo de Fxy/Fxz con carga contraria; escalas de leyes 1.000 con `rp_c1` solo via linealidad (My 0.1/0.5, Vz 0.5/1.0); «Ejes sobre el plano de la ventana» sin efecto visible con barra horizontal.
- La obra `rp_c1.ed3` se abrió con `vh_open.ps1`, se modificó solo en memoria (Describir, apoyos, normas) y se cerró con **No** al guardar; el original reescribio la cabecera de `rp_c1.ed3` al abrirla y se restauró con `git checkout`.
