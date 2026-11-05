# Describir barras: verificación visual del clon frente al original (2026-11-05)

Método: el clon real (`build\msvc-release\bin\cypeclwon.exe`, obra de trabajo `docs/sample_project/describir_clone.ed3`, copia de la demo, nunca guardada) se manejó con `tools/drive_original.ps1` (Geometría > Barras > Sección, clic en la barra vertical, RMB) y se capturó la pantalla real (`sshot`/`fshot`) en la pantalla principal; se comparó con las capturas ya existentes `docs/screenshots/orig_describir_*.png` (no se lanzó el original: no hacía falta repetir capturas). Capturas del clon: `docs/screenshots/clon_describir_*.png` (laminado, info_ipe80, generica, generica_aviso_rango, generica_aviso_numero, generica_info, hormigon, conformado, madera, pilar, tirante, viga, perfiles_madera, material, tt_mat04). El script de captura usado está en `build/run_clone.ps1` (no versionado).

Las medidas se tomaron en píxeles sobre las capturas (diálogo 1051x890 con el panel «Perfiles utilizados»).

## Corregido

| # | Diferencia encontrada | Corrección |
|---|---|---|
| 1 | «Datos adicionales» iba anclado abajo (cabecera en y=565, mucho hueco) | Ahora va siempre a la misma altura que el original (y=392) para cualquier material; el área de perfil conserva su sitio aunque esté oculta (genérico, hormigón) |
| 2 | Banda «Selección del perfil» sobredimensionada al reservar altura | La sección de perfil empuja su contenido arriba (stretch al final) |
| 3 | Márgenes: panel izquierdo a 23 px del marco (original 14), primera banda 5 px más baja, iconos 7 px más bajos | Márgenes del contenido y del panel ajustados; baldosas de material de 71 px cada 78 px y de forma de 56 px cada 64 px como en el original; etiquetas/combos de «Serie de perfiles» / «Perfil» a la misma x (315) |
| 4 | Radio del tipo de elemento no se ponía en negrita al elegirlo | El tipo elegido va en negrita (Genérico, Pilar...) como en el original |
| 5 | Botones + / lápiz / tabla / (i) eran glifos de texto | Iconos dibujados (más negro fino, lápiz dorado, rejilla, disco azul con «i») de 20 px |
| 6 | Página Genérico: columnas de anchos iguales, campos de 84 px, filas cada 32 px, etiqueta «Coeficiente de dilatación» cortada, dibujo de la sección solo con el signo «?» | Columnas 206/252/resto, campos de 56/84/80 px, filas cada 29 px, títulos 18 px bajo la banda; dibujo con envolvente discontinua y cotas 1-4 |
| 7 | Diálogo (i) de propiedades: etiqueta «Radio de acuerdo entre ala y alma» cortada; cotas sin marcas | Dibujo de 212 px fijo y separación mínima (la tabla recupera ancho); líneas de cota con marcas de extremo y de extensión |
| 8 | Aviso de validación de ancho fijo 367x161 (original 336 o 447 de ancho según el texto, alto 160) | El ancho sigue al texto (mínimo 336); alto 160 (también lo usan los avisos de la biblioteca de perfiles) |
| 9 | (i) de «Características mecánicas»: 420 px (original 466 con el corte tras «principales») | 466x161 |
| 10 | Conformado: al elegir el icono se abría en «U conformado rigidizado» / serie C / CF-60x2.0; el original abre en **«U conformado simple», serie U, UF-60x3** (captura `orig_describir_info_conformado.png`) | `firstShapedSlot` prefiere `ColdFormedU`; test actualizado |
| 11 | Página Pilar: Material y Tipo en dos columnas, sin dibujo; campos lejos | Estructura del original: «Material» con 3 baldosas de 135x70 en tercios del ancho, «Tipo» con 2 baldosas en mitades, línea, dibujo con fondo amarillo y cotas X/Y y los campos a su derecha |
| 12 | Página Viga: baldosas de 66 px y sin dibujo | Baldosas de 71 px (paso 78), dibujo amarillo con cotas b/a y cuadro de campos a la derecha |
| 13 | Página Tirante: serie y perfil a la derecha de los iconos, sin botones | Serie y perfil debajo de los iconos (como en el original) con los cuatro botones, deshabilitados hasta que exista el módulo de tirantes |
| 14 | «Perfiles de madera»: 560x250, solo Aceptar centrado | 585x330 con Aceptar a la izquierda y Cancelar a la derecha (como `orig_describir_perfiles_madera.png`) |
| 15 | «Describir material»: alto base 120 | 108 (427x186 con un solo grupo de acero, medido) |

Se verificó sin cambios (coincide): barra de estado «Barras - Sección | - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados.», lista «Perfiles utilizados» con prefijo «[Barra]», orden/forma de los 8 iconos de material y 8 formas del acero laminado, 16 formas del conformado en 2 filas con la serie a su derecha, Disposición del laminado (9 radios) y del conformado (7), 4 formas de madera, combo Comportamiento, rangos y mensajes de validación («El valor de 'Ancho izquierda (1)' debe estar entre 0.1 y 10000.0.», «La cadena introducida no es un número.»), tooltips («Perfil de madera» ... verificado en pantalla real), diálogo «Describir material» (grupo, radios, Aceptar/Cancelar a izquierda/derecha), «Perfiles de madera» (texto de la clase 1, casilla de fuego), cotas y 13 filas del (i) de IPE 80.

## Pendiente

| # | Diferencia | Motivo / dependencia |
|---|---|---|
| P1 | **Iconos de material y de forma**: el original usa imágenes renderizadas en 3D (perfil en perspectiva, madera, hormigón...); el clon dibuja siluetas planas | Arte/recursos del original; no se copian. Sin dependencia de módulos |
| P2 | **(i) de laminados** muestra valores calculados por geometría (IPE 80: A 7.64, Iyy 80.14, Zyy 23.22, Zzz 5.82, It 0.70, Iw 118.74, peso 6.00) frente a los **tabulados** del original (7.60, 80.10, 23.20, 5.80, 0.67, 120.00, 5.97) | Datos: tablas de laminados de `perfil3.dll` (como ya se hizo con IPC/UF/CF/aluminio); candidato a ampliar TK-075 |
| P3 | Dibujo de la sección del (i) solo para dobles T; otras formas con glifo genérico; el dibujo del original lleva radio de acuerdo y cotas con flechas | Solo se capturó la doble T (TODO(captura)) |
| P4 | **Etiquetas de barra en el 3D** («IPE 80», «Ø100», «Gen. (19.6 kg/m)», material con el comando Material): el clon no las dibuja | Depende del módulo de visualización/textos sobre los elementos del viewport |
| P5 | **Tirante**: documento «CÁLCULO DE TIRANTES EN CYPE 3D» con su barra de visor, y Aceptar bloqueado; botones +/lápiz/tabla/(i) deshabilitados | Depende del módulo de tirantes (y de un visor de documento) |
| P6 | **Pilar y Viga**: pictogramas reales (fondo amarillo con armadura, naranja), variantes metálica/mixta del pilar, casilla «Viga bajo forjado» que se ve bajo los campos del tipo rectangular en `orig_describir_viga_tipo.png` (no estaba documentada; su significado no se ha investigado), rangos de los campos de viga | Módulos de pilares y vigas (TK-061); el significado de la casilla requiere investigación |
| P7 | Datos no medidos que se mantienen tal cual (TK-075): UF/CF aproximados, h de CF, coeficientes de cortante IPC, ν/α de madera, g, peso del hormigón, norma de madera, clases de madera por REA | Sin cambios; ver `describir_barras.md` §16 |
| P8 | Icono de información a la izquierda de la barra de estado (el original lo muestra siempre) | Elemento global de la ventana principal, no de Describir |
| P9 | Tamaño exacto de «Describir material» con varios grupos y del grupo de madera (el original solo se midió con un grupo de acero) | No medido |
| P10 | Mensaje de «Aviso» con 453 px de ancho para el texto del rango frente a 447 del original (diferencia de 6 px por la regla ancho = texto + 128) | Despreciable |

## Pruebas

601 tests en verde (`tools/build.ps1`). Test modificado: el icono de conformado abre en U/UF-60x3 (antes CF-60x2.0).
