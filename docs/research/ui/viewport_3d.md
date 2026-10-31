# Ventana 3D del original: ratón y navegación (verificado 2026-10-31, sesión 11)

Método: `tools/drive_original.ps1` (paso nuevo `drag:L|M|R,x1,y1,x2,y2`) sobre `docs/sample_project/dlg_obra.ed3`; capturas `docs/screenshots/vp_*.png` (comparadas píxel a píxel).

| Acción | Resultado verificado |
|---|---|
| Arrastrar con botón central | **Desplaza** la vista; la escena sigue exactamente al ratón (arrastre de +200,-80 px movió los nudos +200,-80 px). |
| Rueda | **Zoom centrado en el cursor** (el punto bajo el cursor queda fijo). 3 muescas hacia arriba ampliaron x1,215, es decir ~6,5 % por muesca. |
| Arrastrar con botón izquierdo (herramienta puntero) | Sin cambio de vista. |
| Arrastrar con botón derecho | Sin cambio de vista. |
| Herramienta de la barra superior (icono de cuatro flechas, x=1473, y=38) + arrastre izquierdo | **Orbita**: arrastre de +200,-80 px llevó la vista por defecto (X abajo-derecha) a una vista casi frontal (X a la derecha, Y hacia el fondo): ~0,25 °/px en acimut y elevación; arrastrar hacia arriba baja la elevación. |
| Herramienta mano (x=1447, y=38) + arrastre izquierdo | Desplaza, pero en la prueba solo se movió en vertical (+50 de +100,+50); sin aclarar. |
| Doble clic en vacío | Sin cambio apreciable (una captura tras desplazar quedó en blanco; no reproducible). |
| Tooltip de coordenadas | Aparece a la derecha del cursor/esquina según "Posición de los tooltips"; formato `x, y, z m` con 3 decimales. |

Otros hechos: la vista por defecto al abrir una obra es la isométrica (yaw 45, pitch 30); fondo blanco; las líneas guía naranjas son familias de rectas paralelas por los nudos (no solo los tres ejes del origen), la T-P4-02 debe replicarlo. Los tooltips de los iconos de la barra no se mostraron con el cursor parado (pendiente identificar los iconos por clic).


Líneas guía (T-P4-02, capturas gl_a0, gl_b2_in2, vp_s5): naranja (~#D98A4A) punteada de 1 px. Por cada nudo una recta completa en Y; en X y Z los nudos alineados se unen con un segmento que sobresale ~12 px (constante al hacer zoom) por los extremos. No hay rejilla. Con rueda -2 desde la vista por defecto el original dejó el área en blanco (sin explicar, TK-024).

Selección (T-P4-03, sesión 11, capturas sel_*.png): con la herramienta puntero por defecto, clic sobre barra, pilar y nudo, arrastres de ventana (izq.-der.) y de cruce (der.-izq.) y paso del cursor NO produjeron resaltado ni selección visible; 4 de 10 capturas salieron con el área 3D en blanco (PrintWindow no captura bien la ventana OpenGL) y el clic en vacío desplazó la obra hacia la izquierda sin explicación. Nudos: cuadrado de contorno azul ~14 px, tamaño constante con el zoom. Conclusión: falta identificar la herramienta de selección real (iconos de la barra superior y de la vertical) antes de implementar.
