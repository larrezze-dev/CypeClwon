# Panel Etiquetas (capas) del original

Método: `tools/drive_original.ps1` sobre `docs/sample_project/dlg_obra.ed3`; capturas `docs/screenshots/l*.png`, `m*.png`, `n*.png`, `o*.png` (2026-11-01). Las acciones no se grabaron en la obra (se cerró sin guardar).

Tabla: columnas **Referencia | Activa | Color | cubo | ojo | candado**. Fila inicial "por defecto" (activa, negra, visible, desbloqueada). Barra de herramientas (y=191): `+`, `×` (rojo), `↑`, `↓`, icono de opciones.

| Control | Comportamiento observado |
|---|---|
| `+` | Tooltip "Añadir / Añadir nuevo elemento a la lista". Crea una fila "Nueva capa N" (N = número de capas tras añadir: 2, 3...), color negro, NO activa, visible, desbloqueada, y la deja seleccionada. |
| Doble clic en el nombre | Edición en línea del texto (cursor en el campo). Esc deja el nombre como estaba. |
| Activa | Casilla exclusiva: marcar la de una capa desmarca la anterior (siempre hay una). |
| Color | Clic abre el diálogo "Selección del color" (paleta de 2 filas de básicos + rejilla 24x9, botón "Más colores...", Aceptar / Cancelar, 707..1223 x 323..718). |
| Cubo | Alterna entre cubo de contorno y cubo relleno (azul). Significado sin determinar (¿capa visible como sólido?). |
| Ojo | Alterna ojo / ojo tachado (capa oculta). |
| Candado | Alterna candado abierto / cerrado (capa bloqueada). |
| `↑` / `↓` | Tooltips "Mover un lugar hacia arriba / abajo el elemento seleccionado de la lista". Intercambian la fila seleccionada con la vecina; el orden se conserva. |
| `×` | Tooltip "Borrar / Suprimir elemento seleccionado de la lista". Pregunta "¿Desea borrar la capa 'Nueva capa 2'?" con Sí / No (diálogo "Pregunta"). |
| Icono de opciones | Tooltip "Asignar elementos a etiquetas". Es una HERRAMIENTA: barra de estado "Herramientas - Asignar elementos a etiquetas | - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados." Abre una ventana "Propiedades - Selección" con la casilla "Barras". Esc no la cancela. |

Sin comprobar: borrar "por defecto"; borrar una capa con elementos; qué hace el cubo; cómo se elige la capa en la ventana "Propiedades - Selección"; efecto de la capa activa al crear barras; efecto de ojo/candado/color sobre el viewport.
