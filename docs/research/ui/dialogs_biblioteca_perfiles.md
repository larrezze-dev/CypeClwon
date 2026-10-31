# Biblioteca de perfiles y diálogo "Describir" (T-RE-03)

[verificado] Capturas `orig_biblioteca_perfiles_*`, `orig_describir_*`, `orig_barra_nueva_02_describir`, `orig_barras_seccion_describir`.

## 1. Obra > Biblioteca de perfiles: "Gestión de la biblioteca de perfiles" (328x224)
Cuatro botones: *Importación de series de perfiles predefinidas*, *Importar catálogos de fabricante*, *Importación inicial de series predefinidas de perfiles*, *Exportar series de perfiles*; Aceptar / Cancelar.
- Importación de series predefinidas (312x555, `_02`): tabla Importar (casilla) | Nombre Biblioteca | Editar. Filas (todas sin marcar): Aluminio, Gama de perfiles europeos, Canada, Gost, Instituto argentino de Siderurgia, Imca, Indian standard, Aisc.Lrfd (mks), Aisc.Lrfd (us), Madera, Nbe-ea95, SAISC, Tabelas Técnicas, TecnoMetal (esp), TecnoMetal (ita), TecnoMetal (bra), Ukprofiles, Korean Standard, Japanese Sections, NHBC UK (Timber), SYS Thai Standard, ГОСТ, Australia.
- Catálogos (430x498, `_03`): botón "Filtros aplicados" ("No se ha aplicado ningún filtro."), casilla "Buscador de productos", tabla Fabricante | Descargar | Actualizar | Borrar | Autoría (Gerdau, ArcelorMittal, Acindar, Açominas, Condesa, Colmena, Cintac, Açotubo, Cimag, Gerdau Corsa, Ahmsa, Sadef, Mundiperfil, Tata Steel ...); enlaces "¿Qué son estos catálogos?" y "¿Le falta algún catálogo?"; botón Terminar. (No se pulsó Descargar: requiere red.)

## 2. Diálogo "Describir" (selección de sección; 859x890)
Se abre desde el panel flotante "Propiedades - Nueva barra" (botón con el perfil actual, p. ej. "[Barra] IPE 80") y desde Geometría > Barras > Sección (en este caso 1051x890, con panel izquierdo "Perfiles utilizados" con la lista de perfiles de la obra). Botones Aceptar / Cancelar.
- **Tipo de elemento estructural**: radios Genérico (defecto) / Tirante / Pilar / Viga. **Comportamiento**: combo Lineal (defecto), Sólo tracción, Sólo compresión.
- **Selección del material** (8 iconos, tooltips no capturados): 1 acero laminado (defecto), 2 segunda familia de acero con series IPC (laminado/armado, nombre no verificado), 3 acero conformado, 4 madera, 5 serie "(EP) I" (probablemente aluminio extruido; nombre no verificado), 6 secciones especiales de aluminio, 7 hormigón, 8 genérica (valores).
- **Selección del perfil**: iconos de forma según material. Acero laminado: I, U, T, L, L desigual, plano, redondo, cuadrado (8). Familia 2: 11 formas. Conformado: 16 formas en dos filas (la serie y el perfil aparecen a la derecha). Madera: Redondo, Cuadrado, Rectangular x2 (4). Aluminio: 10 formas.
- **Serie de perfiles** (combo; defecto IPE) + botones **+** (nueva serie), **lápiz** (editar), **tabla** (abrir gestor); **Perfil** (combo, defecto IPE 80) + icono (i) de información. Lista de series (primeras): IPE, IPE A, IPE AA, IPE 750, IPE O, IPE V, HEA, HEB, HEC, HEM, HEAA, HE, HL, HLZ, HD ...
- **Datos adicionales** > **Disposición** (acero laminado I): Perfil simple (defecto), Simple con cartelas, Con losa de hormigón, Doble en cajón soldado, Doble en cajón con presillas, Doble en cajón unión genérica, Medio perfil, Con platabandas laterales, Boyd (alma aligerada). Familia 2: Perfil simple / Con losa de hormigón + casilla "Hay rigidizadores de alma" (Separación entre rigidizadores 1000.0 mm, Espesor 5.0 mm, deshabilitados hasta marcar). Conformado: Perfil simple, Doble en cajón soldado/presillas/unión genérica, Doble en I unión soldada/presillas/unión genérica. Madera: sólo Perfil simple.
- Hormigón: **Tipo de sección** Circular / Rectangular (defecto) / Rectangular de canto variable; **Dimensiones** Ancho 30.0 cm, Canto 30.0 cm.
- Genérica: Geometría (Ancho izquierda 25.0, derecha 25.0, Canto inferior 25.0, superior 25.0 mm), Características mecánicas (A 25.00 cm2, Avy 20.83, Avz 20.83, Iyy 52.08 cm4, Izz 52.08, It 87.50), Material (E 206010.00 MPa, Poisson 0.300, dilatación 0.000012 m/m°C, peso 77.01 kN/m3).
- Aluminio especial: combo "Secciones especiales de aluminio" + barra de herramientas y área gráfica.

## 3. "Series de Perfiles" (gestor; 530x520, desde el botón tabla)
Barra de herramientas (+ nueva, lápiz editar, duplicar, borrar, subir, bajar, y 4 botones de carpeta/importar-exportar) y tabla Referencia | Catálogo | Descripción (IPE ... "Perfiles I de alas paralelas (EN 10365: 2017)", HE* "Perfiles H de alas anchas", HL, HLZ "muy anchas", HD/HP/UBP pilares/pilotes, UB "Vigas universales", UC "Columnas universales"...). Aceptar / Cancelar.
- **Nueva serie** (330x421): Referencia, Descripción (multilínea), mini-tabla Perfiles con + / editar / duplicar / borrar / subir / bajar, botones laterales de importación. **Validación**: Aceptar con serie vacía → Aviso "Debe introducir, al menos, un perfil en la serie." (`orig_describir_series_nueva_valid`).

## 4. Tras Aceptar en Describir: "Perfiles de acero" (464x299)
Mismo contenido que *Resistencia al fuego* (radios: No considerar la situación de incendio [defecto], Comprobar la resistencia al fuego (Código Estructural), Calcular la temperatura crítica...). Aceptar/Cancelar.

## Huecos
Tooltips y nombres exactos de los 8 materiales; iconos de forma de la familia 2 y aluminio; botón (i); diálogos de editar serie/perfil; orden de tabulación.
