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
#include <QStyle>
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
    t->verticalHeader()->setMinimumSectionSize(16);  // the Windows style imposes a larger minimum
    t->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    t->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    t->verticalHeader()->setDefaultSectionSize(20);
    t->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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

// "Aviso" message of the original (orig_describir_series_nueva_valid): red frame, warning icon, text, centred Aceptar.
class NoticeDialog : public CypeDialog {
public:
    NoticeDialog(const QString& text, QWidget* parent) : CypeDialog(QObject::tr("Aviso"), parent) {
        setFixedSize(367, 161);
        auto* row = new QHBoxLayout;
        row->setContentsMargins(14, 8, 14, 0);
        auto* icon = new QLabel(this);
        icon->setPixmap(style()->standardIcon(QStyle::SP_MessageBoxWarning).pixmap(40, 40));
        row->addWidget(icon);
        auto* label = new QLabel(text, this);
        label->setWordWrap(true);
        row->addWidget(label, 1);
        contentLayout()->addLayout(row);
        contentLayout()->addStretch(1);
        auto* footer = setupCustomFooter();
        auto* ok = new QPushButton(QObject::tr("Aceptar"), this);
        ok->setDefault(true);
        footer->addStretch(1);
        footer->addWidget(ok);
        footer->addStretch(1);
        connect(ok, &QPushButton::clicked, this, &QDialog::accept);
    }
};

}  // namespace

ProfileMessageSink& profileMessageSink() {
    static ProfileMessageSink sink{
        [](QWidget* parent, const QString&, const QString& text) { NoticeDialog(text, parent).exec(); },
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

ProfileEditDialog::ProfileEditDialog(Section initial, bool isNew, bool shapeChangeable, QWidget* parent)
    : CypeDialog(isNew ? tr("Nuevo perfil") : tr("Editar perfil"), parent), m_initial(std::move(initial)), m_kind(m_initial.kind) {
    setFixedSize(360, 520);
    m_form = new QFormLayout;
    m_form->setContentsMargins(4, 0, 4, 0);
    m_reference = new QLineEdit(q(m_initial.reference), this);
    m_form->addRow(tr("Referencia"), m_reference);
    m_shape = new QComboBox(this);
    for (ShapeKind kind : allShapes()) {
        if (kind == ShapeKind::Generic) continue;  // generic sections need tabulated values (TK-052)
        if (shapeFamily(kind)) continue;           // concrete / timber shapes have their own dialog (T-P5-06 GUI)
        m_shape->addItem(tr(shapeNameEs(kind)), static_cast<int>(kind));
        if (kind == m_kind) m_shape->setCurrentIndex(m_shape->count() - 1);
    }
    m_shape->setEnabled(shapeChangeable);
    m_form->addRow(tr("Forma"), m_shape);
    contentLayout()->addLayout(m_form);
    contentLayout()->addWidget(new QLabel(tr("Propiedades calculadas"), this));
    m_properties = new QLabel(this);
    m_properties->setObjectName(QStringLiteral("profileProperties"));
    m_properties->setFrameShape(QFrame::StyledPanel);
    m_properties->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_properties->setWordWrap(true);
    m_properties->setMinimumHeight(200);
    contentLayout()->addWidget(m_properties, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    buildDims(m_kind, m_initial.dims);
    connect(m_shape, &QComboBox::currentIndexChanged, this, [this] {
        m_kind = static_cast<ShapeKind>(m_shape->currentData().toInt());
        std::vector<double> dims(dimensionKeys(m_kind).size(), 0.0);
        const auto& keys = dimensionKeys(m_kind);
        for (std::size_t i = 0; i < keys.size(); ++i)
            if (isSlopeKey(keys[i])) dims[i] = m_kind == ShapeKind::ISloped ? kIpnSlopePercent : kUpnSlopePercent;
        buildDims(m_kind, dims);
    });
}

void ProfileEditDialog::buildDims(ShapeKind kind, const std::vector<double>& dims) {
    while (m_form->rowCount() > 2) m_form->removeRow(2);
    m_dims.clear();
    const auto& keys = dimensionKeys(kind);
    for (std::size_t i = 0; i < keys.size(); ++i) {
        auto* spin = new QDoubleSpinBox(this);
        const bool slope = isSlopeKey(keys[i]);
        spin->setDecimals(2);
        spin->setRange(0.0, 100000.0);
        spin->setSuffix(slope ? QStringLiteral(" %") : QStringLiteral(" mm"));
        const double stored = i < dims.size() ? dims[i] : 0.0;
        spin->setValue(slope ? stored : stored * 1000.0);
        connect(spin, &QDoubleSpinBox::valueChanged, this, [this] { refreshProperties(); });
        m_dims.append(spin);
        m_form->addRow(q(keys[i]), spin);
    }
    refreshProperties();
}

Section ProfileEditDialog::value() const {
    Section s = m_initial;
    s.kind = m_kind;
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

namespace {
// Flat tool button with the colour of the original's icon (the original uses pictograms; these are glyphs, TK-060).
QToolButton* flatButton(const QString& name, const QString& glyph, const QString& colour, const QString& tip, QWidget* parent) {
    auto* b = toolButton(name, glyph, tip, parent);
    b->setAutoRaise(true);
    b->setFixedSize(26, 24);
    b->setStyleSheet(QStringLiteral("QToolButton { border: none; background: transparent; color: %1; font-size: 16px; font-weight: bold; }"
                                    "QToolButton:hover { background: #E5F1FB; }"
                                    "QToolButton:disabled { color: #B5B5B5; }").arg(colour));
    return b;
}
const char* kTableStyle =
    "QTableWidget { background: white; border: 1px solid #A0A0A0; gridline-color: #E3E3E3; alternate-background-color: #F6F6F6; }"
    "QTableWidget::item { padding: 0px 2px; }"
    "QTableWidget::item:selected { background: #CCE8FF; color: black; }"
    "QTableView::indicator { width: 13px; height: 13px; }"
    "QHeaderView::section { background: white; border: none; border-right: 1px solid #E3E3E3; border-bottom: 1px solid #E3E3E3; padding: 2px 3px; }";
}  // namespace

SeriesEditDialog::SeriesEditDialog(ProfileSeries initial, bool isNew, QWidget* parent)
    : CypeDialog(tr("Series de Perfiles"), parent), m_series(std::move(initial)) {  // the original keeps the manager's title
    (void)isNew;
    setFixedSize(330, 421);
    auto* outer = new QHBoxLayout;
    auto* left = new QVBoxLayout;
    auto* form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setLabelAlignment(Qt::AlignLeft);
    m_reference = new QLineEdit(q(m_series.reference), this);
    form->addRow(tr("Referencia"), m_reference);
    left->addLayout(form);
    left->addWidget(new QLabel(tr("Descripción"), this));
    m_description = new QPlainTextEdit(q(m_series.description), this);
    m_description->setFixedHeight(42);
    left->addWidget(m_description);
    auto* row = new QHBoxLayout;
    struct Def { const char* name; const char* glyph; const char* colour; const char* tip; };
    const Def defs[] = {{"new", "+", "#202020", QT_TR_NOOP("Nuevo perfil")},      {"edit", "✎", "#D9A400", QT_TR_NOOP("Editar perfil")},
                        {"duplicate", "⧉", "#7F7F7F", QT_TR_NOOP("Duplicar perfil")}, {"delete", "✕", "#D0021B", QT_TR_NOOP("Borrar perfil")},
                        {"up", "▲", "#303030", QT_TR_NOOP("Subir")},               {"down", "▼", "#303030", QT_TR_NOOP("Bajar")}};
    for (const Def& d : defs) {
        auto* b = flatButton(QString::fromLatin1(d.name), QString::fromUtf8(d.glyph), QString::fromLatin1(d.colour), tr(d.tip), this);
        m_buttons.append(b);
        row->addWidget(b);
    }
    row->addStretch(1);
    left->addLayout(row);
    m_table = makeTable({tr("Perfiles")}, this);
    m_table->setStyleSheet(QString::fromLatin1(kTableStyle));
    left->addWidget(m_table, 1);
    outer->addLayout(left, 1);

    // Right column of the original: four pictogram buttons (meaning of the first two not measured, TK-060); the folder buttons
    // import / export the series as a file.
    auto* right = new QVBoxLayout;
    struct Side { const char* name; const char* glyph; const char* colour; const char* tip; bool on; };
    const Side sides[] = {{"catalogue", "I", "#8B1A1A", QT_TR_NOOP("Importar perfiles de un catálogo"), false},
                          {"cube", "⬢", "#C0392B", QT_TR_NOOP("Importar desde otra aplicación"), false},
                          {"importFile", "⤓", "#D9A400", QT_TR_NOOP("Importar la serie desde un fichero"), true},
                          {"exportFile", "⤒", "#D9A400", QT_TR_NOOP("Exportar la serie a un fichero"), true}};
    for (const Side& d : sides) {
        auto* b = flatButton(QString::fromLatin1(d.name), QString::fromUtf8(d.glyph), QString::fromLatin1(d.colour), tr(d.tip), this);
        b->setEnabled(d.on);
        m_buttons.append(b);
        right->addWidget(b);
    }
    right->addStretch(1);
    outer->addLayout(right);
    contentLayout()->addLayout(outer, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));

    connect(button(QStringLiteral("new")), &QToolButton::clicked, this, [this] { onNew(); });
    connect(button(QStringLiteral("edit")), &QToolButton::clicked, this, [this] { onEdit(); });
    connect(button(QStringLiteral("duplicate")), &QToolButton::clicked, this, [this] { duplicateProfile(selectedRow(m_table)); });
    connect(button(QStringLiteral("delete")), &QToolButton::clicked, this, [this] { removeProfile(selectedRow(m_table)); });
    connect(button(QStringLiteral("up")), &QToolButton::clicked, this, [this] { moveProfile(selectedRow(m_table), -1); });
    connect(button(QStringLiteral("down")), &QToolButton::clicked, this, [this] { moveProfile(selectedRow(m_table), +1); });
    connect(button(QStringLiteral("importFile")), &QToolButton::clicked, this, [this] {
        const QString path = profileFileSink().openFile(this);
        if (!path.isEmpty()) importFile(path);
    });
    connect(button(QStringLiteral("exportFile")), &QToolButton::clicked, this, [this] {
        const QString path = profileFileSink().saveFile(this);
        if (!path.isEmpty()) exportFile(path);
    });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this] { onEdit(); });
    refreshTable();
}

QToolButton* SeriesEditDialog::button(const QString& name) const {
    for (QToolButton* b : m_buttons)
        if (b->objectName() == name) return b;
    return nullptr;
}

void SeriesEditDialog::refreshTable() {
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
}

bool SeriesEditDialog::importFile(const QString& path) {
    if (!QFileInfo::exists(path)) {
        warn(this, tr("El fichero a importar '%1' no existe o no se puede leer").arg(path));
        return false;
    }
    const SeriesImport read = importSeriesFromFile(s8(path));
    if (!read.error.empty() || read.series.empty()) {
        warn(this, read.error.empty() ? tr("El fichero no contiene ninguna serie de perfiles.") : q(read.error));
        return false;
    }
    m_series = read.series.front();
    m_reference->setText(q(m_series.reference));
    m_description->setPlainText(q(m_series.description));
    refreshTable();
    return true;
}

bool SeriesEditDialog::exportFile(const QString& path) {
    syncFromFields();
    return exportSeriesFile(this, {m_series}, path);
}

void SeriesEditDialog::addProfile(const Section& profile) {
    if (m_series.profiles.empty()) m_series.kind = profile.kind;  // the first profile fixes the shape of the series
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
    const bool first = m_series.profiles.empty();
    if (!first) {
        // Start from the last profile: dimensions are rarely typed from zero.
        blank = m_series.profiles.back();
        blank.reference.clear();
        blank.tabulated = {};
    }
    ProfileEditDialog dialog(blank, true, first, this);
    if (dialog.exec() == QDialog::Accepted) addProfile(dialog.value());
}

void SeriesEditDialog::onEdit() {
    const int row = selectedRow(m_table);
    if (row < 0) return;
    ProfileEditDialog dialog(m_series.profiles[static_cast<std::size_t>(row)], false, m_series.profiles.size() == 1, this);
    if (dialog.exec() == QDialog::Accepted) {
        if (m_series.profiles.size() == 1) m_series.kind = dialog.value().kind;
        replaceProfile(row, dialog.value());
    }
}

void SeriesEditDialog::accept() {
    syncFromFields();
    // The original checks the profiles first: an empty form says "Debe introducir, al menos, un perfil" even without reference.
    const std::string problem = m_series.profiles.empty() ? "Debe introducir, al menos, un perfil en la serie." : validateSeries(m_series);
    if (!problem.empty()) {
        warn(this, q(problem));  // "Debe introducir, al menos, un perfil en la serie." (#131925)
        return;
    }
    CypeDialog::accept();
}

// --- SeriesManagerDialog -----------------------------------------------------------------------------------------------------

SeriesManagerDialog::SeriesManagerDialog(LibraryScratch& scratch, QWidget* parent)
    : CypeDialog(tr("Series de Perfiles"), parent), m_scratch(scratch), m_snapshot(scratch.library()) {
    resize(530, 520);
    auto* row = new QHBoxLayout;
    setMaximizable();  // the original window is resizable (maximise button in the title bar)
    struct Def { const char* name; const char* glyph; const char* colour; const char* tip; };
    const Def defs[] = {{"new", "+", "#202020", QT_TR_NOOP("Nueva serie")},         {"edit", "✎", "#D9A400", QT_TR_NOOP("Editar serie")},
                        {"duplicate", "⧉", "#7F7F7F", QT_TR_NOOP("Duplicar serie")}, {"delete", "✕", "#D0021B", QT_TR_NOOP("Borrar serie")},
                        {"up", "▲", "#303030", QT_TR_NOOP("Subir")},                 {"down", "▼", "#303030", QT_TR_NOOP("Bajar")},
                        {"import", "⤓", "#D9A400", QT_TR_NOOP("Importar series de perfiles desde un fichero")},
                        {"export", "⤒", "#D9A400", QT_TR_NOOP("Exportar series de perfiles a un fichero")}};
    for (const Def& d : defs) {
        auto* b = flatButton(QString::fromLatin1(d.name), QString::fromUtf8(d.glyph), QString::fromLatin1(d.colour), tr(d.tip), this);
        m_buttons.append(b);
        row->addWidget(b);
    }
    row->addStretch(1);
    contentLayout()->addLayout(row);
    m_table = makeTable({tr("Referencia"), tr("Catálogo"), tr("Descripción")}, this);
    m_table->setColumnWidth(0, 120);
    m_table->setColumnWidth(1, 60);
    m_table->setStyleSheet(QString::fromLatin1(kTableStyle));
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
    setCompactTitle();
    contentLayout()->setContentsMargins(7, 6, 7, 6);
    m_table = makeTable({tr("Importar"), tr("Nombre Biblioteca"), tr("Editar")}, this);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setColumnWidth(0, 56);
    m_table->setColumnWidth(1, 186);
    m_table->setStyleSheet(QString::fromLatin1(kTableStyle));
    m_table->verticalHeader()->setDefaultSectionSize(19);
    m_table->horizontalHeader()->setFixedHeight(22);
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
        // "Editar": meaning not measured; in the clone it opens the series manager (the original reaches it from "Describir").
        edit->setFlags(available ? Qt::ItemIsEnabled : Qt::NoItemFlags);
        if (available) edit->setToolTip(tr("Editar las series de perfiles"));
        m_table->setItem(r, 0, check);
        m_table->setItem(r, 1, label);
        m_table->setItem(r, 2, edit);
    }
    contentLayout()->addWidget(m_table, 1);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int column) {
        if (column == 2 && isAvailable(m_table->item(row, 1)->text())) openSeriesManager();
    });
}

void PredefinedImportDialog::openSeriesManager() {
    SeriesManagerDialog dialog(m_scratch, this);
    dialog.exec();
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
    setFixedSize(328, 224);
    struct Def { const char* name; const char* text; };
    const Def defs[] = {{"predefined", QT_TR_NOOP("Importación de series de perfiles predefinidas")},
                        {"catalogues", QT_TR_NOOP("Importar catálogos de fabricante")},
                        {"initial", QT_TR_NOOP("Importación inicial de series predefinidas de perfiles")},
                        {"export", QT_TR_NOOP("Exportar series de perfiles")}};
    auto* column = new QVBoxLayout;
    column->setSpacing(8);
    for (const Def& d : defs) {
        auto* b = new QPushButton(tr(d.text), this);
        b->setObjectName(QString::fromLatin1(d.name));
        b->setFixedHeight(25);
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

}  // namespace cc::app
