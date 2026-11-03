# Datos y fórmulas de la biblioteca de perfiles (T-P5-05, núcleo)

Complementa `perfiles_biblioteca.md` (pasada REA). Aquí: de dónde salen los datos del clon, cómo se calculan las propiedades y qué precisión se midió frente a tablas publicadas. No se ha lanzado ninguna aplicación.

## 1. Fuente de datos (libre, no CYPE)

- **Catálogo «Sections and Merchant Bars»** de ArcelorMittal (Long Carbon Europe, PDF público, ~244 páginas), obtenido de `https://home.agh.edu.pl/~blajer/wpages/cad/tablice.pdf` (alojado en AGH). Contiene dimensiones nominales y propiedades de: IPE (dimensiones «EU 19-57», tolerancias EN 10034), HE A/B/M («EU 53-62»; ambas normas fueron absorbidas por EN 10365), IPN (DIN 1025-1), UPN (DIN 1026-1), ángulos de lados iguales (EN 10056-1).
- Las **dimensiones nominales** (h, b, tw, tf, r…) son hechos normalizados; se guardan en `src/core/profile_catalogue_data.inc` (generado por `tools/gen_profile_data.py`, que extrae el texto con `pdftotext -layout`). Las **propiedades se calculan** en el clon, nunca se copian al producto. Los valores publicados (A, Iy, Iz, Wel, Wpl, It, Iw, Avz) viven solo en `tests/profile_reference_data.inc` como referencia de pruebas.
- El generador une cada fila de dimensiones con su fila de propiedades por masa lineal (G ≈ 0,785·A) y por el canto nominal del nombre; aborta si una unión es ambigua. Las filas de referencia de ángulos que no cumplen `i²·A ≈ Iy` (texto del PDF desalineado) se descartan (52 de 103 quedan como referencia).
- No incluidas (sin fuente fiable verificada o fuera de alcance): series AA/A/O de IPE y HE, HL/HD/HP, UPE/UE, ángulos de lados desiguales (hay datos en el mismo PDF; falta parsearlos), T, plano/redondo/cuadrado (EN 10058/10060), tubulares (EN 10210/10219), conformados, aluminio, madera. Ticket TK-052.
- Resultado: 7 series — IPE 18, HEA 24, HEB 24, HEM 24, IPN 19, UPN 18, L (iguales) 103 perfiles. Las descripciones («Perfiles IPE (alas paralelas, EN 10365)», …) son propias, no literales del original.

## 2. Cálculo de propiedades (`src/core/section_properties.cpp`)

Convención: metros, ejes locales y horizontal (ancho) y z vertical (canto); Iy = inercia respecto a y (eje fuerte del I). Regla del original «tabulado si existe, si no calculado» (`SectionOverrides`); Wel = I / fibra extrema (el original no tiene campo propio, confirmado en la pasada REA).

- **A, Iy, Iz, centro de gravedad, fibras extremas**: integración exacta del contorno poligonal (fórmula de Green). Las esquinas redondeadas (acuerdo ala-alma r, radio de punta r2) son arcos tangentes discretizados con 32 cuerdas (error ≈ 1e-4 de su propia aportación). Alas inclinadas: espesor `tf` dado a b/4 de la punta (IPN, el original usa `tf − (b/4)·pendiente`) y a b/2 de la punta (UPN), creciendo hacia el alma con la pendiente; pendientes **IPN 14 %, UPN 8 %** (ajustadas y verificadas contra A, Iy, Iz publicados; no vienen del original).
- **Wpl**: eje neutro que divide el área en dos mitades iguales (bisección + recorte del polígono); Wpl = Σ A_i·d_i.
- **It**: doble T paralela con acuerdo: `2/3(b−0,63tf)tf³ + 1/3(h−2tf)tw³ + 2·α·D⁴` con `α=(tw/tf)(0,1r/tf+0,145)`, `D=((r+tw/2)²+(r+tf)²−r²)/(2r+tf)` (misma expresión que `inercia_torsion_doblete_con_radio_acuerdo` en seccion.dll, pasada REA; sin radio: suma de rectángulos). IPN: la misma fórmula con tf. U: suma de rectángulos (cota inferior). T: suma de rectángulos + un término de unión. L: `(h+b−t)t³/3`. Macizas: rectángulo de Saint-Venant `b t³(1/3 − 0,21 t/b (1 − t⁴/(12 b⁴)))`, redondo `πd⁴/32`.
- **Iw**: doble T `Iz(h−tf)²/4`; U fórmula de pared delgada de canal; T y L de pared delgada (`Σ t³l³/36`); plano `b³t³/144`; redondo y cuadrado 0.
- **Cortante (Avy, Avz)**: laminados EN 1993-1-1 6.2.6(3): I y U `A − 2b·tf + (tw + 2r)·tf` (U: `tw + r`), T `0,9(A − b·tf)`; en la otra dirección «A menos el alma paralela a la carga». Macizas: redondo 0,9·A, rectángulo/cuadrado 5/6·A (áreas de cortante de Timoshenko, **sin verificar frente al original**, TK-055).
- **Genérico**: A, Iy, Iz (y opcionalmente It, Iw, Wpl, Avy, Avz) obligatoriamente tabulados; cuatro distancias al contorno envolvente (ancho izq./der., canto inf./sup., #65234-#65237); Wel = I / distancia mayor; Avy, Avz ≤ A (#65239).

## 3. Precisión medida frente a la tabla publicada (tests `section_properties_tests.cpp`)

| Serie | A | Iy | Iz | Wel,y | Wpl,y | It | Iw | Avz |
|---|---|---|---|---|---|---|---|---|
| IPE / HEA / HEB / HEM (≈ 90 perfiles) | < 1,2 % | < 1,2 % | < 2 % | < 1,2 % | < 1,5 % | < 5 % | < 5 % | < 3 % |
| IPN | < 1,5 % | < 2 % | < 3 % | — | — | < 20 % | < 25 % | < 8 % |
| UPN 50..300 | < 1,5 % | < 2 % | < 3 % | — | — | < 20 % | < 25 % | < 8 % |
| UPN 320..400 | tolerancia ×3 (pendiente de las alas distinta de 8 %, no identificada; TK-052) |
| L iguales (52 filas de referencia) | < 1,2 % | < 2 % | = Iy | Wel < 2 % | | | | |

Con IPE 200 el It calculado (6,98 cm⁴) coincide con el publicado. It de IPN y U subestima ~9 %.

## 4. Persistencia y operaciones

- Sección `profile_library` (JSON, título «Biblioteca de perfiles») en el `.ed3` del clon; `Project::profileLibrary` vacío en una obra nueva (la importación de predefinidas la hará la GUI).
- Operaciones deshacibles (nueva/editar/duplicar/borrar/subir/bajar serie y perfil, importar series con «conservar»/«sustituir», #33823/#33824) mediante un único `ReplaceLibraryCommand` que guarda la biblioteca anterior; validaciones con las cadenas originales (#131925, #131965, #131966, #131926, #400894, #65417, #33818).
- Export/import de series: JSON propio `{"format":"cype-clone-profile-series","version":1,"series":[...]}`. El original no revela la extensión ni el formato (TK-054).
