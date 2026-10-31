# 05 – Verificación de licencias y toolchain (T-STK-01/02/03)

Fecha: 2026-10-31. Fuente: API de GitHub y ficheros de los repositorios oficiales (no memoria).

## Resultados

| Componente | Licencia verificada | Veredicto |
|---|---|---|
| Qt 6 (qtbase) | Repo `qt/qtbase/LICENSES`: LGPL-3.0-only, GPL-2/3, LicenseRef-Qt-Commercial | OK en LGPLv3 con enlace dinámico (DLL de Qt), incluir textos de licencia y aviso. Usar solo módulos LGPL (Core/Gui/Widgets/OpenGL/Svg/PrintSupport). Evitar módulos solo GPL/comerciales |
| SARibbon | MIT; release v2.9.6 (2026-10-29); CI con Qt 6.8 LTS (Windows); CMake + `vcpkg.json` | OK. Usar Qt 6.8 LTS |
| IfcOpenShell | LGPL-3.0 | Aceptable con enlace dinámico (fase 17) |
| web-ifc (C++ nativo) | MPL-2.0 | Alternativa permisiva (fase 17) |
| tinygltf | MIT | OK |
| libdxfrw (codelibs) | GPL-2.0 | **Evitar**: parser DXF propio |
| LibreDWG | GPL-3.0 | **Evitar** (DWG): DWG solo vía conversor externo o fuera del MVP |
| Eigen | MPL-2.0 | OK |
| GoogleTest | BSD-3-Clause | OK |
| vcpkg | MIT | OK |
| **OpenSees** | Fichero `COPYRIGHT` (UC Regents), `NOASSERTION` en GitHub | **RIESGO ALTO**, ver abajo |

## OpenSees (TK-001) – hallazgo importante
El texto vigente permite: (a) uso, reproducción, modificación y **distribución** solo por entidades educativas, de investigación y sin ánimo de lucro con fines no comerciales; (b) **uso y modificación por otras entidades solo para fines internos**. Incorporarlo en productos de distribución comercial requiere permiso de UC Berkeley OTL.

Implicación: un repositorio público de GitHub o un `.zip` portable que **redistribuya** OpenSees podría quedar fuera de (a)/(b) si el titular no es una entidad educativa/sin ánimo de lucro. La decisión D2 del usuario (embeber) se mantiene, pero se recomienda esta mitigación (a confirmar por el usuario antes de la fase 11):
1. No incluir el código fuente de OpenSees en el repo ni en el `.zip`; `third_party/OpenSees` se obtiene con un script local (`tools/fetch_opensees.ps1`) y es opcional en CMake (`CYPECLWON_WITH_OPENSEES`).
2. El `.zip` portable distribuido públicamente no lleva OpenSees; el solver propio (Eigen) cubre el caso por defecto.
3. Uso personal del binario con OpenSees = uso interno (permitido).

## Toolchain detectada
- MSVC 14.51 (Visual Studio Build Tools **2026**, v18.8) + Windows SDK 10.0.26100 → sustituye a VS 2022 (mismo conjunto de herramientas v14.x, compatible).
- CMake 4.4.4 y Ninja (MinGW portable en `D:\ProgramsPortable\mingw64\bin`), Git, Python 3.12.
- **Ausentes:** VS 2022, vcpkg, Qt 6. Se instalarán dentro del proyecto (`.toolchain/`, ignorado por git) para respetar la regla de escritura solo en el proyecto.
