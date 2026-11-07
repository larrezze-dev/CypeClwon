// Developer aid: writes a portal-frame test project (.ed3) with two extra layers, used for visual checks of the clone.
#include <filesystem>
#include <iostream>
#include <string>

#include "core/project_file.h"

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: make_demo_project <out.ed3> [steel]\n"; return 2; }
    const bool steel = argc > 2 && std::string(argv[2]) == "steel";  // bars with a steel material (steel variant of the Pandeo dialog)
    using namespace cc::core;
    ProjectDocument doc;
    auto& s = doc.project.structure;
    const int a = s.addNode({0, 0, 0}), b = s.addNode({0, 0, 4}), c = s.addNode({6, 0, 4}), d = s.addNode({6, 0, 0});
    int materialId = 0;
    if (steel) {
        Material m;
        m.name = "Acero";
        m.kind = MaterialKind::Steel;
        materialId = s.addMaterial(m);
    }
    const int col1 = s.addBar(a, b, materialId), beam = s.addBar(b, c, materialId), col2 = s.addBar(d, c, materialId);
    const int red = s.addLayer("Vigas", 0xCC0000);
    s.addLayer("Pilares", 0x0066CC);
    s.assignLayers({beam}, {}, {red}, LayerOperation::Replace);
    (void)col1; (void)col2;
    const auto r = saveProjectFile(argv[1], doc);
    if (!r.ok) { std::cerr << r.error << "\n"; return 1; }
    return 0;
}
