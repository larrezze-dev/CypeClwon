# Gestión archivos y Últimos ficheros (T-RE-04)

Capturas: `orig_inicio_check` (pantalla de inicio con 4 recientes), `orig_gestion_archivos_01`, `orig_gestion_archivos_after_esc`. [verificado]

## Últimos ficheros (pantalla de inicio)
- Seis ranuras con el formato `N <ruta completa>`; las vacías dicen `N Sin fichero` en gris.
- **Un fichero que ya no existe se conserva en la lista y se muestra en gris** (en la captura, 2 `ref.ed3` y 3 `nuevo.ed3`); los existentes se muestran en negro. No se eliminan.
- El botón **Más...** aparece deshabilitado con 4 entradas (se habilita presumiblemente con más de 6; capacidad real no verificada).
- Esc cierra Gestión archivos y vuelve a la pantalla de inicio.

## Diálogo "Gestión archivos" (676x525, título rojo con minimizar/maximizar/cerrar)
- Barra de herramientas con iconos grandes y texto: **Abrir, Nuevo, Copiar, Borrar, Buscar** | **Comprimir, Descomprimir** | **Compartir** | **Ejemplos**.
- Panel izquierdo: combo de ruta (con ruta recortada) + botón de favoritos (estrella azul) y árbol de carpetas: Mi PC > Acceso rápido, Escritorio, Mis documentos, C:, D: ..., Entorno de red. Carpeta actual seleccionada (`D:\CYPE Ingenieros\Proyectos\CYPE 3D`).
- Panel derecho: lista con columnas **Obra | Descripción | Versión | Fecha** (ej. `nuevo2`, `2027.a`, `26/10/2026`), fila resaltada en azul claro.
- Separador arrastrable entre paneles.

## Botones de la barra (sesión 8, capturas `orig_ga_*`) [verificado]
Con la obra `nuevo2` seleccionada y cancelando cada diálogo (no se modificó ningún fichero):
- **Copiar** -> diálogo común de Windows "Copiar obra" (Guardar/Cancelar), nombre propuesto `nuevo2.ed3`, tipo "Obra CYPE 3D (*.ed3)", carpeta inicial = última usada para copiar. Tooltip: "Copiar / Copiar obra". Tras Esc, Gestión archivos también se cierra al volver (el script posterior vio la pantalla de inicio).
- **Borrar** -> "Pregunta": "¿Confirma que quiere enviar esta obra a la Papelera de reciclaje?", casilla marcada "Enviar copias de seguridad", botones Sí / No (547x~190 como los diálogos CYPE).
- **Comprimir** -> diálogo "Comprimir" (547x190): ruta `D:\CYPE Ingenieros\Proyectos\CYPE 3D
uevo2.cyp` (junto a la obra, extensión .cyp) con botón de examinar, casilla "Incluir copias de seguridad" (desmarcada), "Total bytes a enviar: 804.82 KBytes" (cifra en rojo), Aceptar / Cancelar.
- **Descomprimir** -> diálogo común "Selección de Obra Comprimida", tipo "Obras comprimidas (*.cyp)", botón Abrir; carpeta inicial = última usada.
- **Buscar** -> no muestra nada visible en la captura (a 1,5 s); comportamiento sin determinar.
- **Más...** -> deshabilitado (4 entradas): diálogo no capturado; implementado de forma provisional.

## Huecos
Buscar, Compartir, menú contextual, tamaño mínimo, persistencia de la última carpeta, formato real del .cyp (el clon usa un ZIP), nombre de las copias de seguridad (provisional `<obra>.bak`), diálogo real de Más..., tamaño exacto del diálogo de Borrar.
