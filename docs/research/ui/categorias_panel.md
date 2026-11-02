# Panel Categorías del original

Método: `tools/drive_original.ps1` sobre una copia de `docs/sample_project/dlg_obra.ed3` (pórtico, 3 barras, 2 capas: "por defecto" y "Nueva capa 2" roja asignada a la viga); capturas `docs/screenshots/k2..k8_*.png` (2026-11-02). Obra cerrada sin guardar.

Tabla: columnas **Referencia | ojo | candado**; filas fijas: Nudos, Barras, Tirantes, Pilares, Vigas, Paños, Láminas (todas visibles y desbloqueadas por defecto). Debajo: casilla **"Representar la estructura usando los colores de las capas"** (marcada por defecto) y combo **"Elementos considerados en el análisis"** (por defecto "Tensión / Deformación"; opciones según la web: Tensión / Deformación, Pandeo, Modal).

| Acción | Efecto observado en el visor |
|---|---|
| Ojo de **Nudos** → tachado | Desaparecen los cuadrados de los nudos Y los símbolos de vinculación (empotramientos). Las barras siguen. |
| Ojo de **Barras** → tachado | Desaparecen todas las barras (también la roja) y además los nudos y los símbolos de vinculación. Quedan solo ejes/planos auxiliares. |
| Ojo de **Pilares** / **Vigas** → tachado | Sin cambio: las barras de esta obra son de tipo "Barras" (pilares/vigas/tirantes/paños son tipos de elemento de otros modelos). |
| Candado de **Barras** | Sin cambio visible (bloquea modificaciones; sin comandos que probar). |
| Casilla de colores de capa desmarcada | La barra roja pasa a negro (color por defecto); nudos y vinculaciones no cambian (azules). |
| Combo | Lista no manejable por script; valores tomados de la web. Se guarda con la obra (ver viewport_3d.md). |

Sin comprobar: Láminas/Paños/Tirantes con elementos de ese tipo; efecto real del candado; efecto de Pandeo/Modal.
