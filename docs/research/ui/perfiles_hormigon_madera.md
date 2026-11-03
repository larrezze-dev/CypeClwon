# Secciones de hormigón y madera (T-P5-06, núcleo)

Etiquetas: [UI] = observado en capturas (`dialogs_biblioteca_perfiles.md` §2); [cadena] = texto UTF-16 con id (`data/strings_ed3dlib.tsv`); [código] = pseudocódigo REA de `seccion.dll` (consulta de esta tarea, solo análisis estático, sin lanzar aplicaciones); [clon] = decisión propia no verificada.

## 1. Formas [UI] [cadena]
- **Hormigón** («Tipo de sección», #27957): Circular / Rectangular (defecto) / Rectangular de canto variable (#65245). Dimensiones por defecto Ancho 30,0 cm y Canto 30,0 cm.
- **Madera** (4 iconos): Redondo, Cuadrado, Rectangular y Rectangular de canto variable (la 2.ª rectangular es la de canto variable: mismo id #65245; `recmad`/`recvmad` en perfil3).
- Dimensiones del rectángulo: #65243 (paralela al eje local Y, «ancho»), #65244 (paralela al eje local Z, «canto»); canto variable: #65246 «dimensión paralela a Z en el extremo inicial de la pieza» (#131436 «Canto inicial») y #65247 «en el extremo final» (#131437 «Canto final»).
- Clon: `ShapeKind::{ConcreteCircular {d}, ConcreteRectangular {b,h}, ConcreteRectangularVariable {b,h0,h1}, TimberRound {d}, TimberSquare {a}, TimberRectangular {b,h}, TimberRectangularVariable {b,h0,h1}}`; familias `SectionFamily::Concrete` («Hormigón», #27639) y `Timber`. Metros SI.

## 2. Fórmulas verificadas en el código [código]
- `paramsec_area_cortante_rectangulo` = **5·A/6**; `paramsec_area_cortante_redondo` = **0,9·A** (una sola función, sin distinción de dirección: Avy = Avz). Resuelve la parte de macizas de TK-055.
- `paramsec_modulo_plastico_y_rectangulo(canto, ancho)` = **canto²·ancho/4** (error si canto o ancho ≤ 1e-5).
- `paramsec_inercia_torsion_redondo` = 2 × inercia a flexión del redondo (= π d⁴/32).
- `paramsec_inercia_torsion_rectangulo(a, b)` ordena (lado largo l, corto s) y llama a `FUN_18001ef90`: **It = β·l·s³**, con β lineal a trozos en r = l/s por los nodos 1→0,140; 1,25→0,171; 1,5→0,196; 2→0,229; 3→0,263; 4→0,281; 6→0,299; 10→0,333 y constante 0,333 para r > 10. Comprobaciones (`error_interno`): l > 1e-5, s > 1e-5, l > s − 1e-4. El clon la reproduce tal cual (no usa la serie de Saint-Venant).
- Coherente con la tarjeta del genérico del original (cuadrado de 50 mm, [UI]): A 25,00 cm², Avy 20,83 (5/6), Iyy 52,08 (b h³/12), It 87,50 cm⁴ (= 0,14·0,05⁴).
- A, Iy = b h³/12, Iz = h b³/12, Wel = I / fibra extrema, círculo A = π d²/4, I = π d⁴/64, Wpl = d³/6: fórmulas de libro (el original no tiene campo propio de Wel, ver `perfiles_biblioteca.md` §2.2). Iw = 0 en todas.
- Convención de ejes: Iy respecto al eje y (horizontal), canto h a lo largo de z.

## 3. Validación y modelo [clon salvo lo indicado]
- Todas las dimensiones > 1e-5 m (límite que el original comprueba en sus rutinas); mensaje «Las dimensiones del perfil deben ser mayores que cero.» (texto propio; el doc no da #id para hormigón/madera). Número de dimensiones incorrecto, NaN/inf → mensajes ya existentes.
- Hormigón y madera **no admiten valores tabulados** (solo cálculo).
- Una serie de forma de hormigón/madera debe declarar la familia correspondiente.
- Canto variable: `sectionAt(s, t)` interpola el canto **linealmente** entre h0 (t = 0) y h1 (t = 1) y devuelve la sección rectangular equivalente; `propertiesAt(s, t)` da sus propiedades. `effectiveProperties/calculatedProperties` de una sección variable devuelven las del extremo inicial. La linealidad es una suposición (TK-062).
- Defecto: `defaultConcreteSection()` = Rectangular 0,30 × 0,30 m, referencia «30x30» [clon: el original no muestra referencia].
- Serialización: la biblioteca de obra (`profile_library`) ya serializa cualquier forma por `shapeKey` y `familyKey`; los deshacer son los de la biblioteca (`ReplaceLibraryCommand`). Tests: `tests/concrete_timber_tests.cpp`.

## 4. Huecos → tickets
TK-061 (GUI, dimensiones por defecto del resto de formas, tooltips), TK-062 (ley del canto variable en el cálculo), TK-063 (materiales hormigón/madera por defecto: E, G, ν, α, peso; no deducidos), TK-064 (verificación en la app de los valores mostrados y de Av de madera según norma).
