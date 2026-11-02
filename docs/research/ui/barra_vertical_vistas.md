# Barra vertical de vistas y barra de zoom/encuadre (T-P4-09)

Investigado en el original el 2026-11-02 (sesión 17) con `tools/drive_original.ps1` sobre `dlg_obra.ed3`. Capturas: `docs/screenshots/vb_*.png`, `vb_tips.png` (tooltips), `vb_full.png` (paleta abierta), `pj_01..22.png` (una por proyección), `pj_triads.png`.

## Barra vertical (x≈402, junto a la ventana "3D")

| y | Tooltip del original |
|---|---|
| 212 | **Proyección**: abre una ventana que permite seleccionar proyecciones predeterminadas. Clic alternativo sobre el mismo botón muestra/oculta la ventana. |
| 236 | **Tipo de proyección**: cambia el tipo alternativamente (axonométrica/cónica). |
| 260 | **Rotación alrededor de un punto**: activa/desactiva la rotación alrededor de un punto de un objeto situado bajo el cursor. |
| 284 | **Rotación alrededor de la cámara**: activa/desactiva la rotación alrededor del eje vertical de la cámara. |
| 314 | **Sección perpendicular al eje 'X' global**: define una sección del modelo; doble clic en la flecha invierte el sentido del plano de recorte. |
| 339 | Ídem eje 'Y' global. |
| 363 | Ídem eje 'Z' global. |
| 387 | **Volumen de recorte**: volumen de visualización mediante 6 planos de corte a partir de la envolvente geométrica; flechas para ajustar los planos y arcos para su orientación. |
| 411 | Sin tooltip en 3 s (icono de dibujo con cámara/flecha). Sin identificar. |
| 440 | Icono "3D" con engranaje, separado por línea: sin tooltip capturado. Sin identificar. |

## Paleta «Proyección»

Ventana flotante propia (116x292 px, esquina superior izquierda en ≈ (415,201) de la ventana principal), pegada a la barra vertical. 3 columnas x 7 filas + 1 celda centrada (22 proyecciones). Centros de celda (pantalla): x = 434 / 472 / 510; y = 219, 258, 289, 320, 357, 396, 434 y (472, 471). Los clics deben hacerse en coordenadas relativas a la paleta con `drive_original.ps1` porque `Top()` devuelve la paleta (origen ≈ (415,201)). El primer clic tras abrir se pierde (activación).

Contenido (iconos = triedro dibujado; entre paréntesis la orientación medida en la captura):

| Fila | Celdas | Significado |
|---|---|---|
| 1 | 1-3 | Z arriba / X derecha (alzado frontal); Y arriba / X derecha (planta); Z arriba / Y derecha (lateral) |
| 2 | 4-6 | Simétricas: Z arriba / X izquierda (posterior); Y arriba / X izquierda (inferior); Z arriba / Y izquierda |
| 3 | 7-9 | Las mismas vistas giradas 180° (Z abajo; Y abajo): requieren giro de cámara (roll) |
| 4 | 10-12 | Axonometrías desde arriba con X e Y alejándose (cámara en -X,-Y): yaw 300/315/330, pitch ≈40-55 |
| 5 | 13-15 | Axonometrías desde arriba con X hacia abajo-derecha e Y arriba-derecha: yaw 30/45/60, pitch ≈40-50 (la 14 es la isométrica por defecto) |
| 6 | 16-18 | Axonometrías desde abajo (pitch ≈ -40…-60), yaw 300/315/330 |
| 7 | 19-21 | 19 inferior (Y abajo, X derecha); 20-21 vistas con Y en profundidad (Z arriba/X derecha) |
| 8 | 22 | Z arriba / X derecha, Y casi en profundidad |

Ajuste numérico (`tools/fit_projections.py`, error angular ±5°): filas 4-6 reproducidas con (yaw, pitch) del modelo de cámara del clon. Los valores exactos de la fila 3 y de 20-22 no se han podido fijar (ver TK-030).

## Barra superior de zoom/encuadre

Ya documentada en `viewport_3d.md` (tabla de iconos x=1310…1499): Ventana anterior (Ctrl+P), Ventana completa (Ctrl+W, también doble clic con el botón central), Ventana doble, Redibujar (Ctrl+R), Marcar zoom (Ctrl+M), Mover imagen (mano; también botón central), Órbita 3D (F1 muestra/oculta textos de ayuda), Imprimir la vista actual.

## Pendiente de investigar
- Efecto de «Tipo de proyección» (cónica) y de las secciones/volumen de recorte (T-P4-10+ / ticket).
- Iconos de y=411 y y=440.
- Comportamiento exacto de «Ventana doble» y «Marcar zoom».

## Sesión 19: celdas 19-22 y zoom
- Medidas con la longitud relativa de los ejes (c20.png, c22.png): 19 = yaw 0 / pitch -45 (Y abajo, Z y Y igual de largos); 20 = yaw 0 / pitch +45 (Y oculto tras Z, Z al 71 % de X); 21 = yaw 315; 22 = yaw 45 / pitch 0 (Y coincide con X, X al 73 % de Z). Celda 8 = (0,-90) sin roll; 7 y 9 con roll 180.
- «Marcar zoom» (a5, b3-b6): tras activarlo y hacer clic aparece un rectángulo de goma con la proporción del visor (~1,9) y cursor en cruz; un segundo clic aplica el zoom. Falta fijar el anclaje del rectángulo con un arrastre diagonal.
- «Ventana doble»: en las pruebas a2 y b2 no cambió la escala respecto a lo esperado; por comprobar si exige un clic posterior en el dibujo.
- Los scripts con sshot devuelven tooltips si el ratón queda sobre la barra: usar `move:960,800` y `fshot`.
