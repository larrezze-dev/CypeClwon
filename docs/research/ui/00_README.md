# T-RE-03 – Inventario del ribbon de CYPE 3D 2027 (estado: PARCIAL)

## Método y niveles de evidencia
1. **[captura]** Etiquetas y estado habilitado/deshabilitado de cada botón, leídos de `docs/screenshots/07_ribbon_tabs.png` (obra vacía). Verificado visualmente.
2. **[cadena]** Texto en español (y traducciones) extraído estáticamente de `ed3dlib.dll` y `e3dcomun.dll`: tablas UTF-16LE con formato `<id>texto\0`, repetidas una vez por idioma (orden típico es, fr, pt, ca, it, en, gl, ro, de, pl, sr, tr; más bloques ru/zh/bg/ja/el). Prueba que la cadena existe en el producto (tooltips, mensajes de validación, etiquetas de diálogos), **no** prueba qué diálogo/campo la usa ni su valor por defecto. Volcados: `docs/research/ui/data/strings_ed3dlib.tsv` y `strings_e3dcomun.tsv` (id, es, 6.ª cadena ≈ en; algunas filas empiezan en otro idioma). El id se cita como `#id`.
3. **[web]** Páginas de info.cype.com (ver `03_features_web.md` y `04_web_pages_extra.md`).
4. **[no verificado]** Valores por defecto, rangos y orden de campos de diálogos: requieren abrir cada diálogo.

## Por qué no hay capturas de diálogos
- Las herramientas REA `capture_native_ui_scenario/observe_native_ui` están descritas para Mach-O (macOS); no son aplicables a un PE de Windows. `inspect_pe_resources` devuelve 0 recursos (la UI es propia, sin RT_DIALOG/RT_MENU): la UI no es inspeccionable por recursos Win32.
- Se intentó automatizar la GUI con capturas de pantalla + clics sintéticos, pero en el momento de ejecución el escritorio del usuario tenía otra aplicación en primer plano con texto en edición; los clics/teclas sintéticos habrían caído en esa aplicación. Se abortó, se cerró el proceso lanzado y se borró la captura (contenía datos ajenos al proyecto). **Pendiente**: repetir con el escritorio dedicado o en una sesión de VM/escritorio aparte.
- La app ejecutable no incluye el ribbon como datos legibles (estructura de pestañas/grupos construida en código de `ed3dlib.dll`/`m_ed3dexe.dll`); la jerarquía pestaña>grupo>botón procede de la captura 07.

## Hallazgos técnicos útiles
- Español de España está embebido en las DLL de lógica (no hay `ejd3des.dll`); `ejd3d{ale,bra,bul,fra,ing,ita,por}.dll` son recursos adicionales (sin tablas de recursos PE; contenido no analizado). El idioma de salida por defecto del clon (es-ES) puede tomar de aquí el glosario terminológico (volcados TSV).
- Idiomas presentes en tablas de `ed3dlib.dll`: es, fr, pt, ca, it, en, gl, ro, de, pl, sr, tr, ru, bg, zh, ja, el (verificado en id 139006 y 315578). **Holandés: no apareció** en las muestras comprobadas.
- Un diálogo tiene cadenas con ids correlativos. Rangos observados en `ed3dlib.dll`: 25xxx–62xxx etiquetas genéricas; 63959–65400 mensajes, tooltips y ayudas de comandos; 372862–402000 funciones recientes (no lineal, pandeo, modal, pushover, etiquetas, numeración).

## Archivos
- `obra.md`, `geometria.md`, `propiedades.md`, `carga.md` (prioritarios), `uniones_calculo_armaduras_ventana.md` (índice de comandos + mensajes).

## Huecos
Capturas y campos reales de TODOS los diálogos; valores por defecto; barra de herramientas superior derecha; solapa Cimentación (ribbon); menú Archivo; diálogos de Datos generales por pestaña (ver wizard en `01_reverse_engineering_findings.md`).
