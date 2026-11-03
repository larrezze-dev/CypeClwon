#include "ProfileLibraryDialogs.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include "core/predefined_profiles.h"

namespace cc::app {

using namespace cc::core;

namespace {

QString q(const std::string& s) { return QString::fromStdString(s); }
std::string s8(const QString& s) { return s.toStdString(); }

// "Aviso" is the title the original uses for the validation message (orig_describir_series_nueva_valid).
void warn(QWidget* parent, const QString& text) { profileMessageSink().warn(parent, QObject::tr("Aviso"), text); }

QToolButton* toolButton(const QString& name, const QString& glyph, const QString& tip, QWidget* parent) {
    auto* b = new QToolButton(parent);
    b->setObjectName(name);
    b->setText(glyph);
    b->setToolTip(tip);
    b->setFixedSize(28, 26);
    b->setAutoRaise(false);
    return b;
}

QTableWidget* makeTable(const QStringList& headers, QWidget* parent) {
    auto* t = new QTableWidget(0, static_cast<int>(headers.size()), parent);
    t->setHorizontalHeaderLabels(headers);
    t->verticalHeader()->setVisible(false);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    t->setSelectionMode(QAbstractItemView::SingleSelection);
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->horizontalHeader()->setStretchLastSection(true);
    t->setWordWrap(false);
    t->setTextElideMode(Qt::ElideRight);
    t->verticalHeader()->setDefaultSectionSize(22);
    return t;
}

QTableWidgetItem* textItem(const QString& text) { return new QTableWidgetItem(text); }

int selectedRow(const QTableWidget* t) {
    const auto rows = t->selectionModel()->selectedRows();
    return rows.isEmpty() ? -1 : rows.first().row();
}

bool isSlopeKey(const std::string& key) { return key == "slope"; }

QString formatProperties(const Section& section) {
    const auto p = effectiveProperties(section);
    if (!p) {
        const std::string problem = validateSection(section);
        return problem.empty() ? QObject::tr("No se pueden calcular las propiedades.") : q(problem);
    }
    const auto line = [](const QString& name, double value, double scale, const QString& unit) {
        return QStringLiteral("%1 = %2 %3").arg(name, QString::number(value * scale, 'f', 2), unit);
    };
    QStringList lines;
    lines << line(QStringLiteral("A"), p->area, 1e4, QStringLiteral("cm²"))
          << line(QStringLiteral("Iy"), p->iy, 1e8, QStringLiteral("cm⁴"))
          << line(QStringLiteral("Iz"), p->iz, 1e8, QStringLiteral("cm⁴"))
          << line(QStringLiteral("It"), p->it, 1e8, QStringLiteral("cm⁴"))
          << line(QStringLiteral("Iw"), p->iw, 1e12, QStringLiteral("cm⁶"))
          << line(QStringLiteral("Wel,y"), p->welY, 1e6, QStringLiteral("cm³"))
          << line(QStringLiteral("Wel,z"), p->welZ, 1e6, QStringLiteral("cm³"))
          << line(QStringLiteral("Wpl,y"), p->wplY, 1e6, QStringLiteral("cm³"))
          << line(QStringLiteral("Wpl,z"), p->wplZ, 1e6, QStringLiteral("cm³"))
          << line(QStringLiteral("Avy"), p->avy, 1e4, QStringLiteral("cm²"))
          << line(QStringLiteral("Avz"), p->avz, 1e4, QStringLiteral("cm²"));
    return lines.join(QLatin1Char('\n'));
}

}  // namespace

ProfileMessageSink& profileMessageSink() {
    static ProfileMessageSink sink{
        [](QWidget* parent, const QString& title, const QString& text) { QMessageBox::warning(parent, title, text); },
        [](QWidget* parent, const QString& title, const QString& text, const QString& first, const QString& second) {
            QMessageBox box(QMessageBox::Question, title, text, QMessageBox::NoButton, parent);
            auto* a = box.addButton(first, QMessageBox::AcceptRole);
            box.addButton(second, QMessageBox::RejectRole);
            box.exec();
            return box.clickedButton() == a;
        }};
    return sink;
}

ProfileFileSink& profileFileSink() {
    static ProfileFileSink sink{
        [](QWidget* parent) {
            return QFileDialog::getOpenFileName(parent, QObject::tr("Importar series de perfiles"), QString(),
                                                QObject::tr("Series de perfiles (*.json)"));
        },
        [](QWidget* parent) {
            return QFileDialog::getSaveFileName(parent, QObject::tr("Exportar series de perfiles"), QString(),
                                                QObject::tr("Series de perfiles (*.json)"));
        }};
    return sink;
}

bool importSeriesFile(QWidget* parent, LibraryScratch& scratch, const QString& path, int* imported) {
    if (imported) *imported = 0;
    if (!QFileInfo::exists(path)) {  // #75062
        warn(parent, QObject::tr("El fichero a importar '%1' no existe o no se puede leer").arg(path));
        return false;
    }
    const SeriesImport read = importSeriesFromFile(s8(path));
    if (!read.error.empty()) {
        warn(parent, q(read.error));
        return false;
    }
    scratch.undo.beginMacro("Importar series de perfiles");
    int total = 0;
    for (const ProfileSeries& incoming : read.series) {
        ImportPolicy policy = ImportPolicy::KeepExisting;
        if (scratch.library().find(incoming.reference)) {
            const bool keep = profileMessageSink().ask(
                parent, QObject::tr("Importación de series de perfiles"),
                QObject::tr("La serie predefinida de perfiles '%1' que se está importando ya existe en la biblioteca de perfiles "
                            "de la obra. Indique lo que desea hacer:").arg(q(incoming.reference)),
                QObject::tr("Conservar la serie existente"), QObject::tr("Sustituir la serie existente por la serie importada"));
            if (keep) continue;
            policy = ImportPolicy::ReplaceExisting;
        }
        int n = 0;
        const std::string error = importSeries(scratch.project, scratch.undo, {incoming}, policy, &n);
        if (!error.empty()) {
            scratch.undo.endMacro();
            warn(parent, q(error));
            return false;
        }
        total += n;
    }
    scratch.undo.endMacro();
    if (imported) *imported = total;
    return true;
}

bool exportSeriesFile(QWidget* parent, const std::vector<ProfileSeries>& series, const QString& path) {
    if (series.empty()) {
        warn(parent, QObject::tr("Debe seleccionar, al menos, una serie de perfiles."));  // #33818
        return false;
    }
    bool overwrite = false;
    if (QFileInfo::exists(path)) {  // #138928
        if (!profileMessageSink().ask(parent, QObject::tr("Exportar series de perfiles"),
                                      QObject::tr("El fichero %1 ya existe. ¿Desea sobrescribirlo?").arg(path),
                                      QObject::tr("Sí"), QObject::tr("No")))
            return false;
        overwrite = true;
    }
    const std::string error = exportSeriesToFile(s8(path), series, overwrite);
    if (!error.empty()) {
        warn(parent, q(error));
        return false;
    }
    return true;
}

// --- ProfileEditDialog -------------------------------------------------------------------------------------------------------

ProfileEditDialog::ProfileEditDialog(Section initial, bool isNew, QWidget* parent)
    : CypeDialog(isNew ? tr("Nuevo perfil") : tr("Editar perfil"), parent), m_initial(std::move(initial)) {
    setFixedSize(360, 520);
    auto* form = new QFormLayout;
    form->setContentsMargins(4, 0, 4, 0);
    m_reference = new QLineEdit(q(m_initial.reference), this);
    form->addRow(tr("Referencia"), m_reference);
    form->addRow(tr("Forma"), new QLabel(tr(shapeNameEs(m_initial.kind)), this));
    const auto& keys = dimensionKeys(m_initial.kind);
    for (std::size_t i = 0; i < keys.size(); ++i) {
        auto* spin = new QDoubleSpinBox(this);
        const bool slope = isSlopeKey(keys[i]);
        spin->setDecimals(2);
        spin->setRange(0.0, 100000.0);
        spin->setSuffix(slope ? QStringLiteral(" %") : QStringLiteral(" mm"));
        const double stored = i < m_initial.dims.size() ? m_initial.dims[i] : 0.0;
        spin->setValue(slope ? stored : stored * 1000.0);
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this] { refreshProperties(); });
        m_dims.append(spin);
        form->addRow(q(keys[i]), spin);
    }
    contentLayout()->addLayout(form);
    contentLayout()->addWidget(new QLabel(tr("Propiedades calculadas"), this));
    m_properties = new QLabel(this);
    m_properties->setObjectName(QStringLiteral("profileProperties"));
    m_properties->setFrameShape(QFrame::StyledPanel);
    m_properties->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_properties->setWordWrap(true);
    m_properties->setMinimumHeight(200);
    contentLayout()->addWidget(m_properties, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    refreshProperties();
}

Section ProfileEditDialog::value() const {
    Section s = m_initial;
    s.reference = s8(m_reference->text().trimmed());
    const auto& keys = dimensionKeys(s.kind);
    s.dims.clear();
    for (int i = 0; i < m_dims.size(); ++i) {
        const double v = m_dims[i]->value();
        s.dims.push_back(isSlopeKey(keys[static_cast<std::size_t>(i)]) ? v : v / 1000.0);
    }
    return s;
}

void ProfileEditDialog::refreshProperties() { m_properties->setText(formatProperties(value())); }

void ProfileEditDialog::accept() {
    const Section s = value();
    if (s.reference.empty()) {
        warn(this, tr("Debe introducir una referencia para el perfil."));  // #131966
        return;
    }
    const std::string problem = validateSection(s);
    if (!problem.empty()) {
        warn(this, q(problem));
        return;
    }
    CypeDialog::accept();
}

// --- SeriesEditDialog --------------------------------------------------------------------------------------------------------

SeriesEditDialog::SeriesEditDialog(ProfileSeries initial, bool isNew, QWidget* parent)
    : CypeDialog(isNew ? tr("Nueva serie") : tr("Editar serie"), parent), m_series(std::move(initial)) {
    setFixedSize(330, 421);
    auto* form = new QFormLayout;
    form->setContentsMargins(4, 0, 4, 0);
    m_reference = new QLineEdit(q(m_series.reference), this);
    form->addRow(tr("Referencia"), m_reference);
    m_description = new QPlainTextEdit(q(m_series.description), this);
    m_description->setFixedHeight(60);
    form->addRow(tr("Descripción"), m_description);
    // PROVISIONAL: the original chooses the shape through the material / shape icons of "Describir".
    m_shape = new QComboBox(this);
    for (ShapeKind kind : allShapes()) {
        if (kind == ShapeKind::Generic) continue;  // generic sections need tabulated values (TK-052)
        m_shape->addItem(tr(shapeNameEs(kind)), static_cast<int>(kind));
        if (kind == m_series.kind) m_shape->setCurrentIndex(m_shape->count() - 1);
    }
    form->addRow(tr("Forma"), m_shape);
    contentLayout()->addLayout(form);

    contentLayout()->addWidget(new QLabel(tr("Perfiles"), this));
    auto* row = new QHBoxLayout;
    struct Def { const char* name; const char* glyph; const char* tip; };
    const Def defs[] = {{"new", "+", QT_TR_NOOP("Nuevo perfil")},      {"edit", "✎", QT_TR_NOOP("Editar perfil")},
                        {"duplicate", "⧉", QT_TR_NOOP("Duplicar perfil")}, {"delete", "✕", QT_TR_NOOP("Borrar perfil")},
                        {"up", "▲", QT_TR_NOOP("Subir")},               {"down", "▼", QT_TR_NOOP("Bajar")}};
    for (const Def& d : defs) {
        auto* b = toolButton(QString::fromLatin1(d.name), QString::fromUtf8(d.glyph), tr(d.tip), this);
        m_buttons.append(b);
        row->addWidget(b);
    }
    row->addStretch(1);
    contentLayout()->addLayout(row);
    m_table = makeTable({tr("Referencia")}, this);
    contentLayout()->addWidget(m_table, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));

    connect(button(QStringLiteral("new")), &QToolButton::clicked, this, [this] { onNew(); });
    connect(button(QStringLiteral("edit")), &QToolButton::clicked, this, [this] { onEdit(); });
    connect(button(QStringLiteral("duplicate")), &QToolButton::clicked, this, [this] { duplicateProfile(selectedRow(m_table)); });
    connect(button(QStringLiteral("delete")), &QToolButton::clicked, this, [this] { removeProfile(selectedRow(m_table)); });
    connect(button(QStringLiteral("up")), &QToolButton::clicked, this, [this] { moveProfile(selectedRow(m_table), -1); });
    connect(button(QStringLiteral("down")), &QToolButton::clicked, this, [this] { moveProfile(selectedRow(m_table), +1); });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this] { onEdit(); });
    refreshTable();
}

QToolButton* SeriesEditDialog::button(const QString& name) const {
    for (QToolButton* b : m_buttons)
        if (b->objectName() == name) return b;
    return nullptr;
}

void SeriesEditDialog::refreshTable() {
    const bool noProfiles = m_series.profiles.empty();
    m_shape->setEnabled(noProfiles);  // every profile of a series has the shape of the series
    m_table->setRowCount(0);
    for (const Section& p : m_series.profiles) {
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        m_table->setItem(r, 0, textItem(q(p.reference)));
    }
}

void SeriesEditDialog::syncFromFields() {
    m_series.reference = s8(m_reference->text().trimmed());
    m_series.description = s8(m_description->toPlainText().trimmed());
    m_series.kind = static_cast<ShapeKind>(m_shape->currentData().toInt());
}

void SeriesEditDialog::addProfile(const Section& profile) {
    m_series.profiles.push_back(profile);
    refreshTable();
    m_table->selectRow(m_table->rowCount() - 1);
}

void SeriesEditDialog::replaceProfile(int row, const Section& profile) {
    if (row < 0 || row >= static_cast<int>(m_series.profiles.size())) return;
    m_series.profiles[static_cast<std::size_t>(row)] = profile;
    refreshTable();
    m_table->selectRow(row);
}

void SeriesEditDialog::duplicateProfile(int row) {
    if (row < 0 || row >= static_cast<int>(m_series.profiles.size())) return;
    Section copy = m_series.profiles[static_cast<std::size_t>(row)];
    std::string name = copy.reference + " (copia)";
    for (int n = 2; m_series.findProfile(name); ++n) name = copy.reference + " (copia " + std::to_string(n) + ")";
    copy.reference = name;
    m_series.profiles.insert(m_series.profiles.begin() + row + 1, std::move(copy));
    refreshTable();
    m_table->selectRow(row + 1);
}

void SeriesEditDialog::removeProfile(int row) {
    if (row < 0 || row >= static_cast<int>(m_series.profiles.size())) return;
    m_series.profiles.erase(m_series.profiles.begin() + row);
    refreshTable();
    if (m_table->rowCount() > 0) m_table->selectRow(std::min(row, m_table->rowCount() - 1));
}

void SeriesEditDialog::moveProfile(int row, int direction) {
    const int to = row + direction;
    const int n = static_cast<int>(m_series.profiles.size());
    if (row < 0 || row >= n || to < 0 || to >= n) return;
    std::swap(m_series.profiles[static_cast<std::size_t>(row)], m_series.profiles[static_cast<std::size_t>(to)]);
    refreshTable();
    m_table->selectRow(to);
}

void SeriesEditDialog::onNew() {
    syncFromFields();
    Section blank;
    blank.kind = m_series.kind;
    blank.dims.assign(dimensionKeys(blank.kind).size(), 0.0);
    // Start from the last profile when there is one: dimensions are rarely typed from zero.
    if (!m_series.profiles.empty()) {
        blank = m_series.profiles.back();
        blank.reference.clear();
        blank.tabulated = {};
    } else {
        const auto& keys = dimensionKeys(blank.kind);
        for (std::size_t i = 0; i < keys.size(); ++i)
            if (isSlopeKey(keys[i])) blank.dims[i] = blank.kind == ShapeKind::ISloped ? kIpnSlopePercent : kUpnSlopePercent;
    }
    ProfileEditDialog dialog(blank, true, this);
    if (dialog.exec() == QDialog::Accepted) addProfile(dialog.value());
}

void SeriesEditDialog::onEdit() {
    const int row = selectedRow(m_table);
    if (row < 0) return;
    ProfileEditDialog dialog(m_series.profiles[static_cast<std::size_t>(row)], false, this);
    if (dialog.exec() == QDialog::Accepted) replaceProfile(row, dialog.value());
}

void SeriesEditDialog::accept() {
    syncFromFields();
    const std::string problem = validateSeries(m_series);
    if (!problem.empty()) {
        warn(this, q(problem));  // "Debe introducir, al menos, un perfil en la serie." (#131925)
        return;
    }
    CypeDialog::accept();
}

// --- SeriesManagerDialog -----------------------------------------------------------------------------------------------------

SeriesManagerDialog::SeriesManagerDialog(LibraryScratch& scratch, QWidget* parent)
    : CypeDialog(tr("Series de Perfiles"), parent), m_scratch(scratch), m_snapshot(scratch.library()) {
    setFixedSize(530, 520);
    auto* row = new QHBoxLayout;
    struct Def { const char* name; const char* glyph; const char* tip; };
    const Def defs[] = {{"new", "+", QT_TR_NOOP("Nueva serie")},         {"edit", "✎", QT_TR_NOOP("Editar serie")},
                        {"duplicate", "⧉", QT_TR_NOOP("Duplicar serie")}, {"delete", "✕", QT_TR_NOOP("Borrar serie")},
                        {"up", "▲", QT_TR_NOOP("Subir")},                 {"down", "▼", QT_TR_NOOP("Bajar")},
                        {"import", "⤓", QT_TR_NOOP("Importar series de perfiles desde un fichero")},
                        {"export", "⤒", QT_TR_NOOP("Exportar series de perfiles a un fichero")}};
    for (const Def& d : defs) {
        auto* b = toolButton(QString::fromLatin1(d.name), QString::fromUtf8(d.glyph), tr(d.tip), this);
        m_buttons.append(b);
        row->addWidget(b);
    }
    row->addStretch(1);
    contentLayout()->addLayout(row);
    m_table = makeTable({tr("Referencia"), tr("Catálogo"), tr("Descripción")}, this);
    m_table->setColumnWidth(0, 110);
    m_table->setColumnWidth(1, 110);
    contentLayout()->addWidget(m_table, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));

    connect(button(QStringLiteral("new")), &QToolButton::clicked, this, [this] { onNew(); });
    connect(button(QStringLiteral("edit")), &QToolButton::clicked, this, [this] { onEdit(); });
    connect(button(QStringLiteral("duplicate")), &QToolButton::clicked, this, [this] { duplicateSeries(currentRow()); });
    connect(button(QStringLiteral("delete")), &QToolButton::clicked, this, [this] { removeSeries(currentRow()); });
    connect(button(QStringLiteral("up")), &QToolButton::clicked, this, [this] { moveSeries(currentRow(), -1); });
    connect(button(QStringLiteral("down")), &QToolButton::clicked, this, [this] { moveSeries(currentRow(), +1); });
    connect(button(QStringLiteral("import")), &QToolButton::clicked, this, [this] { onImport(); });
    connect(button(QStringLiteral("export")), &QToolButton::clicked, this, [this] { onExport(); });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this] { onEdit(); });
    refreshTable();
}

QToolButton* SeriesManagerDialog::button(const QString& name) const {
    for (QToolButton* b : m_buttons)
        if (b->objectName() == name) return b;
    return nullptr;
}

int SeriesManagerDialog::currentRow() const { return selectedRow(m_table); }

void SeriesManagerDialog::refreshTable(int selectRow) {
    m_table->setRowCount(0);
    for (const ProfileSeries& s : m_scratch.library().series) {
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        m_table->setItem(r, 0, textItem(q(s.reference)));
        m_table->setItem(r, 1, textItem(q(s.source)));
        m_table->setItem(r, 2, textItem(q(s.description)));
    }
    if (selectRow >= 0 && selectRow < m_table->rowCount()) m_table->selectRow(selectRow);
}

bool SeriesManagerDialog::report(const std::string& error) {
    if (error.empty()) return true;
    warn(this, q(error));
    return false;
}

bool SeriesManagerDialog::newSeries(const ProfileSeries& series) {
    const bool ok = report(addSeries(m_scratch.project, m_scratch.undo, series));
    refreshTable(ok ? m_table->rowCount() : -1);
    return ok;
}

bool SeriesManagerDialog::editSeries(int row, const ProfileSeries& series) {
    if (row < 0 || row >= static_cast<int>(m_scratch.library().series.size())) return false;
    const std::string ref = m_scratch.library().series[static_cast<std::size_t>(row)].reference;
    const bool ok = report(cc::core::editSeries(m_scratch.project, m_scratch.undo, ref, series));
    refreshTable(row);
    return ok;
}

bool SeriesManagerDialog::duplicateSeries(int row) {
    if (row < 0 || row >= static_cast<int>(m_scratch.library().series.size())) return false;
    const std::string ref = m_scratch.library().series[static_cast<std::size_t>(row)].reference;
    const bool ok = report(cc::core::duplicateSeries(m_scratch.project, m_scratch.undo, ref));
    refreshTable(ok ? m_table->rowCount() - 1 : row);
    return ok;
}

bool SeriesManagerDialog::removeSeries(int row) {
    if (row < 0 || row >= static_cast<int>(m_scratch.library().series.size())) return false;
    // TK-057: whether a bar uses the series is not checked until the bars reference the library (T-P5-07).
    const std::string ref = m_scratch.library().series[static_cast<std::size_t>(row)].reference;
    const bool ok = report(cc::core::removeSeries(m_scratch.project, m_scratch.undo, ref));
    refreshTable(std::min(row, m_table->rowCount() - 1));
    return ok;
}

bool SeriesManagerDialog::moveSeries(int row, int direction) {
    if (row < 0 || row >= static_cast<int>(m_scratch.library().series.size())) return false;
    const std::string ref = m_scratch.library().series[static_cast<std::size_t>(row)].reference;
    const bool ok = cc::core::moveSeries(m_scratch.project, m_scratch.undo, ref, direction) .empty();  // refused at the ends: silent
    refreshTable(ok ? row + direction : row);
    return ok;
}

bool SeriesManagerDialog::importFile(const QString& path) {
    const bool ok = importSeriesFile(this, m_scratch, path);
    refreshTable();
    return ok;
}

bool SeriesManagerDialog::exportFile(const QString& path) {
    const int row = currentRow();
    std::vector<ProfileSeries> chosen;
    if (row >= 0) chosen.push_back(m_scratch.library().series[static_cast<std::size_t>(row)]);
    else chosen = m_scratch.library().series;
    return exportSeriesFile(this, chosen, path);
}

void SeriesManagerDialog::onNew() {
    ProfileSeries blank;
    blank.reference = "";
    SeriesEditDialog dialog(blank, true, this);
    if (dialog.exec() == QDialog::Accepted) newSeries(dialog.value());
}

void SeriesManagerDialog::onEdit() {
    const int row = currentRow();
    if (row < 0) return;
    SeriesEditDialog dialog(m_scratch.library().series[static_cast<std::size_t>(row)], false, this);
    if (dialog.exec() == QDialog::Accepted) editSeries(row, dialog.value());
}

void SeriesManagerDialog::onImport() {
    const QString path = profileFileSink().openFile(this);
    if (!path.isEmpty()) importFile(path);
}

void SeriesManagerDialog::onExport() {
    const QString path = profileFileSink().saveFile(this);
    if (!path.isEmpty()) exportFile(path);
}

void SeriesManagerDialog::reject() {
    m_scratch.project.profileLibrary = m_snapshot;  // Cancelar discards what this dialog changed
    CypeDialog::reject();
}

// --- PredefinedImportDialog --------------------------------------------------------------------------------------------------

const QStringList& PredefinedImportDialog::libraryNames() {
    static const QStringList names = {
        QStringLiteral("Aluminio"), QStringLiteral("Gama de perfiles europeos"), QStringLiteral("Canada"), QStringLiteral("Gost"),
        QStringLiteral("Instituto argentino de Siderurgia"), QStringLiteral("Imca"), QStringLiteral("Indian standard"),
        QStringLiteral("Aisc.Lrfd (mks)"), QStringLiteral("Aisc.Lrfd (us)"), QStringLiteral("Madera"), QStringLiteral("Nbe-ea95"),
        QStringLiteral("SAISC"), QStringLiteral("Tabelas Técnicas"), QStringLiteral("TecnoMetal (esp)"),
        QStringLiteral("TecnoMetal (ita)"), QStringLiteral("TecnoMetal (bra)"), QStringLiteral("Ukprofiles"),
        QStringLiteral("Korean Standard"), QStringLiteral("Japanese Sections"), QStringLiteral("NHBC UK (Timber)"),
        QStringLiteral("SYS Thai Standard"), QStringLiteral("ГОСТ"), QStringLiteral("Australia")};
    return names;
}

bool PredefinedImportDialog::isAvailable(const QString& libraryName) {
    return libraryName == QStringLiteral("Gama de perfiles europeos");
}

PredefinedImportDialog::PredefinedImportDialog(LibraryScratch& scratch, QWidget* parent)
    : CypeDialog(tr("Importación de series de perfiles predefinidas"), parent), m_scratch(scratch) {
    setFixedSize(312, 555);
    m_table = makeTable({tr("Importar"), tr("Nombre Biblioteca"), tr("Editar")}, this);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setColumnWidth(0, 55);
    m_table->setColumnWidth(1, 160);
    for (const QString& name : libraryNames()) {
        const int r = m_table->rowCount();
        m_table->insertRow(r);
        const bool available = isAvailable(name);
        auto* check = new QTableWidgetItem;
        check->setCheckState(Qt::Unchecked);
        auto* label = textItem(name);
        auto* edit = textItem(QString());
        if (!available) {
            check->setFlags(check->flags() & ~Qt::ItemIsEnabled & ~Qt::ItemIsUserCheckable);
            label->setFlags(label->flags() & ~Qt::ItemIsEnabled);
            label->setToolTip(tr("Esta biblioteca aún no está disponible."));  // TK-052
        } else {
            check->setFlags((check->flags() | Qt::ItemIsUserCheckable) & ~Qt::ItemIsEditable);
        }
        edit->setFlags(Qt::NoItemFlags);  // "Editar": meaning not measured (TK-053)
        m_table->setItem(r, 0, check);
        m_table->setItem(r, 1, label);
        m_table->setItem(r, 2, edit);
    }
    contentLayout()->addWidget(m_table, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

bool PredefinedImportDialog::importChecked() {
    std::vector<ProfileSeries> chosen;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        if (m_table->item(r, 0)->checkState() != Qt::Checked) continue;
        if (isAvailable(m_table->item(r, 1)->text()))
            for (const ProfileSeries& s : predefinedSeries()) chosen.push_back(s);
    }
    if (chosen.empty()) {
        warn(this, tr("Debe seleccionar, al menos, una serie de perfiles."));  // #33818
        return false;
    }
    m_scratch.undo.beginMacro("Importar series de perfiles");
    bool ok = true;
    for (const ProfileSeries& incoming : chosen) {
        ImportPolicy policy = ImportPolicy::KeepExisting;
        if (m_scratch.library().find(incoming.reference)) {
            const bool keep = profileMessageSink().ask(
                this, tr("Importación de series de perfiles"),
                tr("La serie predefinida de perfiles '%1' que se está importando ya existe en la biblioteca de perfiles de la obra. "
                   "Indique lo que desea hacer:").arg(q(incoming.reference)),
                tr("Conservar la serie existente"), tr("Sustituir la serie existente por la serie importada"));
            if (keep) continue;
            policy = ImportPolicy::ReplaceExisting;
        }
        const std::string error = importSeries(m_scratch.project, m_scratch.undo, {incoming}, policy);
        if (!error.empty()) {
            warn(this, q(error));
            ok = false;
            break;
        }
    }
    m_scratch.undo.endMacro();
    return ok;
}

void PredefinedImportDialog::accept() {
    if (importChecked()) CypeDialog::accept();
}

// --- ProfileLibraryDialog ----------------------------------------------------------------------------------------------------

ProfileLibraryDialog::ProfileLibraryDialog(const ProfileLibrary& library, QWidget* parent)
    : CypeDialog(tr("Gestión de la biblioteca de perfiles"), parent), m_scratch(library) {
    setFixedSize(328, 254);  // the original is 328x224 with four buttons; the provisional fifth adds 30 px
    struct Def { const char* name; const char* text; };
    const Def defs[] = {{"predefined", QT_TR_NOOP("Importación de series de perfiles predefinidas")},
                        {"catalogues", QT_TR_NOOP("Importar catálogos de fabricante")},
                        {"initial", QT_TR_NOOP("Importación inicial de series predefinidas de perfiles")},
                        {"export", QT_TR_NOOP("Exportar series de perfiles")},
                        {"manager", QT_TR_NOOP("Series de perfiles...")}};  // PROVISIONAL, see header
    auto* column = new QVBoxLayout;
    column->setSpacing(4);
    for (const Def& d : defs) {
        auto* b = new QPushButton(tr(d.text), this);
        b->setObjectName(QString::fromLatin1(d.name));
        b->setFixedHeight(26);
        b->setAutoDefault(false);
        b->setStyleSheet(QStringLiteral("padding: 4px 4px;"));
        m_buttons.append(b);
        column->addWidget(b);
    }
    contentLayout()->addLayout(column);
    contentLayout()->addStretch(1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    connect(button(QStringLiteral("predefined")), &QPushButton::clicked, this, [this] { openPredefined(); });
    connect(button(QStringLiteral("catalogues")), &QPushButton::clicked, this, [this] { openManufacturerCatalogues(); });
    connect(button(QStringLiteral("initial")), &QPushButton::clicked, this, [this] { openInitialImport(); });
    connect(button(QStringLiteral("export")), &QPushButton::clicked, this, [this] { exportSeries(); });
    connect(button(QStringLiteral("manager")), &QPushButton::clicked, this, [this] { openSeriesManager(); });
}

QPushButton* ProfileLibraryDialog::button(const QString& name) const {
    for (QPushButton* b : m_buttons)
        if (b->objectName() == name) return b;
    return nullptr;
}

void ProfileLibraryDialog::openPredefined() {
    PredefinedImportDialog dialog(m_scratch, this);
    dialog.exec();
}

void ProfileLibraryDialog::openManufacturerCatalogues() {
    // Needs the manufacturers' servers (download column of the original): not reproducible offline (TK-058).
    warn(this, tr("Los catálogos de fabricante requieren conexión a Internet: esta función no está disponible."));
}

void ProfileLibraryDialog::openInitialImport() {
    // What the button does with "series chosen when creating a new work" is not measured (TK-058).
    warn(this, tr("Esta función aún no está disponible."));
}

void ProfileLibraryDialog::exportSeries() {
    const QString path = profileFileSink().saveFile(this);
    if (!path.isEmpty()) exportSeriesFile(this, m_scratch.library().series, path);
}

void ProfileLibraryDialog::openSeriesManager() {
    SeriesManagerDialog dialog(m_scratch, this);
    dialog.exec();
}

}  // namespace cc::app
