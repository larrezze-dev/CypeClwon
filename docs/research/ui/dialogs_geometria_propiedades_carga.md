# Geometría (Nudos/Barras), Propiedades > Vinculación exterior, Carga (T-RE-03)

[verificado] Obra de prueba: `docs/sample_project/dlg_obra.ed3` (portico de 4 nudos y 3 barras IPE 80, empotramientos en N1 y N4, carga uniforme en N2-N3). Capturas `orig_nudos_*`, `orig_barra*`, `orig_barras_*`, `orig_vinculacion_exterior_*`, `orig_carga_*`, `orig_*_ribbon.png`, `orig_editor_01.png`.

## Modelo de interacción común (importante para el clon)
- Cada comando de ribbon activa un **modo** (el botón queda resaltado) y muestra una ventanita de ayuda sin título (texto + "Pulse 'F1' alternativamente para mostrar u ocultar los textos de ayuda").
- Barra de estado: "Nudos - Nuevo | - LMB: Introduzca un punto." / "Barras - Sección | - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados." Los comandos de edición: **LMB selecciona, RMB abre el diálogo** sobre la selección.
- Información al pasar el ratón por un elemento (panel superior derecha): "Pieza (N2/N3): L. 4 m / Capa : por defecto / IPE 80 (S275 (UNE-EN 10025-2))"; tooltip de coordenadas "x, y, z m".
- Escribir un dígito en modo Nuevo nudo abre la **Calculadora**.

## Geometría > Nudos > Nuevo
- Ribbon Geometría: grupos Planos (Planos), Nudos (Nuevo, Borrar, Mover), Barras (Barra, Pieza, Pilar, Viga, Borrar, Sección, Material, Disposición, Mover extremo, Invertir eje 'x', Agrupar, Desagrupar, Información, ...), Láminas, Herramientas, Importar (DXF o DWG, Fichero de texto) (`orig_geometria_ribbon`).
- **Calculadora** (323x286, `orig_nudos_nuevo_02_calculadora`): campos **Coordenada X / Y / Z** (defecto 0; se escribe sobre el campo activo, el primer dígito que se teclea en la vista abre el diálogo y entra en X), teclado numérico 0-9, `.`, `=`, `+ - * /`, `+/-`, `???`, `C`, `AC`, `MC M- M+ MR`, `←` y botón grande Enter. **Enter avanza X → Y → Z y en Z crea el nudo** (verificado: "0, 0, 0" crea N1). Nudos nombrados N1, N2... en verde.
- Valores usados: N1 (0,0,0), N2 (0,0,3), N3 (4,0,3), N4 (4,0,0).

## Geometría > Barras > Barra
- Al activar aparece el panel flotante **Propiedades - Nueva barra** (278x370, `orig_barra_nueva_01_propiedades`; sin Aceptar, se queda abierto mientras se dibuja): botón de perfil "[Barra] IPE 80" (con icono de edición) que abre *Describir*; campo **Ángulo** 0.000 grados; cuadrícula 3x3 de puntos de referencia (anclaje) con el punto central marcado; tabla "Perfiles utilizados" (vacía).
- Se crea la barra con dos clics sobre nudos. Defecto IPE 80 (S275 (UNE-EN 10025-2)).
- Barras > Sección: abre *Describir* (ver `dialogs_biblioteca_perfiles.md`). Barras > Disposición: **Describir disposición** (713x597, `orig_barras_disposicion*`): casilla "Considerar las excentricidades en el cálculo" (marcada); **Ángulo de giro** (tres botones: perfil vertical, horizontal, "α = ?"); **Desplazamientos en ejes locales** radios "Iguales en los dos extremos" (defecto) / "Diferentes en cada extremo", campos Y 0 mm, Z 0 mm; **Desplazamientos en ejes globales** mismo esquema con X, Y, Z 0 mm; vista "Posición" con la sección y 9 puntos de anclaje, barra de herramientas de vista; Aceptar / Cancelar; ayuda (libro) en título.

## Propiedades > Vinculación exterior
Ribbon Propiedades (`orig_propiedades_ribbon`): Diafragmas (Editar, Diafragma, Generar por niveles), Nudos (Vinculación interior, Vinculación exterior, Ligaduras), Barras (Pandeo, Pandeo lateral, Parejas, Articular, Empotramiento, Rótulas plásticas, Grupos de flecha, Resistencia al fuego, Análisis modal), Láminas (deshabilitado).
- Modo selección de nudos (LMB) + RMB → **Vinculación exterior** (591x414, `orig_vinculacion_exterior_01`): rejilla de 20 iconos (2 filas x 10): primero "definida por el usuario" (? azul), después tipos predefinidos (empotramiento, apoyos deslizantes X/Y/XY, articulaciones...). El primer icono de la segunda fila aparece seleccionado inicialmente (vinculación "ninguna/libre").
- Icono usuario (`_02_usuario`): grupo **Desplazamientos** con radios *Independientes* (defecto) / *Sobre una recta* / *Sobre un plano*; Dx, Dy, Dz con radios **Libre** (defecto) / Fijo / Elástico; grupo **Giros** Gx, Gy, Gz con Libre / Fijo / Elástico.
- Empotramiento (`_03_empotrado`): Dx..Gz = Fijo (opciones Fijo / Elástico).
- Elástico (`_04_elastico`): aparece **K = 100000 kN/m** y **Sentido** combo (defecto "Ambos").
- **Validación**: si K coincide con el valor por defecto al aceptar, diálogo *Pregunta* (668x175, `_05_pregunta`): "El valor introducido para la constante de apoyo elástico (K) coincide con el valor por defecto. Recuerde que debe asignar un valor conforme al grado de empotramiento que posee la vinculación exterior en el nudo. ¿Desea continuar?" Botones Sí / No.
- Aceptar aplica y dibuja los símbolos de empotramiento (hachurado azul bajo el nudo).

## Carga
Ribbon Carga (`orig_carga_ribbon`): Visualización (Hipótesis vista, Escalas), Nudos (Desplazamientos prescritos, Nueva, Editar, Hipótesis, Borrar), Barras (Nueva, Editar, Hipótesis, Borrar, Peso propio), Láminas, Paños (Nuevo, Editar, Mover, Dividir, Borrar, Dirección de reparto), Cargas superficiales (Nueva, Editar, Hipótesis, Borrar, Copiar, Mover, Asignar).
- **Hipótesis vista** (185x145, `orig_carga_hipotesis_vista`): combo **Hipótesis** (defecto "Peso propio"), casilla "Ver todas", Aceptar / Cancelar.
- **Alta de hipótesis**: Obra > Datos generales > Hipótesis adicionales > botón de la categoría (p. ej. Cargas muertas) → ventana *Cargas muertas* (304x281; enlace "+ Nueva hipótesis adicional", tabla Actúa | Hipótesis adicionales | Editar | Borrar) → **Nueva hipótesis adicional** (415x189, `orig_carga_hipotesis_nueva`): **Referencia** (defecto `CM 1`, preseleccionado), **Descripción**, casilla "Con distintas disposiciones de cargas"; **Validación**: Referencia en blanco → Aviso "'Referencia' no puede quedar en blanco." Tras crear "CM 1" el contador Adicionales de Cargas muertas pasa de 0 a 1.
- **Carga > Barras > Nueva** (selección LMB de barras, RMB) → **Introducir cargas sobre barras** (404x516, `orig_carga_barras_nueva_01`, `_tipo01.._tipo09`):
  - Combo **Hipótesis**: sólo "Peso propio" al inicio; tras crear CM 1 aparece "CM 1".
  - **Tipo de carga** (9 botones, el 2.º preseleccionado = uniforme): 1 puntual (Valor 9.81 kN; Posición relativa marcada, Distancia 0.000 (0..1)); 2 uniforme completa (Valor 9.810 kN/m); 3 uniforme parcial (Valor 9.810 kN/m; Posición inicial 0.000 / final 1.000 (0..1)); 4 trapezoidal ascendente "triangular" (Valor inicial 9.810 kN/m, Pos. ini/fin); 5 (Valor final 9.810 kN/m; Pos. ini/fin); 6 trapezoidal (Valor inicial y final 9.810 kN/m; Pos. ini/fin); 7 momento puntual (Valor 9.810 kN·m, Distancia 0.000); 8 temperatura uniforme (Valor 10.00 °C); 9 gradiente de temperatura (Valor 1 y Valor 2 = 10.00 °C; ejes locales fijos; 3 botones de dirección de variación T1/T2).
  - **Ejes sobre los que se define la dirección de la carga**: radios Ejes globales (defecto) / Ejes locales de la barra.
  - **Dirección y sentido de aplicación**: 7 botones (X, proyección sobre plano, Z+, -X, ..., Z- [defecto, gravedad], dirección perpendicular a barra con signo de cargas).
  - Aceptar / Cancelar. (Los nombres de los tipos de carga y direcciones son interpretaciones de los iconos; los tooltips no se capturaron.)
  - **Validación**: Distancia fuera de 0..1 → Aviso (399x162) "El valor de 'Distancia' debe estar entre 0.000  y 1.000 ." (`orig_carga_barras_valid_distancia`).
- Peso propio se representa automáticamente (valor 0.059 mostrado sobre IPE 80 en la vista).
- Para editar cargas: Barras > Editar / Hipótesis (sólo seleccionan barras con carga de usuario; con ninguna cargada la selección no abre diálogo).

## Cierre de obra
Alt+F4 con obra abierta muestra *Pregunta* (266x162) "¿Desea grabar la obra?" con Sí / No / Cancelar (`orig_salir_pregunta`). Crear obra deja además un fichero de copia `dlg_obra.ed$` junto a `dlg_obra.ed3`.

## Huecos
Geometría > Pilar/Viga/Planos/Mover/Borrar/Información/Agrupar; Nudos > Mover/Borrar; Carga nudos y superficiales, Escalas, Peso propio; Vinculación interior, Ligaduras; tabulación; los otros 18 iconos de vinculación exterior (efecto exacto de cada uno); diálogo de edición del campo Ángulo/Anclaje de la barra nueva; segundo clic extra en selección (a veces hay que repetir LMB+RMB: la primera pulsación sólo activa el foco en la ventana 3D).
