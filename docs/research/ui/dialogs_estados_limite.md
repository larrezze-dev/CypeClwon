# Estados límite (T-RE-03)

Capturas: `orig_obra_vacia_12_estados_limite`, `orig_estados_limite_02_config_combinaciones`. [verificado]

Se abre desde el botón "Estados límite (combinaciones)" del asistente/Datos generales y automáticamente tras **Terminar** en el asistente Obra vacía (en ese caso Aceptar entra al editor; Esc/Cancelar vuelve al asistente).

## Diálogo "Estados límite" (358x438)
Cabecera informativa (texto, no editable) dependiente de las normas elegidas:
- "Hormigón: Eurocódigo 2", "Hormigón en cimentaciones: Eurocódigo 2", "E.L.U. de rotura. Aluminio: Eurocódigo 9".
- **Tensiones sobre el terreno**: combo **Nieve**, defecto "Altitud inferior o igual a 1000 m" + icono de edición (lápiz) de combinaciones.
- "E.L.U. de rotura. Acero conformado: CTE DB SE-A" y "E.L.U. de rotura. Madera: CTE DB SE-M": combo **Cota de nieve** (defecto "Altitud inferior o igual a 1000 m") + lápiz.
- "E.L.U. de rotura. Acero laminado: Código Estructural": radios **CTE** (defecto) / **EC**; combo Cota de nieve (mismo defecto) + lápiz.
- **Desplazamientos**: "Acciones características" + lápiz.
- Botón **Configurar combinaciones para cada estado límite** (abre el diálogo siguiente).
- Botones Aceptar / Cancelar.

## Subdiálogo "Estados límite a verificar" (398x356, icono de ayuda en título)
Ocho botones con la combinación vigente a su derecha, todos por defecto:
| Botón | Valor mostrado |
|---|---|
| Hormigón | EC - Altitud inferior o igual a 1000 m |
| Cimentación | EC - Altitud inferior o igual a 1000 m |
| Acero conformado | CTE - Altitud inferior o igual a 1000 m |
| Acero laminado | CTE - Altitud inferior o igual a 1000 m |
| Madera | CTE - Altitud inferior o igual a 1000 m |
| Aluminio | EC - Altitud inferior o igual a 1000 m |
| Tensiones sobre el terreno | EC - Altitud inferior o igual a 1000 m |
| Desplazamientos | Acciones características |
Aceptar / Cancelar.

## Huecos
Lista completa del combo de nieve / Cota de nieve (el desplegable no se abrió con el clic automático); contenido de los diálogos de edición de combinaciones (lápiz y cada botón de la tabla); tabulación.
