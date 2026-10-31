# Inventario de interfaz – editor principal (capturas 05–07)

Título: `CYPE 3D v2027.a - ref.ed3`. Barra superior roja con logo, barra de acceso rápido (guardar, deshacer, rehacer, imprimir, …), pestañas ribbon, y a la derecha *Iniciar sesión* / *CYPE Mentor*, minimizar/restaurar/cerrar.

## Pestañas del ribbon y grupos (observado)

**Obra** – *Datos generales*; grupo *Obra*: Biblioteca de perfiles, Catálogos de fabricante, Biblioteca de rótulas plásticas, Rendimiento, Unidades de acotación; *Numeración*: Editar, Reiniciar; *Vista 3D*: Estructura completa, Ventana activa, Selección de elementos, Uniones; *Listados*: Elementos seleccionados, CSV de los elementos…, Etiquetas de vigas; *BIMserver.center*: Vincular, Desvincular, Consultar, Compartir.

**Geometría** – *Planos*; *Nudos*: Nuevo, Borrar, Mover; *Barras*: Barra, Pilar, Viga, Pieza, Sección, Material, Borrar, Disposición, Mover extremo, Invertir eje 'x', Información, Agrupar, Desagrupar, Edición de pilares, Edición de vigas; *Láminas*: Nueva, Borrar, Sección, Mover, Unir, Material, Dividir, Invertir eje 'z', Definir eje 'x', Ejes locales; *Herramientas*: Generar y 8 herramientas (mover/copiar/…, reparar); *Importar*: DXF o DWG, Fichero de texto.

**Propiedades** – *Diafragmas*: Editar, Diafragma, Generar por niveles; *Nudos*: Vinculación interior, Vinculación exterior, Ligaduras; *Barras*: Pandeo, Pandeo lateral, Parejas, Articular, Empotramiento, Rótulas plásticas, Grupos de flecha, Resistencia al fuego, Análisis modal; *Láminas*: Vinculación interior/exterior, Bandas de integración, Discretización, Factores de rigidez, Pandeo.

**Carga** – *Visualización*: Hipótesis vista, Escalas; *Nudos*: Desplazamientos prescritos, Nueva, Editar, Borrar, Hipótesis; *Barras*: Nueva, Borrar, Editar, Peso propio, Hipótesis; *Láminas*: Nueva, Borrar, Editar, Peso propio, Hipótesis; *Paños*: Nuevo, Borrar, Editar, Mover, Dividir, Dirección de reparto; *Cargas superficiales*: Nueva, Borrar, Editar, Copiar, Mover, Asignar, Hipótesis.

**Uniones** – Nueva, Borrar, Editar, Asignar, Agrupar, Desagrupar, Bloquear, Desbloquear, Buscar, Generar, Rigideces rotacionales.

**Cálculo** – *Tensión/Deformación*: No lineales (NL), Calcular, Dimensionar, Desplazamientos, Isovalores, Reacciones, Empresillado, Esfuerzos, Sismo dinámico, Comprobar elementos, Comprobaciones E.L.U.; *Pandeo*: Grupos de análisis, Combinaciones (KL), Calcular, Modos de pandeo, Isovalores, Asignar coeficientes; *Modal*: Casos de carga (VB), Calcular, Modos de vibración, Listado; *Pushover*: Casos de carga (PO), Nudo de control, Calcular, Desplazamientos, Isovalores, Reacciones, Esfuerzos, Capacidad, Rótulas plásticas; *Incidencias*.

**Armaduras** – *Pilares*: Editar armaduras, Bloquear, Desbloquear; *Vigas*: Editar/Asignar armaduras, Bloquear, Desbloquear.

**Ventana** – Abrir Nueva, Renombrar, Cerrar, Anterior, Siguiente, Cascada, Mosaico horizontal, Mosaico vertical, Ordenar ventanas, Seleccionar ventana.

Barra de herramientas superior derecha (zoom, encuadre, órbita, selección, filtros, CAD, imán/referencias, vistas, ayuda).

## Paneles
- Izquierda: **Etiquetas** (tabla Referencia/Activa/Color/ocultar/bloquear, + − ↑ ↓), **Categorías** (Nudos, Barras, Tirantes, Pilares, Vigas, Paños, Láminas; ojo/candado), opciones "Representar la estructura usando colores de capas", "Elementos considerados en el análisis (Tensión/Deformación)", **Opciones de visualización** (información, textos, ejes locales, láminas con relleno, cargas con relleno, dimensión de nudos cm, tamaño de textos %, posición de tooltips).
- Centro: **Área de trabajo** con ventana **3D** (ejes X/Y/Z, líneas guía naranja, tooltip de coordenadas, barra vertical de herramientas de vista), ventanas MDI (cascada/mosaico).
- Pie: pestañas **Estructura / Cimentación**; barra de estado con mensaje de ayuda.

## Pendiente de inventariar (tarea T-RE-03, por subagente)
Cada botón/diálogo con sus campos: Datos generales, Biblioteca de perfiles, Pilares/Vigas/Barras (secciones), Paños, Hipótesis, Combinaciones, Uniones, Cimentación, listados, etc. Resultado en `docs/research/ui/*.md`.
