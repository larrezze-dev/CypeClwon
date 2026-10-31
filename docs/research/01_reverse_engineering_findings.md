# Hallazgos de ingeniería inversa – CYPE 3D v2027.a

Fecha: 2026-10-31. Fuente: REA MCP (`inspect_desktop_application`, `inspect_pe_imports`, `inspect_native_exports`), listado de ficheros y ejecución real del programa.
Capturas: `docs/screenshots/`.

## Observaciones (evidencia directa)

| Hecho | Evidencia |
|---|---|
| Instalación: 682 ficheros, ~987 MB; 673 DLL, 5 EXE, `settings.json`, `version.ins`, `chkinsapp.dat`, `uninstall.dat` | Listado de directorio; REA `inspect_desktop_application` (ev_6e3f9ed8…) |
| `CYPE 3D.exe` (1,1 MB, x64) es un lanzador mínimo: solo importa `KERNEL32` y delay-load de DLL propias (`entograf.dll!buclmain_WinMain`, `mn3dexebs.dll!mn3dexebs_FuncionPrincipal`, `arrays`, `cypestr`, `textomsg`, `pastjano` (permisos/licencias), `share`, `contcype`…) | REA `inspect_pe_imports` (ev_d443a868…) |
| La aplicación real está en DLL nativas propias de CYPE: no es Qt, .NET ni Electron. GUI propia (barra de título roja, ribbon propio, diálogos modales rojos) | REA no detecta ningún framework; capturas |
| `cyopenseesbas.dll` = 49,8 MB con solo **8 exports** → OpenSees enlazado estáticamente (motor no lineal, pandeo, modal, pushover) | REA `inspect_native_exports` (ev_4c2f0b7d…) |
| Solver lineal propio: `calcmef.dll`, `calcmallad3d.dll`, `calcesf.dll`, `calchorm.dll`, `calcrec.dll`, `sismo*.dll`, `pushover*.dll`, `pilot*.dll` | Nombres de DLL |
| Gráficos 3D: `cyopengl.dll`, `dllvargl.dll`, `visual3d.dll`, `vistas3d.dll`, `visesf3d.dll` → OpenGL propio | Nombres de DLL (los imports no se resolvieron por cadena ASCII) |
| Ficheros de obra: extensión `.ed3` | Diálogo "Nueva obra" |
| Idiomas: DLL de recursos `ejd3d{ale,bra,bul,fra,ing,ita,por}.dll` (alemán, brasileño, búlgaro, francés, inglés, italiano, portugués) y `acchile`, `argelia`, `australia`, `british`, `bulgaria`… (datos por país). **No hay holandés** | Nombres de DLL |
| Integración BIM: `bim*.dll`, `bsifc*.dll`, `cygltf*.dll`, `libfbxsdk.dll`, `dxflib*.dll`, `cydwgdxf.dll`, `pdf*.dll` | Nombres de DLL |
| Ventana: 1920×1080 maximizada, DPI 100 %. Título `CYPE 3D v2027.a - <obra>.ed3` | Capturas 01–06 |

## Inferencias / incógnitas
- Los gráficos usan OpenGL (inferido por nombres). Imports de `opengl32` no verificados.
- Contenido de los `ejd3d*.dll` (¿tablas de cadenas/menús/diálogos?) sin inspeccionar: tarea T-RE-04.
- Formato binario `.ed3`: no analizado (T-RE-05).
- **OpenSees**: su licencia exige licencia comercial de UC Berkeley para redistribuir software que incluya su código; no es compatible con una redistribución MIT sin acuerdo. Ver `.planning.md` §Stack.

## Flujo de arranque observado
1. Pantalla de inicio: paneles *Archivo* (Nuevo…, Gestión archivos, Ejemplos), *Últimos ficheros* (6 + "Más…"), *Ayuda* (10 enlaces) + botón *Gestión de licencias*; barra superior: *Iniciar sesión*, *CYPE Mentor*.
2. *Nueva obra*: nombre/ruta (`Examinar`), nombre de fichero `.ed3`, descripción.
3. *Nueva obra* (tipo): *Obra vacía* / *Introducción automática IFC* (con ilustración).
4. Asistente *Obra vacía*: Normas, Perfiles (acero laminado/conformado, madera, aluminio, hormigón), Hormigón armado (pilares, vigas de forjado, cimentación, acero barras, árido, recubrimientos, mermas), Acciones (sismo dinámico, resistencia al fuego, estados límite, hipótesis adicionales, cimentación, proceso constructivo), Opciones (pilares, cimentación, vigas, uniones), Ambiente (vigas X0), Terreno de cimentación (tensiones admisibles, pilotes/micropilotes). Botones Cancelar/Anterior/Siguiente/**Terminar**.
5. *Terminar* → diálogo *Estados límite* → *Aceptar* → editor principal.
