# Regla de orden de tabulación del clon (decisión del usuario, 2026-11-04)

**Estado: decisión de diseño del usuario, confirmada como regla general; NO medida en el original.** El único orden medido en el original está en `t_re_03_cierre_huecos.md` §9 («Descripción de la obra»), coherente con la regla.

## Regla madre (todos los diálogos)
Se aplica tal cual, **por posición en pantalla**:
1. Dentro de cada «spot» (grupo/caja con título), el foco recorre los controles de arriba hacia abajo y, a igual altura, de izquierda a derecha.
2. Si el diálogo tiene varios spots, se recorren por filas de spots: izquierda a derecha dentro de cada fila, y de arriba hacia abajo entre filas. Un campo suelto de ancho completo situado arriba (p. ej. «Normas») va antes que los spots que quedan debajo; lo que está más abajo va después.
3. Los botones de la fila inferior (Aceptar, Cancelar) van los últimos y el foco vuelve al primer control. Los iconos de barras de herramientas (+/X) no son parada de tabulación (observado en el original).

## Ejemplo: Datos generales
Por posición: «Normas» (arriba), «Perfiles» (superior izquierdo), «Hormigón armado» (derecha), «Acciones», «Terreno de cimentación», y por último «Opciones» y «Ambiente» según su posición (columna izquierda, de arriba abajo). Dentro de cada spot, arriba-abajo e izquierda-derecha.

Implementación: ordenar los controles por (fila de spot, columna de spot, y, x) al construir el diálogo, o fijar `setTabOrder` explícito siguiendo este criterio.
