// Developer aid: writes a portal-frame test project (.ed3) with two extra layers, used for visual checks of the clone.
#include <filesystem>
#include <iostream>
#include <string>

#include "core/bar_load.h"
#include "core/load_case_edit.h"
#include "core/nodal_load_edit.h"
#include "core/project_file.h"
#include "core/undo_stack.h"
#include "core/panel.h"
#include "core/shell_load.h"
#include "core/shell_properties.h"

int main(int argc, char** argv) {
    if (argc < 2) { std::cerr << "usage: make_demo_project <out.ed3> [steel]\n"; return 2; }
    const bool steel = argc > 2 && std::string(argv[2]) == "steel";  // bars with a steel material (steel variant of the Pandeo dialog)
    using namespace cc::core;
    ProjectDocument doc;
    if (argc > 2 && std::string(argv[2]) == "results") {  // T-P8-06 smoke test: the measured cantilever C1 plus a portal leg, calculable
        auto& st = doc.project.structure;
        Material mat;
        mat.name = "ref";
        mat.kind = MaterialKind::Steel;
        mat.elasticModulusMPa = 210000.0;
        mat.poissonRatio = 0.3;
        mat.densityKnM3 = 77.01;
        const int m = st.addMaterial(mat);
        CrossSection cs;
        cs.name = "ref";
        cs.areaM2 = 0.01;
        cs.inertiaYM4 = 1e-4;
        cs.inertiaZM4 = 5e-5;
        cs.torsionM4 = 8e-5;
        cs.shearAreaYM2 = 0.004;
        cs.shearAreaZM2 = 0.004;
        const int s = st.addSection(cs);
        const int n1 = st.addNode({0, 0, 0}), n2 = st.addNode({0, 0, 4}), n3 = st.addNode({6, 0, 4}), n4 = st.addNode({6, 0, 0});
        st.addBar(n1, n2, m, s);
        st.addBar(n2, n3, m, s);
        st.addBar(n3, n4, m, s);
        ExternalSupport fixed;
        fixed.type = ExternalSupportType::Fixed;
        doc.project.nodeSupports.external[n1] = fixed;
        doc.project.nodeSupports.external[n4] = fixed;
        NodalLoad load;
        load.id = doc.project.nodalLoads.nextId++;
        load.nodeId = n2;
        load.loadCaseId = 0;
        load.magnitude = 20.0;
        load.direction = LoadDirection::PlusX;
        doc.project.nodalLoads.items.push_back(load);
        const auto saved = saveProjectFile(argv[1], doc);
        if (!saved.ok) { std::cerr << saved.error << "\n"; return 1; }
        return 0;
    }
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
    if (argc > 2 && std::string(argv[2]) == "panelsp6") {  // T-P7-05 on-screen checks: the measured work p6_a (5/20/15 kN/m) with CM 1 and V 1 as well
        auto& st = doc.project.structure;
        const double xyz[8][3] = {{0, 0, 0}, {4, 0, 0}, {4, 4, 0}, {0, 4, 0}, {0, 1, 0}, {4, 1, 0}, {4, 0, 3}, {0, 0, 3}};
        int n[8];
        for (int i = 0; i < 8; ++i) n[i] = st.addNode({xyz[i][0], xyz[i][1], xyz[i][2]});
        for (const auto& [from, to] : {std::pair{0, 1}, {1, 5}, {5, 2}, {2, 3}, {3, 4}, {4, 0}, {4, 5}}) st.addBar(n[from], n[to], 0);
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
        Panel horizontal;
        horizontal.id = doc.project.panels.nextId++;
        horizontal.vertices = {{0, 0, 0}, {4, 0, 0}, {4, 4, 0}, {0, 4, 0}};
        horizontal.direction = {0, 1, 0};
        horizontal.loads.push_back(surfaceLoadDefaults(0));
        horizontal.loads.back().value = 10.0;
        horizontal.loads.push_back(surfaceLoadDefaults(cm));
        horizontal.loads.back().value = 5.0;
        doc.project.panels.items.push_back(horizontal);
        Panel vertical;
        vertical.id = doc.project.panels.nextId++;
        vertical.vertices = {{0, 0, 0}, {4, 0, 0}, {4, 0, 3}, {0, 0, 3}};
        vertical.direction = {0, 0, 1};
        vertical.loads.push_back(surfaceLoadDefaults(wind));
        vertical.loads.back().value = 4.0;
        doc.project.panels.items.push_back(vertical);
        const auto saved = saveProjectFile(argv[1], doc);
        if (!saved.ok) { std::cerr << saved.error << "\n"; return 1; }
        return 0;
    }
    if (argc > 2 && std::string(argv[2]).rfind("panels", 0) == 0) {  // T-P7-05 visual checks: 4 x 4 m on four bars; "panelsload" adds a panel, rows and a surface load
        auto& st = doc.project.structure;
        const int n1 = st.addNode({0, 0, 0}), n2 = st.addNode({4, 0, 0}), n3 = st.addNode({4, 4, 0}), n4 = st.addNode({0, 4, 0});
        st.addBar(n1, n2, 0);
        st.addBar(n2, n3, 0);
        st.addBar(n3, n4, 0);
        st.addBar(n4, n1, 0);
        if (std::string(argv[2]) == "panelsload") {
            Panel panel;
            panel.id = doc.project.panels.nextId++;
            panel.vertices = {{0, 0, 0}, {4, 0, 0}, {4, 4, 0}, {0, 4, 0}};
            panel.direction = {1, 0, 0};
            panel.loads.push_back(surfaceLoadDefaults(0));
            panel.loads.back().value = 10.0;
            doc.project.panels.items.push_back(panel);
            SurfaceLoad load;
            load.id = doc.project.surfaceLoads.nextId++;
            load.contour = {{0, 0, 0}, {2, 0, 0}, {2, 2, 0}, {0, 2, 0}};
            load.data = surfaceLoadDefaults(0);
            load.data.value = 8.0;
            doc.project.surfaceLoads.items.push_back(load);
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
    if (argc > 2 && std::string(argv[2]) == "hypos") {  // T-P7-07 visual checks: hypotheses CM 1, T 1, V 1 with nodal and bar loads of each and a shell load
        UndoStack undo;
        int ids[3] = {};
        const std::pair<LoadNature, const char*> natures[3] = {{LoadNature::DeadLoad, "CM 1"}, {LoadNature::Temperature, "T 1"}, {LoadNature::Wind, "V 1"}};
        for (int i = 0; i < 3; ++i) {
            NewLoadCase c;
            c.nature = natures[i].first;
            c.reference = natures[i].second;
            createLoadCase(doc.project, undo, c, &ids[i]);
        }
        const int nodes[4] = {a, b, c, d};
        const int cases[4] = {0, ids[0], ids[1], ids[2]};
        for (int i = 0; i < 4; ++i) {
            NodalLoadData data;
            data.magnitude = 5.0;
            data.direction = LoadDirection::MinusZ;
            data.loadCaseId = cases[i];
            addNodalLoads(doc.project, undo, {nodes[i]}, data);
        }
        int next = 1;
        for (const int bar : {col1, beam, col2}) {
            BarLoad load;
            static_cast<BarLoadData&>(load) = barLoadDefaults(cases[next]);
            load.id = doc.project.barLoads.nextId++;
            load.barId = bar;
            doc.project.barLoads.items.push_back(load);
            ++next;
        }
        const int n1 = s.addNode({10, 0, 0}), n2 = s.addNode({14, 0, 0}), n3 = s.addNode({14, 4, 0}), n4 = s.addNode({10, 4, 0});
        const int shell = s.addShell({n1, n2, n3, n4}, 0.1);
        for (const int caseId : {ids[0], ids[2]}) {
            ShellLoad load;
            static_cast<ShellLoadData&>(load) = shellLoadDefaults(caseId, ShellLoadType::Uniform);
            load.value1 = 5.0;
            load.id = doc.project.shellLoads.nextId++;
            load.shellId = shell;
            doc.project.shellLoads.items.push_back(load);
        }
        doc.project.loadCases.viewAllCases = true;
    }
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
