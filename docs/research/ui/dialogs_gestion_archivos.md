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

## Huecos
Diálogo de Más..., función de cada botón de la barra (Abrir/Nuevo/Copiar/Borrar/Buscar/Comprimir/Descomprimir/Compartir), menú contextual, tamaño mínimo, persistencia de la última carpeta.
