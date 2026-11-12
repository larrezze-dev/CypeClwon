# Cálculo > Tensión / Deformación: GUI del clon (T-P8-06)

Fecha: 2026-11-12. Agente: FRONTEND-DEVELOPER. Complementa `docs/research/resultados_calculo.md` (núcleo) y `docs/research/ui/resultados_paneles.md` (capturas del original `rp_*.png` hechas en esta sesión por el RE-ANALYST).

Marcas: **[V]** verificado en el original (captura, medida o cadena); **[R]** deducido de cadenas REA; **[N]** decisión del clon.

## 1. Fuentes por vía

- **REA (cadenas de `ed3dlib`/`combsanl`, `docs/research/ui/data/strings_*.tsv`)**: textos del diálogo Cálculo (#377030, #377031, #377977, #377978, #64348, #63989), «Informe final de cálculo» (#74974), «Cálculo de la estructura» (#64505), «Consultar valores» (#64438), «Sólo las barras seleccionadas» (#65162), #64071/#64073 (clic sobre barra / nudo muestra y oculta el globo), #64160 (Deformada y flechas en el panel Esfuerzos), filas de «Tipos de combinación» (#377831-#377838), «Hay hipótesis repetidas en la misma combinación.» (#377002), #378563, #380751/#380752.
- **Original en ejecución**: `cp_61_calcdlg.png`, `rr_e2_dlg.png` (diálogo Cálculo sin y con elementos no lineales), `s_5.png` (informe), `s_9.png`/`s_10.png` (combinaciones no lineales), `s_lin.png` (Desplazamientos) y `rp_00..rp_34` (Reacciones, Esfuerzos, estado de los botones; ver `resultados_paneles.md`).

## 2. Implementación

| Elemento | Clon | Marca |
|---|---|---|
| Pestaña Cálculo | Grupos Tensión / Deformación, Pandeo, Modal, Pushover, Incidencias con los botones y su estado de `s_5.png`; los de fases posteriores avisan «Esta función aún no está disponible.» o están inactivos como en el original | [V] distribución |
| Diálogo «Cálculo» | Sin elementos no lineales: banda informativa de fuego + Perfiles + Comprobar las barras (marcada) + dimensión finita. Con muelles con Sentido: «Análisis a realizar» (lineal, no lineal, «Actualizar…» solo si la obra ya tiene combinaciones NL), «Análisis de segundo orden elástico» | [V] |
| Banda «No se ha activado la opción de comprobación de resistencia al fuego.» | Se muestra solo sin elementos no lineales (encaja con las dos capturas; la condición real es desconocida) | [N] |
| Cálculo en segundo plano | `analyseProject` sobre una copia de la obra en un `QThread`, ventana «Cálculo» con barra de progreso indeterminada (el texto de la ventana de proceso del original no se conoce) | [N] |
| «Actualizar las combinaciones no lineales…» | Es dato de la obra (`NonlinearCombinations::updateAfterLinear`); al aceptar se aplica a la copia y se escribe en la obra, junto con las combinaciones regeneradas, en un solo paso deshacible «Cálculo» | [V] el dato; [N] que sea deshacible |
| Informe final | Página con título, «Cálculo de la estructura» y una línea por mensaje con icono (punto rojo = error, triángulo amarillo = aviso); zoom e Imprimir; Compartir/Exportar inactivos | [V] contenido |
| Resultados inactivos | Hasta un cálculo correcto; cualquier edición nueva, deshecha o rehecha los invalida (guardar no) | [V] `rp_33` |
| Desplazamientos | Clic en barra: dos globos sin cabecera con los valores de **extremo de barra** (`barDisplacements`), etiqueta «N1» verde, barra marrón #B08457; segundo clic los oculta | [V] |
| Reacciones | Clic en nudo con reacciones: globo «N1 (0, 0, 0)» + Rx..Mz (kN 3 decimales, kN·m 2); segundo clic lo oculta. El símbolo de apoyo marrón claro no se reproduce | [V] / pendiente |
| Esfuerzos | Radios Leyes/Envolventes y Todas/Sólo seleccionadas (+ Consultar valores, que se marca solo), 10 filas con casilla y escala (0.100 / 100.000), colores medidos, relleno semitransparente, «Dibujar valores máximos y mínimos» con rótulos «My máx.: … / X: …», Deformada (barras: `barDisplacementAt`; láminas: aristas de los triángulos de malla con sus nudos intermedios) | [V] panel, colores, escalas |
| Escala de las leyes | longitud dibujada (m) = valor × escala × 0,1 (medido ≈ 0,0099 m por kN·m con 0.100) | [≈V] |
| Escala de la deformada | posición + escala × desplazamiento (m); la unidad del campo no se muestra | [N] (cierra TK-104 en cuanto a los valores por defecto) |
| Dirección de dibujo de las leyes | N, Vz, Mt, My según z local; Vy, Mz según y local; valor positivo hacia +eje | [N] |
| Consultar valores | Botón derecho sobre una barra seleccionada: globo con las filas de la barra («Barra N1/N2: L. 4 m», «Capa : …», perfil), «X: … m» y una fila por ley marcada | [V] |
| Combinaciones no lineales | Lista Referencia/Actúa/Escalones (escalones ≥ 1, un valor no válido se rechaza), subir/bajar/borrar/borrar todas, flecha azul con el diálogo de 6 casillas y mensaje #378563, «Tipos de combinación» con 8 filas Referencia/Usar, «Factores de combinación» Hipótesis/Coeficiente con #377002 al aceptar | [V] estructura, [R] filas |
| Borrar hipótesis | #64905 solo cargas (formato medido), #380751 solo combinaciones NL, #380752 ambas; formato «texto: 'ref'. ¿Desea continuar?» copiado del #64905. Cancelar en «Hipótesis…» restaura también las combinaciones NL (antes no) | [V] #64905, [N] formato de los otros dos |
| Peso propio por barra | Ya existía (Carga > Barras > Peso propio, `BarLoadTool`), y el núcleo lo respeta (`FrameBuildOptions::honourSelfWeightSwitch`) | [V] |

## 3. No cableado o no medido

- Opciones del diálogo Cálculo mostradas pero inactivas: dimensionamiento rápido/óptimo (fase 9), análisis de segundo orden (fase 10), dimensión finita de los nudos (TK-106). «Comprobar las barras» se guarda; la comprobación es la fase 9 (el informe sigue diciendo «calculada sin realizar la comprobación»).
- Desmarcar «Análisis lineal» con elementos no lineales: el clon calcula igualmente lo lineal (lo necesita el no lineal); si se desmarcan los dos, no se calcula nada [N].
- Envolventes (todos los paneles), familias de combinaciones lineales en el primer combo (Desplazamientos, Acero laminado y armado, …, vistas en `rp_22`), flechas Fxy/Fxz/F, «Ejes sobre el plano de la ventana», «Ver valores máximos y mínimos» y «Ver la estructura» (se guardan, sin efecto), ejes locales dibujados con el panel abierto, símbolo de apoyo marrón.
- Pincel del diálogo de combinaciones no lineales (función no identificada), tooltips de borrar/borrar todas [N], referencia propuesta «Combinación n» y coeficiente 1 al añadir [N], #377829.
- El primer panel de cada obra sale maximizado en el original; el clon lo abre con el tamaño de `s_lin.png`.
