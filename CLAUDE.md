# Directrices del Proyecto

Clonar el proyecto Cype 3D manteniendo absoluta fidelidad en la reproducción de sus funcionalidades e interfaz. La fase de investigación y descubrimiento es tan importante como la fase de desarrollo.

El proyecto está ubicado en:
D:\CYPE Ingenieros\Versión 2027\CYPE 3D

Ejecutable:
"D:\CYPE Ingenieros\Versión 2027\CYPE 3D\CYPE 3D.exe"


## Herramienta de ingeniería inversa

Se utilizará el mcp rea incorporado en este proyecto. Su Skill también existe dentro del proyecto.

Adicionalmente iniciar el ejecutable y realizar capturas de pantalla y pruebas de funcionalidad y documentarlo. Adicionalmente, haz un research extensivo de las características del programa en:
https://info.cype.com/es/software/cype-3d/?_gl=1%2A1sl1l1t%2A_ga%2ANjI4MjI2NTcxLjE3OTM0Mzc0NzY.%2A_ga_P0GJKB1YE4%2AczE3OTM0Mzc0NzYkbzEkZzAkdDE3OTM0Mzc0ODQkajUyJGwwJGgw


## Plan de Implementación

Primer paso: evaluar en detalle el proyecto utilizando el MCP rea para determinar su complejidad. Luego elaborar un detallado plan de implementación y guardarlo en el archivo .planning.md - este plan de implementación debe de ser elaborado de forma que te sirva de guía durante todo el proceso de desarrollo. Detallar todas las funcionalidades e GUI a clonar en una secuencia lógica y dentro de lo posible secuencial.


## Control de tareas

Elabora una lista desglosada de tareas atómicas basada en el plan de implementación. Crea el archivo .tasks.md como referencia para el control de tareas. Cada vez que realices total o parcialmente una tarea, o altere tareas ya realizadas, actualiza el estado de la tarea en el archivo .tasks.md. En la columna "Comentarios" del archivo .tasks.md, anota que has realizado la tarea y cualquier comentario relevante.

Caso una tarea no pueda ser realizada debido a dependencia de features aun no desarrolladas o cualquier otro motivo, en el archivo .ticktets.md crea un "Ticket" asociando la tarea pendiente a su tarea complementaria futura, de forma que una tarea pendiente pueda ser finalizada en el momento correcto.

Serás el único responsable de marcar una tarea como realizada, pendiente o en marcha. Las tareas pendientes se marcarán con "[ ]", las tareas finalizadas con "[x]" y las tareas en marcha con "[o]"

Para que el usuario pueda realizar un seguimiento proactivo del desarrollo del proyecto crea un informe en .tasks.html con un dashboard indicativo del avance del desarrollo desde de diversas perspectivas. Actualizalo al finalizar cada tarea atómica.


## Criterio de elección de tarea a realizar

Con base al archivo .tasks.md controla el desarrollo del proyecto mirando las tareas no finalizadas. Elige con cual trabajar en función de lo que la lógica de desarrollo tenga sentido. Siempre usando el mcp REA cuando sea necesario.


## Lenguaje de programación

Usar C++20 y herramientas auxiliares, librerías, herramientas GUI (Qt u otra, ten en cuenta la ventana gráfica de diseño de estructuras para definirlo). Debes de integrar las decisiones de Stack en .planning.md - no sin antes presentarlo al usuario y en este caso sí, será necesario obtener su aprobación


## Calidad

Asegúrate de que todas las funcionalidades son clonadas, excepto si el usuario te pide explicitamente excluir determinadas funciones o características


# Agentes

Crear un equipo de agentes para el desarrollo de la aplicación. Asigna el modelo más apropiado a cada agente, de forma que se garantice la calidad pero a la vez se maximice el ahorro de tokens.

Abajo un ejemplo de equipo de agentes, que **obligatoriamente tendrás que adaptar a las necesidades del proyecto:**

- ORCHESTRATOR: el agente por defecto y que usa el modelo de AI seleccionado por el usuario. Delega tareas a los sub-agents que pertocan y se comunica directamente con ellos. Es el único agente que reporta al usuario. Crear/buscar el skill para crear este agent.
- SOFTWARE-ARCHITECHT: el agente que coordinado por ORCHESTRATOR. Especializado en desarrollo de software. Crear/buscar el skill para crear este agent.
- BACKEND-DEVELOPER: el nombre lo dice.  Crear/buscar el skill para crear este agent. No malgastar tokens, ni ahorrarlos al punto de sacrificar calidad. Decidir el modelo en función de las tareas delegadas (para cada tarea un modelo ponderado)
- FRONTEND-DEVELOPER: el nombre lo dice. crear/obtener skill. No malgastar tokens, ni ahorrarlos al punto de sacrificar calidad. Decidir el modelo en función de las tareas delegadas (para cada tarea un modelo ponderado)
- DEBUGGER: crear/obtener skill. No escatimar recursos
- TESTING:  crear/obtener skill. Usar todas las herramientas disponibles, incluso tomar control del ordenador.
- QA:  crear/obtener skill. Una vez no hayan bugs, realizar todas las pruebas posibles. Verificar que sea un software a prueba de idiotas - no que sea tan self-explaining, pero que si un idiota intenta hacer una idiotez, queda bloqueada la acción. Un Quality Assistan en toda regla. Usar todas las herramientas disponibles, incluso tomar control del ordenador.
- SCORER: vigila el desarrollo de la aplicación y permite un seguimiento activo, visual por parte del desarrollador.
- DEPLOYER: se encarga de que la aplicación sea distribuible, en este caso en formato "Portable" (compactada en un .zip y con todas las dependencias incluidas)
- MARKETING: una vez completado el proyecto, lo hará visible al mundo.


## Acceso a Archivos y Web

- **Lectura global:** Puedes consultar (modo solo lectura) archivos en cualquier ubicación de este ordenador cuando sea necesario.
- **Escritura restringida:** NUNCA alteres, modifiques ni crees ningún archivo fuera de la carpeta de este proyecto.
- **Búsqueda de información:** Cuando te falte información, búscala en internet. Si no encuentras lo que buscas, pregúntaselo al usuario. NUNCA deduzcas ni asumas algo si no estás completamente seguro de ello.
- **Ejecución de programas externos al proyecto**: cuando sea necesario ejecuta programas ajenos a este proyecto si te ayudan de alguna forma en el desarrollo del proyecto.


## Git y GitHub

- Gestionarás de forma autónoma Git y GitHub (push, merge, etc...).


## Ventana de contexto

Cuando la ventana de contexto llegue al 80%, para con la tarea actual y crea un prompt detallado para continuar con la tarea desde una nueva conversación.
Supervisa constantemente el contexto de la ventana para evitar compresiones innecesarias.
### Regla del umbral de contexto (Límite estricto < 70%)

- **Monitoreo activo:** El agente y el desarrollador deben supervisar continuamente el uso de tokens en el contexto.

- **Detención preventiva al 60–65%:** Si el contexto de la conversación alcanza entre el **60% y el 65%** de la capacidad de la ventana, la sesión activa **debe entrar inmediatamente en modo de cierre**. Nunca esperes a llegar al 70% o superarlo.

- **Límite atómico:** No se debe iniciar la implementación de ninguna funcionalidad nueva una vez que la sesión supere el 55% de uso, a menos que dicha funcionalidad pueda programarse, probarse y documentarse completamente dentro del margen restante del 5%.

- **Rutina de cierre:** Al realizar una transición de sesión:
1. Completa y confirma (commit/guarda) la edición atómica actual del archivo. 
2. Ejecuta el conjunto de pruebas (`pytest`) para confirmar que el estado base es correcto (todas las pruebas pasan). 
3. Genera el **Bloque estándar de traspaso de sesión** (definido en la sección siguiente)
4. Indica explícitamente al usuario que debe iniciar una nueva sesión de chat.

### Formato estándar de traspaso de sesión

Al finalizar cada sesión de chat (o al alcanzar entre el 60 y el 65 % del límite de contexto), el agente debe proporcionar lo siguiente:

📋 Resumen de traspaso de sesión
- **Paso completado:** [p. ej., Paso 03: Analizador y serializador XML]
- **Entregables creados/modificados:**
- **Estado de verificación:** -> X superado en 0,18 s
- **Estado actual en Git / entorno de trabajo:** Limpio, todas las pruebas superadas.
- **Siguiente paso a ejecutar:**

#### 🚀 Instrucción (prompt) para iniciar la siguiente sesión

> Copia y pega el siguiente fragmento en el nuevo chat para retomar el trabajo sin interrupciones:

<!-- > *"Retoma el desarrollo de XXXXXX en el Paso X: [nombre del proyecto]. Revisa [DEVELOPMENT_PLAN.md](file:./DEVELOPMENT_PLAN.md) y [project.md](file:./project.md). Verifica el estado actual con `pytest` antes de comenzar."* -->


## Idiomas

Todos los proyectos serán por defecto multi-o pluri-idiomas. Eel código comentarios, variables se desarrollarán en inglés británico. El idioma GUI del MVP o versión inicial será el español de España.

Una vez finalizado el desarrollo al 100% se realizará la traducción a los idiomas adicionales.

### Idiomas adicionales obligatorios

-  Inglés británico
- Portugués (Portugal)
- Portugués (Brasil)
- Alemán estándard
- Italiano
- Holandés
- Francés


## Licencia

Este programa es para mi uso personal, pero estará en un repositorio de Github. La licencia será totalmente permisiva, MIT. No replicar activaciones de licencia ni nada similar.


## Comunicación con el usuario

- Siempre que hayas finalizado alguna tarea y necesites información para seguir, informa al usuario de inmediato.
- No te quedes en silencio sin motivo.
- Si esperas el resultado de alguna tarea en segundo plano, pon un aviso en MAYÚSCULAS (por ejemplo: "ESPERANDO TAREA EN SEGUNDO PLANO: <qué tarea>").
- Antes de lanzar cualquier aplicación para capturar pantalla (la original o el clon), AVISA al usuario para que libere el workspace (pantalla) y espera su OK explícito antes de lanzarla. No lances aplicaciones con ventana sin ese OK.