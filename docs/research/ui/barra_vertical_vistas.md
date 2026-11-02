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

## Sesión 20: roll, Marcar zoom y Ventana doble (medidos)
- Orbitar desde la celda 7 (roll 180) con la herramienta Órbita 3D mantiene el roll: tras el arrastre vertical Z sigue hacia abajo (t2, o2.png). `Camera::orbit` ya no lo reinicia.
- «Marcar zoom» (z1-z4.png): con el modo activo, al mover el cursor aparece un rectángulo gris con borde oscuro y cursor en cruz, centrado en el cursor y con la proporción del visor; su semiancho es la distancia del cursor al borde derecho del visor (más a la izquierda = rectángulo mayor, menos zoom). El clic zoom a ese rectángulo (factor = ancho visor / ancho rect.; medido 1,14 y comprobado por el desplazamiento de la estructura).
- «Ventana doble» (d1-d3.png): da la misma vista que «Ventana completa»; pulsarla de nuevo no cambia nada. El clon la iguala a «Ventana completa».

## Sesión 21: rotación alrededor de un punto y de la cámara (medidas en el original)
Capturas `rp_a*`, `rp_b*`, `rp_c*`, `rp_montage3/4.png` (obra orig_copy.ed3, herramienta Órbita 3D + arrastre izquierdo; hay que pulsar el título antes de cada acción).
- Los iconos y=260 y y=284 se quedan hundidos (resaltados) al activarse; son independientes de la herramienta Órbita.
- **Por defecto**: el pivote es el centro del modelo (el nudo de partida (930,240) pasó a (852,298) con un arrastre de +100 px).
- **Rotación alrededor de un punto** (y=260): un arrastre horizontal de +100 px iniciado sobre el nudo (930,240) lo deja en (928,240): el punto bajo el cursor es el pivote y no se mueve en pantalla.
- **Rotación alrededor de la cámara** (y=284): con 8 px (2°) o 20 px (5°) el modelo sale por completo de la vista. El tooltip de coordenadas pasa a (187, -187, 152) m: el ojo está a ≈ 302 m del modelo (≈ 30000 px a la escala de la vista), así que girar sobre el ojo desplaza el modelo ~5 m por grado.
- Clon: `Camera::orbitAbout(pivot)` y `orbitAboutEye` (ojo a 38 alturas visibles); pivote = nudo bajo el cursor, punto de la barra bajo el cursor o, si no hay objeto, el punto del plano del objetivo bajo el cursor (comportamiento sin objeto no medido). Con ambas activas gana la cámara (no medido).

## Sesión 21 (II): tipo de proyección, secciones, volumen de recorte, iconos 411/440
Capturas `rq_*.png`, `rs_*.png`, `rq_icons.png`.
- **Tipo de proyección** (y=236): alterna axonométrica ↔ cónica (rq_t1/t2). El original abre en **axonométrica**; el clon ya abre igual (cambio de esta sesión) y el botón alterna.
- **Sección X** (y=314, icono plano con mitad roja): aparece un plano rectangular de contorno gris perpendicular a X, con una **flecha roja gruesa** (sentido de la parte eliminada: +X) y un **arco rojo** alrededor; la geometría del lado de la flecha desaparece (la barra se corta en el plano, rq_x1). Con el script no se consiguió mover el plano ni invertirlo (arrastre y doble clic sobre la flecha: sin cambio en rs_drag/rs_dclick): PENDIENTE de prueba manual del usuario.
- **Volumen de recorte** (y=387): caja de 6 planos en alambre alrededor de la envolvente geométrica (rq_v1/v2). Flechas y arcos de ajuste: sin medir.
- **Icono 411** (plano con ojo y flecha): se hunde al pulsarlo pero no cambia el dibujo; la rueda y el arrastre tampoco cambian nada (rq_w411/d411). Tooltip no capturado: PENDIENTE (pedir al usuario que lo lea con el ratón real).
- **Icono 440** («3D» con engranaje): abre el diálogo **Opciones** (265x136) con dos casillas sin marcar, «Aristas de objetos» e «Iluminación ambiental mejorada (SSAO)», y el botón Aceptar (rq_i2.png).

## Sesión 22: clon de secciones, volumen de recorte, icono 411 y «Opciones»
- `core/clipping`: semiespacios por eje; sección = se conserva el lado opuesto a la flecha roja (flecha +eje elimina coordenada > posición); recorte de segmentos (barras) y de polígonos (láminas, Sutherland-Hodgman); el volumen conserva el interior de la caja de 6 planos (envolvente de los nudos) y sus caras nunca se cruzan.
- Visor: plano gris translúcido + flecha roja + arco rojo; arrastrar la flecha mueve el plano a lo largo del eje; doble clic invierte el sentido (según el tooltip del original). Supuestos aún no medidos en el original: posición inicial (centro de la envolvente), tamaño de flecha/arco, función de los arcos.
- Icono y=411: botón hundible sin efecto visible, igual que el original; sin tooltip hasta que el usuario lo lea.
- Icono y=440: `ViewOptionsDialog` (265x136, solo Aceptar; «Aristas de objetos» dibuja un borde oscuro alrededor de los perfiles/láminas renderizados; «SSAO» se guarda sin efecto).
- Verificación visual del clon (build/smoke/s22_x.png, s22_vol.png, s22_dlg3.png, obra demo.ed3): sección X corta la viga en el plano y oculta el pilar del lado de la flecha; el volumen muestra la caja y 6 flechas; el diálogo Opciones coincide con rq_i2.png tras dar estilo a las casillas de CypeDialog (check_white.svg). Botones de la barra del clon (ventana 1280x800): x=396, y=178/202/226/250 (proyección…cámara), 283/307/331 (secciones X/Y/Z), 362 (volumen), 394 (icono 411), 425 (Opciones). Se abre la obra con `-AppArgs build\smoke\demo.ed3`.

## Sesión 22 (II): análisis del código del original (vistas3d.dll, panelarea.dll)
Método: REA (Ghidra) sobre `vistas3d.dll` (interfaz de las vistas) y `panelarea.dll` (motor de recorte `pnglslclip_*`, `pndibglsl_*clipping*`); la tabla de cadenas de `vistas3d.dll` está en UTF-16 (ids de recurso + 9 idiomas por cadena: es, fr, pt-BR, pt-PT, ca, it, en, de, tr, ro, pl) y sirve también para las traducciones futuras.
- **Icono y=411** = «Mostrar / Ocultar planos de sección» (cadena 270250; en inglés «Show / Hide section planes»). Comprobado en el original (sp_1..sp_3.png, `sp_montage.png`): pulsado **oculta el plano, la flecha y el arco** pero el recorte se mantiene; al soltarlo vuelven. Corresponde al indicador `mostrar_planos_clipping` de la configuración (`pnglslclip_get_cfg_botones`, `pndibglsl_set_visibilidad_planos_rcp_clipping`). Clonado.
- El plano de la sección se dibuja **solo como contorno** (sin relleno); la flecha es roja translúcida con volumen. Clonado el contorno.
- Doble clic en la flecha invierte el sentido (cadena 259959): ya clonado. Volumen: «Utilice las flechas para ajustar los planos que definen el volumen de visualización y los arcos para modificar su orientación» (350823): ya clonado el ajuste por flechas.
- Configuración de recorte (`pnglslclip_cfg_get_planos_clipping_activos`, `FUN_180051650/180051850`): botones X/Y/Z y volumen = «hay un plano activo de ese eje» / «hay volumen activo» (`con_plano_recorte_x/y/z_activo`, `con_recorte_volumen_activo`). Cada plano guarda {punto de referencia (3 doubles), eje 0/1/2, modo 0/1, indicador de sentido (short), desplazamiento (double)}; la configuración guarda además una rotación (eje + ángulo) que se aplica con `bsMat4x4_make_rotation_around_axis` a los planos con modo 1: **los arcos inclinan los planos** (orientación libre, no solo ortogonales a un eje). Pendiente de clonar: inclinación de planos por arcos (requiere semirrecta de rotación y ángulos por arrastre, no medidos).
- «Opciones»: cadenas 278230 «Aristas de objetos» y 278231 «Iluminación ambiental mejorada (SSAO)» (en `vistas3d.dll`); SSAO es un efecto de pantalla en GLSL del motor `bsgesg.dll`/`sismo.dll`.

## Sesión 22 (III): los arcos (medidos en el original y en su código)
- **Código** (`pnglslclip_cfg_representacion_json_para_test`, panelarea.dll): la configuración de recorte guarda `son_visibles_planos_clipping`, `Xo/Yo/Zo_centro_volumen` y `angulo_giro_antihorario_rad_xy_global`: **una única rotación global, antihoraria en el plano XY, alrededor del eje vertical por el centro del volumen**, aplicada a todos los planos y al volumen.
- **Medidas** (ar_*.png, arm_*.png; vista isométrica por defecto, arco agarrado en su punto más a la derecha): el arco es un círculo **horizontal** alrededor de la flecha (elipse en la vista). Con +30 px verticales el plano gira +14,6° (se estrecha, la flecha se endereza), con −30 px −14,2° (se ensancha) y con +30 px horizontales +7,7°; con +100 px vertical queda de canto (≈45°). Ganancia adoptada: 0,48 °/px vertical + 0,26 °/px horizontal. Medida solo en esta vista; el original probablemente calcula el ángulo con el rayo del cursor y el plano horizontal, pero ese modelo no reproduce las cifras (predice el doble en vertical y 0 en horizontal), por lo que se dejó la ganancia empírica.
- **Clon**: `core/clipping` con semiespacios generales (normal + desplazamiento), marco local girado (`centre`, `angleRad`, `toWorld/toLocal/axisDirection`), la rotación vuelve a 0 al quedar sin planos activos; el visor dibuja plano, caja, flechas y arco girados, y arrastrar el arco gira el conjunto (verificado: build/smoke/s22_arc0z.png, s22_arc1z.png).

## Sesión 23: ley del arco leída en el código
Ver `docs/research/ui/clipping_gizmo_code.md` (REA sobre panelarea.dll, pnglslclip.c). Giro incremental: delta = d·2π/D (D = alto o ancho del visor según el eje dominante; sin atan2 ni rayo-plano), lo que reproduce las medidas 0,48/0,26 °/px con un visor ≈1385×750. Clonado en `Viewport3D::dragClipArc`. Pendientes: posición inicial del plano (clic sobre el modelo), límite ±80° del arco por plano.
