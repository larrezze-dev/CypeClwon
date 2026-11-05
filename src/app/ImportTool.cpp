#include "ImportTool.h"

#include <QAction>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMessageBox>
#include <fstream>
#include <set>
#include <sstream>

#include "Icons.h"
#include "ImportDialogs.h"
#include "core/general_data.h"
#include "core/geom3.h"
#include "core/import_geometry.h"
#include "core/import_text.h"

namespace cc::app {

namespace {
QString fromStd(const std::string& s) { return QString::fromStdString(s); }

QString messageOf(const cc::core::ImportMessage& m) { return fromStd(m.text); }
}  // namespace

ImportTool::ImportTool(QObject* parent) : QObject(parent) {
    m_dxf = new QAction(tr("DXF o DWG"), this);
    m_dxf->setStatusTip(tr("Importar - DXF o DWG. Permite generar la geometría de la obra automáticamente a partir de un fichero DXF o DWG. "
                           "Puede desactivar las capas de los elementos que no se quieren contemplar. Es imprescindible que la unidad de dibujo sea el metro."));
    m_text = new QAction(tr("Fichero de texto"), this);
    m_text->setStatusTip(tr("Importar - Fichero de texto. Permite generar la geometría de la obra a partir de un fichero de texto con los sectores NUDOS, BARRAS, LAMINAS y PAÑOS."));
    connect(m_dxf, &QAction::triggered, this, [this] { importDxf(); });
    connect(m_text, &QAction::triggered, this, [this] { importText(); });
    updateEnabled();
}

void ImportTool::setProject(cc::core::Project* project, cc::core::UndoStack* undo) {
    m_project = project;
    m_undo = undo;
    updateEnabled();
}

void ImportTool::updateEnabled() {
    const bool open = m_project != nullptr;
    m_dxf->setEnabled(open);
    m_text->setEnabled(open);
}

QString ImportTool::dxfFilter() { return tr("Ficheros DXF") + QStringLiteral(" (*.dxf);;") + tr("Ficheros DWG") + QStringLiteral(" (*.dwg)"); }
QString ImportTool::textFilter() { return tr("Ficheros de texto") + QStringLiteral(" (*.txt)"); }

int ImportTool::run(QDialog& dialog) { return m_runner ? m_runner(dialog) : dialog.exec(); }

bool ImportTool::confirm(const QString& question) {
    if (m_confirmer) return m_confirmer(question);
    return true;
}

bool ImportTool::hasGroupedPlanes() const {
    if (!m_project) return false;
    const auto& g = m_project->workPlanes.groups;
    return !g.x.empty() || !g.y.empty() || !g.z.empty();
}

bool ImportTool::importDxf() {
    if (!m_project) return false;
    emit aboutToImport();
    QString path;
    if (m_chooser) path = m_chooser(Kind::Dxf);
    else path = QFileDialog::getOpenFileName(m_dialogParent, tr("Selección de ficheros DXF-DWG a leer"), QString(), dxfFilter());
    return !path.isEmpty() && importDxfFile(path);
}

bool ImportTool::importText() {
    if (!m_project) return false;
    emit aboutToImport();
    QString path;
    if (m_chooser) path = m_chooser(Kind::Text);
    else path = QFileDialog::getOpenFileName(m_dialogParent, tr("Nombre del fichero"), QString(), textFilter());
    return !path.isEmpty() && importTextFile(path);
}

bool ImportTool::commit(const cc::core::ImportedGeometry& geometry, bool ungroup) {
    m_summary = {};
    // Building the command does not touch the project (it works on a copy): it only tells whether anything would be added.
    cc::core::ImportGeometryCommand probe(*m_project, geometry);
    const bool anything = probe.summary().nodesCreated + probe.summary().barsCreated + probe.summary().shellsCreated > 0;
    if (!anything) {
        m_summary = probe.summary();
        return false;
    }
    if (ungroup) m_undo->beginMacro("Importar geometría");
    if (ungroup) {
        cc::core::WorkPlanes planes = m_project->workPlanes;
        planes.groups = {};
        m_undo->push(std::make_unique<cc::core::EditWorkPlanesCommand>(*m_project, planes, cc::core::WorkPlanesEdit::Ungroup));
    }
    const bool done = cc::core::importGeometry(*m_project, *m_undo, geometry, &m_summary);
    if (ungroup) m_undo->endMacro();
    if (done) emit imported();
    return done;
}

bool ImportTool::importDxfFile(const QString& path) {
    if (!m_project || !m_undo) return false;
    m_summary = {};
    const QString title = tr("DXF o DWG");
    const cc::core::DxfReadResult probe = [&] {
        const std::filesystem::path p(path.toStdWString());
        // DWG: the error of the core ("formato no soportado") is shown at once.
        if (QFileInfo(path).suffix().compare(QStringLiteral("dwg"), Qt::CaseInsensitive) == 0 || !QFileInfo::exists(path)) return cc::core::readDxfFile(p);
        return cc::core::DxfReadResult{};
    }();
    if (!probe.errors.empty()) {
        emit messageRequested(title, messageOf(probe.errors.front()), true);
        return false;
    }
    std::ifstream in(std::filesystem::path(path.toStdWString()), std::ios::binary);
    std::ostringstream buffer;
    buffer << in.rdbuf();
    const std::string content = buffer.str();
    const std::vector<cc::core::DxfLayerInfo> layers = cc::core::dxfLayerInfo(content);
    if (layers.empty()) {
        // #61039 when the drawing is fine but has no layers with entities; #60948 when the file is not a DXF.
        const cc::core::DxfReadResult r = cc::core::readDxf(content);
        emit messageRequested(title, r.errors.empty() ? tr("Este dibujo no tiene capas definidas.") : messageOf(r.errors.front()), true);
        return false;
    }
    // [REA 0x1800011a0] the generation needs ungrouped planes: ask before the selection dialog.
    bool ungroup = false;
    if (hasGroupedPlanes()) {
        if (!confirm(tr("Se van a desagrupar todos los planos que intervienen en la generación.") + QStringLiteral("\n") + tr("¿Desea continuar?"))) return false;
        ungroup = true;
    }
    DxfSelectionDialog selection(content, layers, m_dialogParent);
    if (run(selection) != QDialog::Accepted) return false;
    cc::core::DxfOptions options;
    options.layers = selection.selectedLayers();
    options.panelLayers = selection.panelLayers();
    cc::core::DxfReadResult read = cc::core::readDxf(content, options);
    if (!read.ok()) {
        emit messageRequested(title, messageOf(read.errors.front()), true);
        return false;
    }
    // [REA 3.4] with data in the work the position is asked; an empty work takes the origin of the file.
    if (!m_project->structure.nodes.empty()) {
        ImportInsertionDialog where(m_dialogParent);
        if (run(where) != QDialog::Accepted) return false;
        if (where.useReferencePoint()) {
            const cc::core::Point3 offset = where.referencePoint();
            for (cc::core::ImportedNode& n : read.geometry.nodes) n.position = cc::core::geom::add(n.position, offset);
        }
    }
    if (!read.geometry.panels.empty()) {
        // The model has no load panels yet: the nodes that only belong to them are not created either.
        std::set<long long> used;
        for (const auto& b : read.geometry.bars) used.insert({b.startNumber, b.endNumber});
        for (const auto& sh : read.geometry.shells) used.insert(sh.nodeNumbers.begin(), sh.nodeNumbers.end());
        std::erase_if(read.geometry.nodes, [&used](const cc::core::ImportedNode& n) { return used.count(n.number) == 0; });
    }
    const bool done = commit(read.geometry, ungroup);
    for (const cc::core::ImportMessage& w : read.warnings) emit messageRequested(title, messageOf(w), false);
    if (m_summary.ignoredPanels > 0)
        emit messageRequested(title, tr("Los paños de carga todavía no existen en el modelo: se han ignorado %1.").arg(m_summary.ignoredPanels), false);
    if (done)
        emit statusChanged(tr("Importados %1 nudos, %2 barras y %3 láminas.").arg(m_summary.nodesCreated).arg(m_summary.barsCreated).arg(m_summary.shellsCreated));
    return done;
}

bool ImportTool::importTextFile(const QString& path) {
    if (!m_project || !m_undo) return false;
    m_summary = {};
    m_log.clear();
    const QString title = tr("Fichero de texto");
    if (!QFileInfo::exists(path)) {
        emit messageRequested(title, tr("No existe el fichero %1").arg(QDir::toNativeSeparators(path)), true);
        return false;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        emit messageRequested(title, tr("No existe el fichero %1").arg(QDir::toNativeSeparators(path)), true);
        return false;
    }
    const QByteArray bytes = file.readAll();
    // Log as in importar.md 3.5 (#106874..#106881).
    m_log << QStringLiteral("-> ") + tr("Lectura datos fichero texto") + QStringLiteral(" \"") + QDir::toNativeSeparators(path) + QStringLiteral("\"");
    m_log << QStringLiteral("   ") + tr("Cargando fichero") + QStringLiteral("...");
    const cc::core::TextImportResult r = cc::core::parseGeometryText(std::string(bytes.constData(), static_cast<std::size_t>(bytes.size())));
    m_log << QStringLiteral("   ") + tr("Leídas %1 líneas.").arg(r.linesRead);
    if (r.ok()) {
        m_log << QStringLiteral("-> ") + tr("Lectura sector nudos");
        if (!r.geometry.bars.empty()) m_log << QStringLiteral("-> ") + tr("Lectura sector barras");
        if (!r.geometry.shells.empty()) m_log << QStringLiteral("-> ") + tr("Lectura sector láminas");
        if (!r.geometry.panels.empty()) m_log << QStringLiteral("-> ") + tr("Lectura sector paños de carga");
        m_log << QStringLiteral("-> ") + tr("Fin lectura datos");
        m_log << fromStd(cc::core::importTotalsMessage(static_cast<long long>(r.geometry.nodes.size()), static_cast<long long>(r.geometry.bars.size())));
    } else {
        for (const cc::core::ImportMessage& e : r.errors) {
            QString line = QStringLiteral("!! ") + messageOf(e);
            if (e.line > 0) line += QStringLiteral("   ") + tr("Línea %1").arg(e.line);
            m_log << line;
        }
    }
    ImportLogDialog dialog(m_log, r.ok(), m_dialogParent);
    const bool yes = run(dialog) == QDialog::Accepted;
    if (!r.ok() || !yes) return false;
    const bool done = commit(r.geometry, false);
    if (m_summary.ignoredPanels > 0)
        emit messageRequested(title, tr("Los paños de carga todavía no existen en el modelo: se han ignorado %1.").arg(m_summary.ignoredPanels), false);
    if (done)
        emit statusChanged(tr("Importados %1 nudos, %2 barras y %3 láminas.").arg(m_summary.nodesCreated).arg(m_summary.barsCreated).arg(m_summary.shellsCreated));
    else
        emit messageRequested(title, tr("No se ha añadido ningún elemento: los datos del fichero ya existen en la obra."), false);
    return done;
}

}  // namespace cc::app
