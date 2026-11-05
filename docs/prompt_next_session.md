# Prompt para la siguiente sesión (T-P5-08 a T-P5-11)

> Copia y pega el siguiente fragmento en el nuevo chat:

Continúa el proyecto CypeClwon (D:\Coding\CypeClwon, rama main, último commit 5c84447, 603/603 tests, 3 omitidos de instantáneas visuales). Lee CLAUDE.md, .tasks.md, .tickets.md y docs/research/ui/describir_barras*.md.

Estado: T-P5-07 (Describir barras) está en [o]. TK-075 y TK-077 se cerraron por REA el 2026-11-05; sus restos están anotados en .tickets.md: UB, ν/α de madera, norma de madera por defecto, clases de frondosas/laminadas, densidad del acero laminado, (i) de IPN/UPN/L. TK-070 y TK-076 siguen abiertos y dependen de T-P5-08 (pilares/vigas, tirantes, etiquetas de barra, páginas Pilar y Viga del diálogo Describir).

Tareas, por orden: T-P5-08 (edición de pilares y vigas, que desbloquea TK-070 y TK-076), T-P5-09 (láminas: nueva, dividir, unir, ejes locales, invertir z), T-P5-10 (herramientas: mover, copiar, simetría, reparar…) y T-P5-11 (importar DXF/DWG y fichero de texto).

Método obligatorio: REA primero sobre los binarios de D:\CYPE Ingenieros\Versión 2027\CYPE 3D (cadenas de recursos, list_procedures, procedure_pseudo_code). Después, si REA no responde, observar el original con tools/drive_original.ps1. Documenta lo averiguado en docs/research/. Delega con agentes (re-analyst, backend-developer, frontend-developer) en segundo plano. Sirve que dos agentes no usen REA a la vez: el binario activo es global y se pisan; serialízalos. Compila y prueba con tools/build.ps1 y actualiza el dashboard con tools/gen_dashboard.ps1. Tras cada tarea actualiza .tasks.md, .tickets.md y .tasks.html, y haz commit. Cierra cada tarea salvo lo que dependa de módulos aún no desarrollados.

Lanzamiento de aplicaciones: según CLAUDE.md necesitas mi autorización en CADA sesión. Pídemela al inicio, antes de lanzar el clon, el original u otra aplicación con ventana. Mientras no la concedo, trabaja solo con REA y los tests.

Contexto: avisa al llegar al 55 % (no empezar funcionalidades nuevas) y cierra con el bloque de traspaso al 60-65 %.
