# Paneles de resultados del original (Cálculo > Desplazamientos / Reacciones / Esfuerzos)

Marcas: [V] verificado en el original (captura) · [N] no verificado / no medido.
Obra de prueba: copia `docs/sample_project/rp_c1.ed3` (voladizo C1, N1 (0,0,0) empotrado, N2 (4,0,0), barra N1/N2 L = 4 m, perfil genérico, hipótesis «Peso propio»). Cálculo > Calcular sin dimensionar ni comprobar. Capturas en `docs/screenshots/rp_*.png` (pantalla completa 1920x1080 salvo indicación). Vía: solo observación del original (los textos de ayuda ya estaban en `data/strings_ed3dlib.tsv`).

## 1. Estado de los botones

- [V] Antes de calcular (`rp_01_precalc.png`), y tras editar la obra: el grupo Tensión / Deformación deja **inactivos** (gris) Desplazamientos, Isovalores, Reacciones, Empresillado, Esfuerzos, Comprobar elementos y Comprobaciones E.L.U. (Sismo dinámico y Dimensionar también en gris). Activos: Combinaciones no lineales, Calcular.
- [V] Tras Calcular y cerrar el informe (`rp_02_postcalc.png`) se activan los siete botones anteriores (Dimensionar sigue en gris con «No dimensionar»).
- [V] **Editar una carga invalida los resultados**: borrar una carga de nudo (Carga > Nudos > Borrar: 1.er clic izq. resalta, clic derecho borra) volvió a dejar inactivos los siete botones (`rp_33_after_delete.png`) sin necesidad de más acciones. Clon: cualquier edición de modelo o cargas pone `resultsValid = false`.
- [V] Cada botón es un modo exclusivo (toggle resaltado); abrir otro cambia el panel flotante. Primera apertura en una obra: el panel sale **maximizado** (`rp_03_react_open.png`, 1920x1080); se restaura y queda en franja izquierda (x 8..528, y 232..1032).
- [V] Pie de la ventana principal con un modo activo: «Tensión / Deformación - Desplazamientos | - LMB: Seleccione un elemento.»

## 2. Panel «Propiedades - Reacciones» (`rp_03`, `rp_04`)

Orden de controles [V]:
1. Radios (grupo sin título): **«Combinación seleccionada»** (por defecto) · «Envolventes de las combinaciones de equilibrio de cimentación» · «Envolventes de las combinaciones de tensión sobre el terreno».
2. Grupo «Comb. seleccionada»: combo 1 (defecto «Hipótesis simple») y combo 2 (defecto «Peso propio»; lista de hipótesis de la obra).
3. Grupo «Reacciones»: casillas **Rx, Ry, Rz, Mx, My, Mz**, todas marcadas por defecto.
Nada más debajo. Los radios de envolventes no se pulsaron (no hay combinaciones de cimentación en la obra de prueba) [N].

**Globo** (clic izq. sobre un nudo apoyado; segundo clic en el mismo nudo lo oculta, `rp_05_react_hide.png`) [V]:
- Cabecera en negrita «N1 (0, 0, 0)» (referencia y coordenadas en m, sin unidad).
- Una fila por casilla marcada, etiqueta en negrita: `Rx: -10.000 kN`, `Ry: -10.000 kN`, `Rz: 10.000 kN`, `Mx: 0.00 kN·m`, `My: -40.00 kN·m`, `Mz: -40.00 kN·m`. **Fuerzas 3 decimales kN, momentos 2 decimales kN·m**, punto decimal, signo explícito solo si negativo.
- Marco blanco con borde negro fino, fuente del tooltip de la aplicación, anclado sobre el nudo (flecha hacia el nudo). El rótulo verde «N1» del nudo se mueve al lado.
- Al abrir Reacciones los símbolos de apoyo pasan de azul a **marrón claro** mientras hay globo; sin globo vuelven a azul turquesa (`rp_05`).

## 3. Panel «Propiedades - Esfuerzos» (`rp_06`..`rp_17`)

Tooltip del botón [V]: «Esfuerzos — Dibuja en pantalla las leyes de esfuerzos, deformada y flecha, o bien las envolventes de esfuerzos, flecha y tensión, tanto de las barras como de las bandas de integración. Estos datos pueden ser relativos a las hipótesis simples o a combinaciones de esfuerzos. Además, permite la consulta en un punto tanto para barras como para bandas de integración de los valores de las gráficas seleccionadas.»

Orden de controles (modo **Leyes**, **Todas las barras**) [V `rp_07_eff_panel.png`]:
1. Radios: **«Leyes»** (defecto) · «Envolventes».
2. Radios: **«Todas las barras»** (defecto) · «Sólo las barras seleccionadas»; debajo casilla «Consultar valores» (deshabilitada y desmarcada con «Todas las barras»; al elegir «Sólo las barras seleccionadas» se habilita).
3. Diez filas `[muestra de color] [casilla] etiqueta  [campo de escala]` (la muestra de color solo aparece mientras la casilla está marcada):

| Etiqueta | Escala por defecto | Color [V] |
|---|---|---|
| Axil (N) | 0.100 | #008000 |
| Cortante y (Vy) | 0.100 | #FF00FF |
| Cortante z (Vz) | 0.100 | #000080 |
| Momento torsor (Mt) | 0.100 | #800080 |
| Momento y (My) | 0.100 | #008080 |
| Momento z (Mz) | 0.100 | #0000FF |
| Deformada (D) | 100.000 | #FF3F00 |
| Flecha xy (Fxy) | 100.000 | #00BFFF |
| Flecha xz (Fxz) | 100.000 | #667FCC |
| Flecha (F) | 100.000 | #CC7F66 |

   Todas las casillas desmarcadas por defecto. El primer campo (Axil) llega con el foco y el texto seleccionado. Los campos tienen 3 decimales. **Esto cierra TK-104: escala por defecto de leyes 0.100 y de deformada/flechas 100.000** (unidad de las escalas [N]: no se muestra ninguna).
4. Casillas (separadas por línea): «Ejes sobre el plano de la ventana» (desmarcada) · «Dibujar valores máximos y mínimos» (desmarcada) · «Ver valores máximos y mínimos» (deshabilitada hasta marcar la anterior) · **«Dibujar las leyes con relleno» (marcada)** · **«Ver la estructura» (marcada)**.
5. Grupo «Comb. seleccionada»: los mismos dos combos que en Reacciones («Hipótesis simple» / «Peso propio»).

Variantes [V]:
- **«Envolventes»** (`rp_08_eff_env.png`): desaparece la fila «Deformada (D)» (quedan 9 filas: N, Vy, Vz, Mt, My, Mz, Fxy, Fxz, F con las mismas escalas) y desaparece el grupo «Comb. seleccionada» (sin combos). Resto de casillas igual.
- **«Sólo las barras seleccionadas»** (`rp_09_eff_sel.png`): se marca sola «Consultar valores» al activarla por primera vez en esta sesión; desaparecen «Ejes sobre el plano de la ventana» y «Ver valores máximos y mínimos» (quedan «Dibujar valores máximos y mínimos», «Dibujar las leyes con relleno», «Ver la estructura»). La fila «Deformada (D)» **también desaparece** (solo 9 filas), igual que en Envolventes.
- «Ver valores máximos y mínimos» (casilla con «Todas las barras» + «Dibujar valores máximos y mínimos»): marcada no produjo cambio visible en el dibujo con una ley [N]; con las diez leyes activas aparece marcada pero en gris (deshabilitada) (`rp_17_all_laws.png`).
- «Dibujar valores máximos y mínimos» fuerza la lectura sobre el 3D (rótulo en el extremo de la ley, ver 4.2).

### 3.1 Selección de barras y consulta (Sólo las barras seleccionadas) [V]
- Clic izq. sobre la barra: se **selecciona** (la línea de la barra pasa a naranja #E88F38 con borde #A37F00 junto a la ley) y se dibuja su ley; otro clic la deselecciona (no probado) [N]. Con «Todas las barras» se dibujan en todas sin seleccionar.
- Con «Consultar valores» marcado, el **clic derecho** sobre la barra seleccionada muestra el globo (`rp_11_my_query.png`), anclado en el punto pulsado:
  - fila 1 negrita: `Barra N1/N2: L. 4 m`
  - fila 2: `Capa : por defecto`
  - fila 3: `Gen.(0.0 kg/m)` (generador de barra; peso por metro según el perfil)
  - fila 4: `X: 1.999 m` (abscisa del punto pulsado desde el nudo inicial, 3 decimales)
  - una fila por ley marcada, solo esas: `My: -20.01 kN·m` (axiles/cortantes 3 decimales kN, momentos 2 decimales kN·m; flechas en mm presumiblemente 3 decimales [N]).
  Filas separadas por líneas finas grises.
- Gizmo de ejes locales de la barra dibujado en el 3D mientras el panel Esfuerzos está abierto (flechas roja x, verde y, azul z en el extremo; `rp_07`) [V].

## 4. Dibujo de las leyes [V `rp_10`..`rp_17`]

1. **Ley con relleno**: contorno de la ley en el color de la ley (My #008080, 1 px), relleno semitransparente (sobre fondo blanco medido #C1C162 para My; con varias leyes se mezclan), la ley se dibuja en el plano de flexión (My en el plano x-z de la barra, hacia arriba para valores negativos de My en la obra de prueba), unida a la barra en los extremos por segmentos verticales. Sin relleno («Dibujar las leyes con relleno» desmarcada) solo contorno [N, no capturado]. «Ver la estructura» desmarcada oculta la barra [N].
2. **Rótulos de extremos** («Dibujar valores máximos y mínimos»): texto grande en negrita en el **color de la ley**, dos líneas, junto al punto extremo, con un punto marcador y una línea de cota discontinua:
   - `My mín.: -40.00 kN·m` / `X: 0.000 m` (en el empotramiento)
   - `Mz máx.: 40.000 kN·m` (valor mostrado según magnitud, 3 decimales cuando es kN) — con las diez leyes se ven también `N mín.: 10.000 kN`, `Vy máx.: 10.000 kN`, `Vy mín.: 10.000 kN`, `Vz máx.: -10.000 kN`, `F máx.: 4.367 mm` (X: 1.750 m), `Fxy mín.: -3.5xx mm` (X: 1.750 m), `Fxz máx.: 1.953 mm` (X: 1.750 m). Formato: `<Ley> máx.|mín.: <valor> <unidad>` + `X: <abscisa> m`. Los momentos llevan 2 decimales en el rótulo de My y 3 en el de Mz según captura (posible inconsistencia de lectura) [N, comprobar].
3. **Deformada (D)** (`rp_15_deformada.png`): polilínea roja-naranja #FF3F00, 1 px, sobre la barra original (que queda negra con «Ver la estructura»); con escala 100 la punta del voladizo (6.7 m de desplazamiento axial en la obra) se sale de la vista y el tramo exterior se dibuja **discontinuo** (puntos). Desplazamiento = valor × escala [N: referencia de longitud exacta].
4. **Flecha (F)** (`rp_16_flecha.png`): curva salmón #CC7F66 (fina) más una línea vertical/discontinua de cota en la sección del máximo y rótulo `F máx.: 4.367 mm` / `X: 1.750 m`. F = flecha respecto a la cuerda entre extremos; Fxy y Fxz son sus componentes.
5. Escala de dibujo: leyes de esfuerzos 0.100 dio una altura de 127 px en el empotramiento (|My| = 40) con la barra de 4 m ≈ 1140 px proyectados, es decir ~0.0099 m por kN·m en vista isométrica; la fórmula exacta (m por unidad, ¿por 1 kNm?) [N].

## 5. Panel «Propiedades - Desplazamientos» (`rp_18`..`rp_24`)

Ya documentado en `s_lin.png`; completado [V]:
- Radios: «Combinación seleccionada» / «Envolventes». Con «Envolventes» el grupo «Comb. seleccionada» queda **deshabilitado en gris** (no oculto, a diferencia de Esfuerzos; `rp_21_disp_env.png`). Casillas Dx..Gz iguales.
- **Primer combo de combinaciones** (lista desplegable, `rp_22_combo1_list.png`), entradas literales y en este orden: **Hipótesis simple · Desplazamientos · Acero laminado y armado · Acero conformado · Madera · Aluminio · Hormigón**. Defecto «Hipótesis simple».
- **Segundo combo**: depende del primero; con «Hipótesis simple» lista las hipótesis de la obra (aquí solo «Peso propio», `rp_23_combo2_list.png`).
- **Globos por barra** (clic izq. sobre la barra, `rp_19_disp_bar1.png`): dos globos, uno en cada extremo (N1 y N2), sin cabecera, filas en negrita `Dx: 6666.667 mm`, `Dy: 0.000 mm`, `Dz: 0.000 mm`, `Gx: 0.000 mRad`, `Gy: 1574.835 mRad`, `Gz: 8.000 mRad` (mm y mRad, 3 decimales). La barra se dibuja marrón (#B08457 medido) mientras tiene globos.
- **Segundo clic sobre la misma barra oculta los globos** (`rp_20_disp_bar2.png`), de acuerdo con el texto #64071 («Para ocultar la información de desplazamientos en una barra previamente seleccionada, vuelva a pulsar el botón izquierdo del ratón sobre la misma.»). En el original el panel de Desplazamientos **no** dibuja deformada ni ofrece escala; la deformada vive solo en Esfuerzos (ley «Deformada (D)»).
- Con el cursor sobre una barra aparece (esquina superior derecha, configuración «Posición de los tooltips = En la esquina superior dere...») el cuadro de información del elemento: `Barra N1/N2: L. 4 m` / `Capa : por defecto` / `Gen.(0.0 kg/m)` (`rp_19`, también visible en `rp_26`).

## 6. No medido / pendiente

- [N] Contenido del combo 1 desplegado en Reacciones y Esfuerzos (se supone igual que en Desplazamientos; solo se vio en Desplazamientos).
- [N] Envolventes de Reacciones (necesitan combinaciones de cimentación) y los textos de las listas de envolventes; combos de Envolventes en Desplazamientos (deshabilitados).
- [N] Unidad de los campos de escala; fórmula exacta de la escala de leyes y de la deformada; dibujo sin relleno; efecto de «Ver valores máximos y mínimos»; leyes de varias barras con valores mixtos.
- [N] Globo de Esfuerzos con «Consultar valores» y varias leyes simultáneas (formato de una sola ley verificado).
- [N] Selección múltiple de barras y deselección por segundo clic en Esfuerzos.
- Obra de prueba modificada (borrada una carga de nudo) **no se guardó** (No en «¿Desea grabar la obra?»); `rp_c1.ed3` queda intacta como copia de `sv_c1.ed3`.
