#include "LayersPanel.h"

#include <QAbstractItemView>
#include <QButtonGroup>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace cc::app {

namespace {
constexpr int kIconSize = 18;
const QColor kBlue(40, 100, 200);
const QColor kLockColour(0x3a, 0x4a, 0x63);

QString fromStd(const std::string& s) { return QString::fromStdString(s); }

QPixmap newPixmap() {
    QPixmap pm(kIconSize, kIconSize);
    pm.fill(Qt::transparent);
    return pm;
}

/// Isometric cube: outline, or filled blue when `filled` (layer drawn rendered).
QIcon cubeIcon(bool filled) {
    QPixmap pm = newPixmap();
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    const QPointF c(9, 9);
    QPolygonF hex;
    hex << QPointF(9, 2) << QPointF(15.5, 5.5) << QPointF(15.5, 12.5) << QPointF(9, 16) << QPointF(2.5, 12.5)
        << QPointF(2.5, 5.5);
    p.setPen(QPen(filled ? kBlue.darker(130) : QColor(70, 70, 70), 1.2));
    p.setBrush(filled ? QBrush(kBlue) : Qt::NoBrush);
    p.drawPolygon(hex);
    p.drawLine(c, QPointF(2.5, 5.5));
    p.drawLine(c, QPointF(15.5, 5.5));
    p.drawLine(c, QPointF(9, 16));
    return QIcon(pm);
}

/// Eye, struck through when the layer is hidden.
QIcon eyeIcon(bool visible) {
    QPixmap pm = newPixmap();
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(70, 70, 70), 1.2));
    QPainterPath path;
    path.moveTo(1.5, 9);
    path.quadTo(9, 1, 16.5, 9);
    path.quadTo(9, 17, 1.5, 9);
    p.drawPath(path);
    p.setBrush(QColor(70, 70, 70));
    p.drawEllipse(QPointF(9, 9), 2.6, 2.6);
    if (!visible) {
        p.setPen(QPen(QColor(200, 30, 30), 1.8));
        p.drawLine(QPointF(2.5, 15.5), QPointF(15.5, 2.5));
    }
    return QIcon(pm);
}

/// Padlock, open or closed.
QIcon lockIcon(bool locked) {
    QPixmap pm = newPixmap();
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(kLockColour, 1.5));
    p.setBrush(Qt::NoBrush);
    if (locked) {
        p.drawArc(QRectF(5, 1.5, 8, 9), 0, 180 * 16);
        p.drawLine(QPointF(5, 6), QPointF(5, 8.5));
        p.drawLine(QPointF(13, 6), QPointF(13, 8.5));
    } else {
        p.drawArc(QRectF(8, 1.5, 8, 9), 0, 180 * 16);
        p.drawLine(QPointF(8, 6), QPointF(8, 8.5));
    }
    p.setBrush(kLockColour);
    p.drawRect(QRectF(3.5, 8.5, 11, 7.5));
    return QIcon(pm);
}

constexpr int kToolIconSize = 20;
constexpr int kToolButtonSize = 26;

/// Toolbar glyphs (plus, cross, up and down triangles) drawn at toolbar size.
enum class Glyph { Plus, Cross, Up, Down };

QIcon glyphIcon(Glyph glyph) {
    QPixmap pm(kToolIconSize, kToolIconSize);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    switch (glyph) {
        case Glyph::Plus:
            p.setPen(QPen(Qt::black, 3.0, Qt::SolidLine, Qt::FlatCap));
            p.drawLine(QPointF(10, 3.5), QPointF(10, 16.5));
            p.drawLine(QPointF(3.5, 10), QPointF(16.5, 10));
            break;
        case Glyph::Cross:
            p.setPen(QPen(QColor(205, 25, 25), 3.0, Qt::SolidLine, Qt::FlatCap));
            p.drawLine(QPointF(4.5, 4.5), QPointF(15.5, 15.5));
            p.drawLine(QPointF(15.5, 4.5), QPointF(4.5, 15.5));
            break;
        case Glyph::Up:
        case Glyph::Down: {
            p.setPen(Qt::NoPen);
            p.setBrush(Qt::black);
            QPolygonF tri;
            if (glyph == Glyph::Up)
                tri << QPointF(10, 4) << QPointF(16.5, 15) << QPointF(3.5, 15);
            else
                tri << QPointF(10, 16) << QPointF(16.5, 5) << QPointF(3.5, 5);
            p.drawPolygon(tri);
            break;
        }
    }
    return QIcon(pm);
}

/// Cursor over a dashed selection window: the "Asignar elementos a etiquetas" tool.
QIcon assignToolIcon() {
    QPixmap pm = newPixmap();
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen dashed(QColor(70, 70, 70), 1.2, Qt::DashLine);
    p.setPen(dashed);
    p.drawRect(QRectF(2, 2, 12, 12));
    p.setPen(QPen(QColor(40, 40, 40), 1));
    p.setBrush(kBlue);
    QPolygonF arrow;
    arrow << QPointF(8, 8) << QPointF(16, 11) << QPointF(12, 12) << QPointF(11, 16);
    p.drawPolygon(arrow);
    return QIcon(pm);
}

QIcon colorSwatch(unsigned rgb) {
    QPixmap pm(kIconSize + 18, kIconSize - 4);
    pm.fill(QColor::fromRgb(rgb));
    QPainter p(&pm);
    p.setPen(QColor(90, 90, 90));
    p.drawRect(pm.rect().adjusted(0, 0, -1, -1));
    return QIcon(pm);
}

class DialogPrompter : public LayersPrompter {
public:
    void warnLastLayer(QWidget* parent) override {
        QMessageBox box(QMessageBox::Warning, QObject::tr("Aviso"), QObject::tr("Debe definir, al menos, una capa."),
                        QMessageBox::NoButton, parent);
        box.addButton(QObject::tr("Aceptar"), QMessageBox::AcceptRole);
        box.exec();
    }
    bool confirmRemoval(QWidget* parent, const QString& layerName) override {
        QMessageBox box(QMessageBox::Question, QObject::tr("Pregunta"),
                        QObject::tr("¿Desea borrar la capa '%1'?").arg(layerName), QMessageBox::NoButton, parent);
        QPushButton* yes = box.addButton(QObject::tr("Sí"), QMessageBox::YesRole);
        box.addButton(QObject::tr("No"), QMessageBox::NoRole);
        box.exec();
        return box.clickedButton() == yes;
    }
    std::optional<std::pair<core::LayerRemoval, int>> chooseRemoval(QWidget* parent, const QString& layerName,
                                                                    const std::vector<core::Layer>& others) override {
        DeleteLayerDialog dialog(layerName, others, parent);
        if (dialog.exec() != QDialog::Accepted) return std::nullopt;
        return std::make_pair(dialog.removal(), dialog.destinationLayerId());
    }
    std::optional<unsigned> chooseColor(QWidget* parent, unsigned current) override {
        QColorDialog dialog(QColor::fromRgb(current), parent);
        dialog.setWindowTitle(QObject::tr("Selección del color"));
        dialog.setOption(QColorDialog::DontUseNativeDialog);
        if (auto* box = dialog.findChild<QDialogButtonBox*>()) {
            if (auto* ok = box->button(QDialogButtonBox::Ok)) ok->setText(QObject::tr("Aceptar"));
            if (auto* cancel = box->button(QDialogButtonBox::Cancel)) cancel->setText(QObject::tr("Cancelar"));
        }
        if (dialog.exec() != QDialog::Accepted) return std::nullopt;
        return dialog.selectedColor().rgb() & 0xFFFFFF;
    }
    std::optional<std::pair<core::LayerOperation, std::vector<int>>> chooseAssignment(
        QWidget* parent, const std::vector<core::Layer>& layers) override {
        AssignLayersDialog dialog(layers, parent);
        if (dialog.exec() != QDialog::Accepted) return std::nullopt;
        return std::make_pair(dialog.operation(), dialog.selectedLayerIds());
    }
};

LayersPrompter* defaultPrompter() {
    static DialogPrompter instance;
    return &instance;
}

void translateButtons(QDialogButtonBox* box) {
    box->button(QDialogButtonBox::Ok)->setText(QObject::tr("Aceptar"));
    box->button(QDialogButtonBox::Cancel)->setText(QObject::tr("Cancelar"));
}
}  // namespace

// ---------------------------------------------------------------- AssignLayersDialog

AssignLayersDialog::AssignLayersDialog(const std::vector<core::Layer>& layers, QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Asignar elementos a etiquetas"));
    auto* root = new QVBoxLayout(this);

    auto* operationBox = new QGroupBox(tr("Operación"), this);
    auto* operationLayout = new QHBoxLayout(operationBox);
    m_assign = new QRadioButton(tr("Asignar"), operationBox);
    m_replace = new QRadioButton(tr("Reemplazar"), operationBox);
    m_remove = new QRadioButton(tr("Eliminar"), operationBox);
    m_replace->setChecked(true);  // default in the original
    for (auto* r : {m_assign, m_replace, m_remove}) {
        operationLayout->addWidget(r);
        connect(r, &QRadioButton::toggled, this, [this] { updateOk(); });
    }
    root->addWidget(operationBox);

    auto* labelsBox = new QGroupBox(tr("Etiquetas"), this);
    auto* labelsLayout = new QVBoxLayout(labelsBox);
    m_table = new QTableWidget(static_cast<int>(layers.size()), 2, labelsBox);
    m_table->setHorizontalHeaderLabels({tr("Seleccionado"), tr("Referencia")});
    m_table->verticalHeader()->hide();
    m_table->horizontalHeader()->setStretchLastSection(true);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    for (int row = 0; row < static_cast<int>(layers.size()); ++row) {
        auto* check = new QTableWidgetItem;
        check->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        check->setCheckState(Qt::Unchecked);
        check->setData(Qt::UserRole, layers[row].id);
        m_table->setItem(row, 0, check);
        auto* name = new QTableWidgetItem(fromStd(layers[row].name));
        name->setFlags(Qt::ItemIsEnabled);
        m_table->setItem(row, 1, name);
    }
    connect(m_table, &QTableWidget::itemChanged, this, [this] { updateOk(); });
    labelsLayout->addWidget(m_table);
    root->addWidget(labelsBox, 1);

    m_buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    translateButtons(m_buttons);
    connect(m_buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(m_buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(m_buttons);
    updateOk();
}

core::LayerOperation AssignLayersDialog::operation() const {
    if (m_assign->isChecked()) return core::LayerOperation::Assign;
    if (m_remove->isChecked()) return core::LayerOperation::Remove;
    return core::LayerOperation::Replace;
}

void AssignLayersDialog::setOperation(core::LayerOperation operation) {
    (operation == core::LayerOperation::Assign   ? m_assign
     : operation == core::LayerOperation::Remove ? m_remove
                                                 : m_replace)
        ->setChecked(true);
}

std::vector<int> AssignLayersDialog::selectedLayerIds() const {
    std::vector<int> ids;
    for (int row = 0; row < m_table->rowCount(); ++row)
        if (m_table->item(row, 0)->checkState() == Qt::Checked) ids.push_back(m_table->item(row, 0)->data(Qt::UserRole).toInt());
    return ids;
}

void AssignLayersDialog::setLayerChecked(int layerId, bool checked) {
    for (int row = 0; row < m_table->rowCount(); ++row)
        if (m_table->item(row, 0)->data(Qt::UserRole).toInt() == layerId)
            m_table->item(row, 0)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}

void AssignLayersDialog::updateOk() {
    const bool needsLayer = operation() != core::LayerOperation::Remove;
    m_buttons->button(QDialogButtonBox::Ok)->setEnabled(!needsLayer || !selectedLayerIds().empty());
}

// ---------------------------------------------------------------- DeleteLayerDialog

DeleteLayerDialog::DeleteLayerDialog(const QString& layerName, const std::vector<core::Layer>& others, QWidget* parent)
    : QDialog(parent) {
    setWindowTitle(tr("Borrar capa"));
    auto* root = new QVBoxLayout(this);
    root->addWidget(new QLabel(tr("La capa '%1' tiene elementos asignados. ¿Qué desea hacer con ellos?").arg(layerName), this));
    m_move = new QRadioButton(tr("Mover los elementos a la capa:"), this);
    m_delete = new QRadioButton(tr("Eliminar los elementos asignados"), this);
    m_move->setChecked(true);
    m_combo = new QComboBox(this);
    for (const core::Layer& l : others) m_combo->addItem(fromStd(l.name), l.id);
    root->addWidget(m_move);
    root->addWidget(m_combo);
    root->addWidget(m_delete);
    connect(m_move, &QRadioButton::toggled, m_combo, &QWidget::setEnabled);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    translateButtons(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    root->addWidget(buttons);
}

core::LayerRemoval DeleteLayerDialog::removal() const {
    return m_delete->isChecked() ? core::LayerRemoval::DeleteElements : core::LayerRemoval::MoveElements;
}

void DeleteLayerDialog::setRemoval(core::LayerRemoval removal) {
    (removal == core::LayerRemoval::DeleteElements ? m_delete : m_move)->setChecked(true);
}

int DeleteLayerDialog::destinationLayerId() const { return m_combo->currentData().toInt(); }

void DeleteLayerDialog::setDestinationLayerId(int id) {
    const int index = m_combo->findData(id);
    if (index >= 0) m_combo->setCurrentIndex(index);
}

// ---------------------------------------------------------------- LayersPanel

LayersPanel::LayersPanel(QWidget* parent) : QWidget(parent), m_prompter(defaultPrompter()) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(2);

    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(2, 2, 2, 2);
    bar->setSpacing(2);
    auto makeButton = [&](const QString& objectName, const QIcon& icon, const QString& tip) {
        auto* b = new QToolButton(this);
        b->setObjectName(objectName);
        b->setIcon(icon);
        b->setIconSize(QSize(kToolIconSize, kToolIconSize));
        b->setFixedSize(kToolButtonSize, kToolButtonSize);
        b->setToolTip(tip);
        b->setAutoRaise(true);
        bar->addWidget(b);
        return b;
    };
    m_add = makeButton(QStringLiteral("layerAdd"), glyphIcon(Glyph::Plus), tr("Añadir / Añadir nuevo elemento a la lista"));
    m_remove = makeButton(QStringLiteral("layerRemove"), glyphIcon(Glyph::Cross),
                          tr("Borrar / Suprimir elemento seleccionado de la lista"));
    m_up = makeButton(QStringLiteral("layerUp"), glyphIcon(Glyph::Up),
                      tr("Mover un lugar hacia arriba el elemento seleccionado de la lista"));
    m_down = makeButton(QStringLiteral("layerDown"), glyphIcon(Glyph::Down),
                        tr("Mover un lugar hacia abajo el elemento seleccionado de la lista"));
    m_assignTool = makeButton(QStringLiteral("layerAssignTool"), assignToolIcon(), tr("Asignar elementos a etiquetas"));
    m_assignTool->setCheckable(true);
    bar->addStretch(1);
    root->addLayout(bar);

    m_table = new QTableWidget(0, ColumnCount, this);
    m_table->setObjectName(QStringLiteral("layersTable"));
    m_table->setHorizontalHeaderLabels({tr("Referencia"), tr("Activa"), tr("Color"), QString(), QString(), QString()});
    m_table->verticalHeader()->hide();
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked);
    m_table->setIconSize(QSize(kIconSize, kIconSize));
    m_table->setShowGrid(true);
    QHeaderView* header = m_table->horizontalHeader();
    header->setSectionResizeMode(ColReference, QHeaderView::Stretch);
    for (int c = ColActive; c < ColumnCount; ++c) header->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    root->addWidget(m_table, 1);

    connect(m_add, &QToolButton::clicked, this, [this] { addLayer(); });
    connect(m_remove, &QToolButton::clicked, this, [this] { removeSelectedLayer(); });
    connect(m_up, &QToolButton::clicked, this, [this] { moveSelectedLayer(-1); });
    connect(m_down, &QToolButton::clicked, this, [this] { moveSelectedLayer(1); });
    connect(m_assignTool, &QToolButton::toggled, this, &LayersPanel::assignToolToggled);
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int column) { toggleCell(row, column); });
    connect(m_table, &QTableWidget::cellChanged, this, [this](int row, int column) { onItemChanged(row, column); });
    refresh();
}

LayersPanel::~LayersPanel() = default;

void LayersPanel::setStructure(core::StructuralModel* model) {
    m_model = model;
    refresh();
}

void LayersPanel::setPrompter(LayersPrompter* prompter) { m_prompter = prompter ? prompter : defaultPrompter(); }

int LayersPanel::layerIdAtRow(int row) const {
    const QTableWidgetItem* item = m_table->item(row, ColReference);
    return item ? item->data(Qt::UserRole).toInt() : 0;
}

int LayersPanel::selectedLayerId() const {
    const auto rows = m_table->selectionModel()->selectedRows();
    return rows.isEmpty() ? 0 : layerIdAtRow(rows.first().row());
}

void LayersPanel::selectLayer(int id) {
    for (int row = 0; row < m_table->rowCount(); ++row) {
        if (layerIdAtRow(row) == id) {
            m_table->selectRow(row);
            return;
        }
    }
}

void LayersPanel::refresh() {
    const int selected = selectedLayerId();
    m_refreshing = true;
    const std::vector<core::Layer> none;
    const auto& layers = m_model ? m_model->layers : none;
    m_table->setRowCount(static_cast<int>(layers.size()));
    for (int row = 0; row < static_cast<int>(layers.size()); ++row) {
        const core::Layer& l = layers[row];
        auto ensure = [&](int column) {
            QTableWidgetItem* item = m_table->item(row, column);
            if (!item) {
                item = new QTableWidgetItem;
                m_table->setItem(row, column, item);
            }
            return item;
        };
        QTableWidgetItem* name = ensure(ColReference);
        name->setText(fromStd(l.name));
        name->setData(Qt::UserRole, l.id);
        name->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsEditable);
        QTableWidgetItem* active = ensure(ColActive);
        active->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
        active->setCheckState(l.active ? Qt::Checked : Qt::Unchecked);
        QTableWidgetItem* color = ensure(ColColor);
        color->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        color->setIcon(colorSwatch(l.color));
        color->setData(Qt::UserRole, l.color);
        QTableWidgetItem* rendered = ensure(ColRendered);
        rendered->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        rendered->setIcon(cubeIcon(l.rendered));
        QTableWidgetItem* visible = ensure(ColVisible);
        visible->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        visible->setIcon(eyeIcon(l.visible));
        QTableWidgetItem* locked = ensure(ColLocked);
        locked->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        locked->setIcon(lockIcon(l.locked));
    }
    m_refreshing = false;
    if (selected != 0) selectLayer(selected);
    const bool has = m_model != nullptr;
    m_add->setEnabled(has);
    m_remove->setEnabled(has);
    m_up->setEnabled(has);
    m_down->setEnabled(has);
}

void LayersPanel::changed() {
    refresh();
    emit layersChanged();
}

void LayersPanel::addLayer() {
    if (!m_model) return;
    const int id = m_model->addLayer(m_model->suggestedLayerName());
    if (id == 0) return;
    changed();
    selectLayer(id);
}

void LayersPanel::removeSelectedLayer() {
    const int id = selectedLayerId();
    if (!m_model || id == 0) return;
    if (m_model->layers.size() <= 1) {
        m_prompter->warnLastLayer(this);
        return;
    }
    const core::Layer* layer = m_model->findLayer(id);
    if (!layer) return;
    const QString name = fromStd(layer->name);
    bool done = false;
    if (m_model->layerUsage(id) == 0) {
        if (!m_prompter->confirmRemoval(this, name)) return;
        done = m_model->removeLayer(id, core::LayerRemoval::MoveElements, m_model->layers.front().id == id ? m_model->layers.back().id
                                                                                                           : m_model->layers.front().id);
    } else {
        std::vector<core::Layer> others;
        for (const core::Layer& l : m_model->layers)
            if (l.id != id) others.push_back(l);
        const auto choice = m_prompter->chooseRemoval(this, name, others);
        if (!choice) return;
        done = m_model->removeLayer(id, choice->first, choice->second);
    }
    if (done) changed();
}

void LayersPanel::moveSelectedLayer(int direction) {
    const int id = selectedLayerId();
    if (!m_model || id == 0) return;
    if (m_model->moveLayer(id, direction)) {
        changed();
        selectLayer(id);
    }
}

void LayersPanel::toggleCell(int row, int column) {
    if (!m_model || row < 0 || row >= m_table->rowCount()) return;
    core::Layer* layer = m_model->findLayer(layerIdAtRow(row));
    if (!layer) return;
    switch (column) {
        case ColRendered: layer->rendered = !layer->rendered; break;
        case ColVisible: layer->visible = !layer->visible; break;
        case ColLocked: layer->locked = !layer->locked; break;
        case ColColor: {
            const auto picked = m_prompter->chooseColor(this, layer->color);
            if (!picked) return;
            layer->color = *picked;
            break;
        }
        default: return;
    }
    changed();
}

void LayersPanel::onItemChanged(int row, int column) {
    if (m_refreshing || !m_model) return;
    const int id = layerIdAtRow(row);
    QTableWidgetItem* item = m_table->item(row, column);
    if (!item || id == 0) return;
    bool modelChanged = false;
    if (column == ColReference) {
        modelChanged = m_model->renameLayer(id, item->text().toStdString());
    } else if (column == ColActive) {
        modelChanged = item->checkState() == Qt::Checked && m_model->setActiveLayer(id);
    } else {
        return;
    }
    // In-place sync (no row rebuild), so it is safe while the item is still being committed.
    refresh();
    if (modelChanged) emit layersChanged();
}

bool LayersPanel::assignTo(const std::vector<int>& barIds, const std::vector<int>& shellIds) {
    if (!m_model) return false;
    const auto choice = m_prompter->chooseAssignment(this, m_model->layers);
    if (!choice) return false;
    if (!m_model->assignLayers(barIds, shellIds, choice->second, choice->first)) return false;
    emit layersChanged();
    return true;
}

}  // namespace cc::app
