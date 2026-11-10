// Developer aid: writes a portal-frame test project (.ed3) with two extra layers, used for visual checks of the clone.
#include <filesystem>
#include <iostream>
#include <string>

#include "core/load_case_edit.h"
#include "core/project_file.h"
#include "core/undo_stack.h"
#include "core/shell_load.h"
#include "core/shell_properties.h"

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: make_demo_project <out.ed3> [steel]\n"; return 2; }
    const bool steel = argc > 2 && std::string(argv[2]) == "steel";  // bars with a steel material (steel variant of the Pandeo dialog)
    using namespace cc::core;
    ProjectDocument doc;
    if (argc > 2 && std::string(argv[2]).rfind("shellonly", 0) == 0) {  // T-P7-04 visual checks: one 4 x 4 m shell (100 mm) alone, with or without loads
        auto& st = doc.project.structure;
        const int n1 = st.addNode({0, 0, 0}), n2 = st.addNode({4, 0, 0}), n3 = st.addNode({4, 4, 0}), n4 = st.addNode({0, 4, 0});
        const int shell = st.addShell({n1, n2, n3, n4}, 0.1);
        if (std::string(argv[2]) == "shellonlyloads") {
            const auto add = [&](ShellLoadType type, double v1, double v2) {
                ShellLoad load;
                static_cast<ShellLoadData&>(load) = shellLoadDefaults(0, type);
                load.value1 = v1;
                load.value2 = v2;
                load.id = doc.project.shellLoads.nextId++;
                load.shellId = shell;
                doc.project.shellLoads.items.push_back(load);
            };
            add(ShellLoadType::Uniform, 5.0, 0.0);
            add(ShellLoadType::VariableX, 5.0, 15.0);
        }
        if (std::string(argv[2]) == "shellonlycases") {  // tanda 4 alignment: hypotheses CM 1 and V 1, one uniform load of the shell in each
            UndoStack undo;
            int cm = 0, wind = 0;
            NewLoadCase c;
            c.nature = LoadNature::DeadLoad;
            c.reference = "CM 1";
            createLoadCase(doc.project, undo, c, &cm);
            NewLoadCase w;
            w.nature = LoadNature::Wind;
            w.reference = "V 1";
            createLoadCase(doc.project, undo, w, &wind);
            for (const auto& [caseId, value] : {std::pair{cm, 9.81}, std::pair{wind, 5.0}}) {
                ShellLoad load;
                static_cast<ShellLoadData&>(load) = shellLoadDefaults(caseId, ShellLoadType::Uniform);
                load.value1 = value;
                load.id = doc.project.shellLoads.nextId++;
                load.shellId = shell;
                doc.project.shellLoads.items.push_back(load);
            }
        }
        const auto saved = saveProjectFile(argv[1], doc);
        if (!saved.ok) { std::cerr << saved.error << "\n"; return 1; }
        return 0;
    }
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
    if (argc > 2 && (std::string(argv[2]) == "shell" || std::string(argv[2]) == "shellloads")) {  // a 4 x 4 m shell L1 with non-default shell properties (T-P6-07 visual checks)
        const int n1 = s.addNode({10, 0, 0}), n2 = s.addNode({14, 0, 0}), n3 = s.addNode({14, 4, 0}), n4 = s.addNode({10, 4, 0});
        const int shell = s.addShell({n1, n2, n3, n4}, 0.1);
        ShellRecord record;
        record.maxTriangleSideM = 0.5;
        record.bands.push_back(normaliseBand({0.5, 1.0, 3.5, 1.0, 0.2}));
        setShellRecord(doc.project.shellProps, shell, record);
        if (std::string(argv[2]) == "shellloads") {  // T-P7-04 visual checks: a uniform load (sense 1) and a wedge on the same shell
            ShellLoad uniform;
            static_cast<ShellLoadData&>(uniform) = shellLoadDefaults(0, ShellLoadType::Uniform);
            uniform.value1 = 5.0;
            uniform.id = doc.project.shellLoads.nextId++;
            uniform.shellId = shell;
            doc.project.shellLoads.items.push_back(uniform);
            ShellLoad wedge;
            static_cast<ShellLoadData&>(wedge) = shellLoadDefaults(0, ShellLoadType::VariableX);
            wedge.value1 = 5.0;
            wedge.value2 = 15.0;
            wedge.id = doc.project.shellLoads.nextId++;
            wedge.shellId = shell;
            doc.project.shellLoads.items.push_back(wedge);
        }
    }
    const auto r = saveProjectFile(argv[1], doc);
    if (!r.ok) { std::cerr << r.error << "\n"; return 1; }
    return 0;
}
