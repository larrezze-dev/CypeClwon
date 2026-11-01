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

## Ronda 2 (2026-11-01, capturas p*, q*, r*, s*, t*)

- **Cubo**: sin tooltip (no sale tras 3,5 s de hover). Significado aún sin determinar.
- **Borrar "por defecto"**: se puede. Misma pregunta "¿Desea borrar la capa 'por defecto'?" (Sí/No; Esc = No). Tras Sí desaparece y la capa restante pasa a ser la activa.
- **Borrar la última capa**: aviso (título "Aviso", botón Aceptar) "Debe definir, al menos, una capa." y no se borra. Por tanto el modelo del original NO exige la capa "por defecto", solo al menos una capa.
- **Nombre de `+`**: tras borrar "por defecto" y quedar solo "Nueva capa 2", `+` creó "Nueva capa 3" (no "2"): el número es el de capas tras añadir, saltando los nombres ya usados.
- **Herramienta "Asignar elementos a etiquetas"**: al pulsarla aparece sola la ventana "Propiedades - Selección" (casilla "Barras", resto vacío). El script no consigue cerrarla (X no responde) ni seleccionar con ventana de dos clics en el visor; sin resolver, se pide ayuda manual.

## Ronda 3: capturas manuales del usuario (docs/screenshots/u01..u11_user_layers.png)

- **Cambiar el color** de una capa: el diálogo "Selección del color" cambia la columna Color (probado con rojo).
- **Herramienta "Asignar elementos a etiquetas"** (icono de la barra): LMB selecciona una barra (aparece naranja); RMB sobre la selección abre el diálogo **"Asignar elementos a etiquetas"** (con la ventana "Propiedades - Selección" detrás, que lista "Barras"):
  - Grupo **Operación**: radios `Asignar` / `Reemplazar` (por defecto) / `Eliminar`.
  - Grupo **Etiquetas**: tabla `Seleccionado` (casilla) | `Referencia` con una fila por capa. Botones `Aceptar` / `Cancelar`.
- **Un elemento puede pertenecer a varias capas.** La caja de información de la barra muestra "Pieza (N2/N3): L. 4 m / Capa : por defecto y Nueva capa 2 / IPE 80 (S275 (UNE-EN 10025-2))" tras `Asignar` y "Capa : Nueva capa 2" tras `Reemplazar` (las barras nuevas nacen en "por defecto").
- Con la herramienta no activa, el tooltip negro de coordenadas ("184.323, -188.776, 154.627 m") sí sale en la pestaña Obra junto al cursor (confirma TK-027 a).
- Sin capturas del usuario aún: efecto del cubo, del ojo, del candado y del color sobre las barras en el visor; qué pasa al borrar una capa con elementos.
