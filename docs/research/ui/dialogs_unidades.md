# Unidades de acotación y sistema de unidades (T-P3-02, sesión 9)

Capturas: `u_units1` (diálogo), `u_c1`, `u_c2`, `u_c3` (listas de los combos), `u_open2` (editor con la pestaña Obra). Obtenidas abriendo `nuevo2.ed3` desde Últimos ficheros en el original y pulsando Obra > Unidades de acotación (330,97 con la pestaña Obra activa); el diálogo se cerró con Esc, sin cambiar nada. [verificado]

## Diálogo "Unidades de acotación" (~224x174, título rojo con ×)
- Tres filas con combo: **Dimensiones** = Centímetros, **Armaduras** = Centímetros, **Longitudes** = Metros (valores por defecto de `nuevo2`).
- Las tres listas ofrecen **Metros, Centímetros, Milímetros** (en ese orden).
- Pie: Aceptar / Cancelar.

## Conclusión sobre el sistema de unidades
- No existe un selector de sistema de unidades (SI/técnico/imperial): el programa trabaja siempre en kN, m y MPa, y muestra cada magnitud con su unidad fija en los diálogos: kN (fuerzas), kN/m (cargas lineales y rigideces), MPa (tensiones), cm² (áreas de perfil), mm (geometría de perfiles, diámetros), cm (recubrimientos, dimensiones de nudos), m (excentricidades, coordenadas: el tooltip del editor muestra `184.315, -184.353, 151.575 m`), grados y %.
- Lo único configurable es la unidad **de visualización** de longitudes en tres categorías (Dimensiones, Armaduras, Longitudes). El campo "Dimensiones de los nudos" del panel de opciones de visualización se muestra en cm (15.0 cm) con la configuración por defecto.
- Separador decimal: punto, siempre ("15.0", "100.0").

## Huecos
Qué campos exactos entran en cada categoría (se asume: Dimensiones = secciones, recubrimientos y tamaño de nudos; Armaduras = longitudes y separaciones de barras; Longitudes = barras, coordenadas y cotas del modelo); si el ajuste se guarda dentro de la obra o como preferencia global (se asume por obra); efecto en las cotas del dibujo 2D.

## Implementación del clon
- `core/units`: valores del modelo siempre en metros; `LengthUnit`, conversión, nombres/símbolos, `DimensioningUnits` (defaults del original) y formato con punto decimal.
- `UnitsDialog` (src/app): réplica del diálogo; el botón de la cinta se conecta con la pestaña Obra (ticket TK-019).
