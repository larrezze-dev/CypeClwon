// Developer aid: writes a portal-frame test project (.ed3) with two extra layers, used for visual checks of the clone.
#include <filesystem>
#include <iostream>

#include "core/project_file.h"

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: make_demo_project <out.ed3>\n"; return 2; }
    using namespace cc::core;
    ProjectDocument doc;
    auto& s = doc.project.structure;
    const int a = s.addNode({0, 0, 0}), b = s.addNode({0, 0, 4}), c = s.addNode({6, 0, 4}), d = s.addNode({6, 0, 0});
    const int col1 = s.addBar(a, b), beam = s.addBar(b, c), col2 = s.addBar(d, c);
    const int red = s.addLayer("Vigas", 0xCC0000);
    s.addLayer("Pilares", 0x0066CC);
    s.assignLayers({beam}, {}, {red}, LayerOperation::Replace);
    (void)col1; (void)col2;
    const auto r = saveProjectFile(argv[1], doc);
    if (!r.ok) { std::cerr << r.error << "\n"; return 1; }
    return 0;
}
