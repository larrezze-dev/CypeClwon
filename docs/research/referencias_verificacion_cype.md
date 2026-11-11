# Referencias publicadas de CYPE para validar el solver lineal (T-P8-05)

Fecha: 2026-11-12. Solo lectura y web; no se lanzó CYPE 3D ni el clon. Marcas: **[V]** documentado con fuente citada; **[N]** no verificado / no encontrado.

## 0. Resultado resumido

- **No existe ningún ejemplo resuelto oficial de CYPE 3D con valores numéricos completos** (geometría + cargas + desplazamientos/reacciones/esfuerzos) en info.cype.com, learning.cype.com, el manual de usuario ni en la carpeta de instalación. Conclusión [V] por ausencia en las fuentes de la sección 1, no prueba de que no exista en otro sitio.
- Lo único numérico es **no oficial** (foro, 2009, Metal 3D antiguo, sin E/A/I/Avz) y sirve solo como contraste de orden de magnitud, no como test (sección 3).
- Convenio de signos de **desplazamientos y reacciones**: documentado oficialmente [V] (sección 2). Convenio de **esfuerzos** (N, V, M): no documentado [N].
- Deformación por cortante (TK-105a): sin fuente oficial; solo testimonios de usuarios de foro (sección 4) [N].
- Umbral de desplazamientos excesivos (TK-105c): no documentado [N].

## 1. Fuentes revisadas

| Fuente | Contenido útil | Resultado |
|---|---|---|
| `D:\CYPE Ingenieros\Versión 2027\CYPE 3D` (find de *.pdf, *.ed3, *.chm, *.htm(l), *.txt, *.xml) | Solo `.dll`, `.exe`, `settings.json`, `version.ins`, `chkinsapp.dat`, `uninstall.*`: **ningún** PDF, ayuda ni ejemplo `.ed3` incluido [V] | Nada |
| https://downloads2.cype.com/documents_en/manuals/nm3dmu09.pdf (manual de usuario de CYPE 3D, 1593 líneas de texto, descargado y leído) | Describe barras genéricas con propiedades A, Avy, Avz, Iyy, Izz, It, E, ν, α, peso [V]; opciones de Análisis; «Displacements» y «Reactions» de apoyos. No contiene ejemplos numéricos, ni deformación por cortante, ni convenio de esfuerzos, ni umbrales | Sin valores |
| https://info.cype.com/en/product/cype-3d-analysis/ | Convenio de desplazamientos y reacciones (sección 2), opción «Consider the finite dimension of the nodes»; análisis lineal por defecto | Convenio |
| https://info.cype.com/en/subject/cype-3d-reactions/ , https://info.cype.com/en/subject/cype-3d-displacements/ | Mismo convenio, ejes globales; sin números | Convenio |
| https://info.cype.com/en/subject/cype-3d-forces/ | Diagramas, envolventes; **no** define signos ni ejes locales | Nada |
| https://info.cype.com/?p=292475 (Getting started), https://learning.cype.com/es/video/cype-3d-forces/ (vídeo 51, sin transcripción), https://info.cype.com/en/subject/cype-3d-list-of-issues-in-cype-3d/ | Sin números ni signos; la lista de incidencias no incluye mecanismo/singularidad/desplazamientos excesivos | Nada |
| novedades 2008.1/2011/2012 de Nuevo Metal 3D (novedades.cype.es), `uniones.cype.es/PDFs/InfoMetal3D02.pdf` | Mencionan «cortante» solo en comprobación de resistencia de uniones/aluminio, no en rigidez | Nada |
| Búsquedas web de memorias de cálculo / benchmarks / validación de CYPE 3D | Solo TFG/TFM universitarios que usan CYPE (Valladolid TFG-I-3144, Comillas, Zaragoza TAZ-TFM-2020-1350, Jaén, Vigo) y la memoria de CYPECAD (`downloads2.cype.com/documentos_es/manuales/ccadmc01.pdf`, otro programa). Los PDF académicos devolvieron 403/Anubis; **no se leyeron** | Sin acceso [N] |

Pistas no agotadas: los TFG anteriores (comparan CYPE con cálculo manual o SAP2000) podrían contener casos con números; requieren abrirlos en un navegador (bloqueo anti-bot). Conviene que el usuario los descargue a mano si se quiere seguir por esta vía.

## 2. Convenio de signos documentado [V]

Fuente: https://info.cype.com/en/product/cype-3d-analysis/ , https://info.cype.com/en/subject/cype-3d-reactions/ , https://info.cype.com/en/subject/cype-3d-displacements/ (frases literales traducidas).

- **Desplazamientos**: «The positive sign of a displacement refers to the positive direction of each axis.» Los giros son positivos en sentido antihorario («a rotation referred to an axis indicates a rotation around that axis, and is positive in a counter-clockwise direction»). Valores referidos a los **ejes globales** del modelo.
- **Reacciones**: «A reaction force with a positive sign follows the positive direction of the axis.» Momentos: giro alrededor del eje, positivo antihorario (regla de la mano derecha). Valores en **ejes globales**.
- Las envolventes de desplazamientos/reacciones muestran máximos y mínimos **no simultáneos** («These values cannot be simultaneous»).
- **No se documenta** si la reacción es la fuerza del apoyo sobre la estructura o de la estructura sobre el apoyo. El texto «reaction force with a positive sign follows the positive direction of the axis» es compatible con fuerza del apoyo sobre la estructura (convenio habitual y el del clon, TK-107), pero la frase no lo dice explícitamente [N]. Test sugerido (tolerante): carga de gravedad −Z sobre apoyo => reacción Rz > 0.
- **Convenio de esfuerzos N, Vy, Vz, T, My, Mz**: no documentado en ninguna fuente oficial consultada [N]. TK-107 sigue abierto en lo referente a esfuerzos.

## 3. Casos numéricos encontrados (no oficiales, solo contraste) [N]

Fuente única: foro soloarquitectura.com, hilo «exactitud resultados» (https://www.soloarquitectura.com/foros/threads/exactitud-resultados.42908/), usuario kyua, 28/03/2009, **Metal 3D de esa época**, no CYPE 3D 2027. Faltan E, A, I y áreas de cortante; por tanto no son reproducibles exactamente. No usarlos como test de tolerancia estricta.

| Caso | Datos publicados | Resultado |
|---|---|---|
| F1 Viga continua 2 vanos | 2 × 5 m, IPE-300, q = 20 kN/m uniforme, peso propio desactivado | Reacción apoyo central: **124,64 kN** en Metal 3D vs 125 kN (1,25·q·L) del prontuario (−0,29 %) |
| F2 Pórtico a dos aguas | Luz 20 m, pilares 5 m, flecha de cumbrera 2 m, IPE-400, 5 kN/m sobre cumbrera | Descenso del nudo central: **37,5 mm** en Metal 3D vs 34,9 mm a mano/Cespla (+7 %) |

Uso propuesto como test de «tendencia»: con propiedades estándar de IPE (de tablas) comprobar que el clon con cortante da una reacción central ligeramente menor que 125 kN y un desplazamiento mayor que el de Euler-Bernoulli; en F1 la diferencia debería ser del orden de 0,3 %, y en F2 del orden de 7 %. Esto **no** demuestra que el original use Timoshenko (ver sección 4); además las propiedades exactas del perfil (Avz del programa) son desconocidas.

## 4. Deformación por cortante (TK-105a) [N]

Ninguna fuente oficial de CYPE lo confirma ni lo niega. Indicios no oficiales:
- Foro (misma fuente de la sección 3), JF Sebastian: Metal 3D «considera la influencia de la deformación por cortante en las matrices de rigidez elementales»; fórmula citada 12·EIz/(L³·(1+ψy)) con ψy = 12·EIz/(G·Asy·L²). kyua: anulando las áreas de cortante (Avy = Avz = 0) en SAP2000 los resultados coinciden con el prontuario.
- Foro (https://www.soloarquitectura.com/foros/threads/analisis-sin-consideracion-de-deformaciones-por-corte.118997/, 28/08/2021): un usuario entiende que «CYPE 3D calcula considerando las deformaciones por cortante»; otro (zio) dice que no se puede desactivar. Ninguno es personal de CYPE.
- Manual oficial (nm3dmu09.pdf, §2.2.6): las barras genéricas piden **Avy y Avz** como característica mecánica [V], coherente con que existan en el modelo, pero no prueba su uso en la rigidez.
- La memoria de CYPECAD (otro programa) sí dice que los muros usan elementos de placa gruesa con deformación por cortante; no aplica a barras.

Recomendación: mantener `shearDeformation = true` (valor actual del clon) marcado como **[N] hipótesis apoyada por testimonios**; la opción debe seguir configurable.

## 5. Umbral de desplazamientos excesivos (TK-105c) [N]

Ni las páginas de ayuda, la lista de incidencias, ni el manual lo mencionan. No hay dato. Seguir con detección por pivote nulo/negativo y, si se añade umbral, documentarlo como decisión del clon.

## 6. Otros datos confirmados útiles para el clon [V]

- Análisis **lineal por defecto**; no lineal solo si hay combinaciones/elementos no lineales (info.cype.com/en/product/cype-3d-analysis/).
- Opción «Consider the finite dimension of the nodes»: ajusta las longitudes de barra restando las dimensiones de las uniones; recomendada solo si se van a calcular uniones (misma fuente). Coherente con la sección 4.3 de `solver_lineal_portico.md`.
- «Check bars» desactivado: solo esfuerzos y desplazamientos.
- CYPE 3D usa OpenSees embebido para pandeo, modos y pushover (https://info.cype.com/en/software/cype-3d/), ya recogido.

## 7. Casos con solución cerrada independientes (de `solver_lineal_portico.md` §8, no son de CYPE)

Siguen siendo los únicos tests numéricos exactos disponibles (C1 voladizo, C2 biapoyada, C3 pórtico plano; con y sin cortante). El caso F1 de la sección 3 puede añadirse como test de tendencia con tolerancia amplia (±1 %) usando tablas de IPE-300 estándar; se desaconseja F2 por falta de datos (cubierta a dos aguas con IPE-400 sin Avz ni uniones).

## 8. Qué haría falta para cerrar TK-105/TK-107

1. Un cálculo real en CYPE 3D 2027 (bloqueado por licencia M3D): viga en voladizo L/h bajo, con Avz explícito (barra genérica), comparar flecha con PL³/3EI y PL³/3EI + PL/(G·Avz).
2. Para el convenio de esfuerzos: una viga biapoyada con carga −Z y una consulta de esfuerzos en el original.
3. O bien abrir los TFG de la sección 1 en un navegador y extraer sus tablas CYPE vs manual (sin garantía de datos completos).
