#include "ImportDialogs.h"

#include <QColor>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
#include <limits>

namespace cc::app {

namespace {
constexpr int kColTick = 0, kColName = 1, kColColour = 2, kColPanels = 3;
}

// ---- DxfPreviewWidget ----------------------------------------------------------------------------------------------------

DxfPreviewWidget::DxfPreviewWidget(QWidget* parent) : QWidget(parent) {
    setMinimumSize(240, 200);
    setAutoFillBackground(true);
    QPalette p = palette();
    p.setColor(QPalette::Window, Qt::white);
    setPalette(p);
}

void DxfPreviewWidget::setStrokes(std::vector<Stroke> strokes) {
    m_strokes = std::move(strokes);
    update();
}

void DxfPreviewWidget::mousePressEvent(QMouseEvent* event) { m_last = event->pos(); }

void DxfPreviewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!(event->buttons() & Qt::LeftButton)) return;
    m_yaw += (event->pos().x() - m_last.x()) * 0.5;
    m_pitch = std::clamp(m_pitch + (event->pos().y() - m_last.y()) * 0.5, -89.0, 89.0);
    m_last = event->pos();
    update();
}

void DxfPreviewWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::white);
    painter.setPen(QColor(0xB0, 0xB0, 0xB0));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
    if (m_strokes.empty()) return;
    // Orthographic view: turn about Z by the yaw, then tilt by the pitch; X goes right, Z goes up.
    const double yaw = m_yaw * M_PI / 180.0, pitch = m_pitch * M_PI / 180.0;
    const double cy = std::cos(yaw), sy = std::sin(yaw), cp = std::cos(pitch), sp = std::sin(pitch);
    const auto project = [&](double x, double y, double z) {
        const double xr = x * cy - y * sy, yr = x * sy + y * cy;
        return QPointF(xr, -(z * cp + yr * sp));
    };
    double minX = std::numeric_limits<double>::max(), minY = minX, maxX = -minX, maxY = -minX;
    for (const Stroke& s : m_strokes)
        for (const QPointF p : {project(s.x1, s.y1, s.z1), project(s.x2, s.y2, s.z2)}) {
            minX = std::min(minX, p.x());
            maxX = std::max(maxX, p.x());
            minY = std::min(minY, p.y());
            maxY = std::max(maxY, p.y());
        }
    const double w = std::max(maxX - minX, 1e-6), h = std::max(maxY - minY, 1e-6);
    const double margin = 14.0;
    const double scale = std::min((width() - 2 * margin) / w, (height() - 2 * margin) / h);
    const QPointF centre((minX + maxX) / 2, (minY + maxY) / 2);
    const auto toScreen = [&](QPointF p) { return QPointF(width() / 2.0 + (p.x() - centre.x()) * scale, height() / 2.0 + (p.y() - centre.y()) * scale); };
    for (const Stroke& s : m_strokes) {
        painter.setPen(QPen(s.colour, 1.4));
        painter.drawLine(toScreen(project(s.x1, s.y1, s.z1)), toScreen(project(s.x2, s.y2, s.z2)));
    }
}

// ---- DxfSelectionDialog --------------------------------------------------------------------------------------------------

DxfSelectionDialog::DxfSelectionDialog(const std::string& content, const std::vector<cc::core::DxfLayerInfo>& layers, QWidget* parent)
    : CypeDialog(tr("Selección de entidades"), parent), m_content(content), m_layers(layers) {
    resize(900, 460);
    auto* row = new QHBoxLayout();
    row->setSpacing(8);
    m_table = new QTableWidget(static_cast<int>(layers.size()), 4, this);
    m_table->setHorizontalHeaderLabels({QString(), tr("Capa"), tr("Color"), tr("Importar polígonos como\npaños de carga")});
    m_table->verticalHeader()->hide();
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->horizontalHeader()->setSectionResizeMode(kColTick, QHeaderView::Fixed);
    m_table->setColumnWidth(kColTick, 28);
    m_table->horizontalHeader()->setSectionResizeMode(kColName, QHeaderView::Stretch);
    m_table->setColumnWidth(kColColour, 56);
    m_table->setColumnWidth(kColPanels, 160);
    m_table->horizontalHeader()->setFixedHeight(44);
    for (int i = 0; i < static_cast<int>(layers.size()); ++i) {
        const cc::core::DxfLayerInfo& info = layers[static_cast<std::size_t>(i)];
        auto* tick = new QTableWidgetItem();
        tick->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        tick->setCheckState(Qt::Checked);  // the selector of the original passes "selected"; here every layer starts included
        m_table->setItem(i, kColTick, tick);
        auto* name = new QTableWidgetItem(QString::fromStdString(info.name));
        name->setFlags(Qt::ItemIsEnabled);
        m_table->setItem(i, kColName, name);
        auto* colour = new QTableWidgetItem();
        colour->setFlags(Qt::ItemIsEnabled);
        colour->setBackground(QColor::fromRgb(cc::core::aciToRgb(info.colorIndex)));
        m_table->setItem(i, kColColour, colour);
        auto* panels = new QTableWidgetItem();
        panels->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        panels->setCheckState(Qt::Unchecked);  // [REA] default FALSE
        m_table->setItem(i, kColPanels, panels);
    }
    connect(m_table, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (item->column() == kColTick) refreshPreview();
    });
    row->addWidget(m_table, 4);  // 40 % / 60 % as in the descriptor of the original
    m_preview = new DxfPreviewWidget(this);
    row->addWidget(m_preview, 6);
    contentLayout()->addLayout(row, 1);
    m_error = new QLabel(this);
    m_error->setStyleSheet(QStringLiteral("color: #C00000;"));
    m_error->setWordWrap(true);
    m_error->hide();
    contentLayout()->addWidget(m_error);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
    refreshPreview();
}

int DxfSelectionDialog::layerCount() const { return m_table->rowCount(); }

std::set<std::string> DxfSelectionDialog::selectedLayers() const {
    std::set<std::string> out;
    for (int i = 0; i < m_table->rowCount(); ++i)
        if (m_table->item(i, kColTick)->checkState() == Qt::Checked) out.insert(m_layers[static_cast<std::size_t>(i)].name);
    return out;
}

std::set<std::string> DxfSelectionDialog::panelLayers() const {
    std::set<std::string> out;
    for (int i = 0; i < m_table->rowCount(); ++i)
        if (m_table->item(i, kColPanels)->checkState() == Qt::Checked) out.insert(m_layers[static_cast<std::size_t>(i)].name);
    return out;
}

void DxfSelectionDialog::setLayerChecked(int row, bool checked) { m_table->item(row, kColTick)->setCheckState(checked ? Qt::Checked : Qt::Unchecked); }
void DxfSelectionDialog::setPanelChecked(int row, bool checked) { m_table->item(row, kColPanels)->setCheckState(checked ? Qt::Checked : Qt::Unchecked); }
QString DxfSelectionDialog::errorText() const { return m_error->isHidden() ? QString() : m_error->text(); }

void DxfSelectionDialog::accept() {
    if (selectedLayers().empty()) {  // #61019
        m_error->setText(tr("Es necesario haber seleccionado, al menos, una capa."));
        m_error->show();
        return;
    }
    m_error->hide();
    CypeDialog::accept();
}

void DxfSelectionDialog::refreshPreview() {
    std::vector<DxfPreviewWidget::Stroke> strokes;
    for (const cc::core::DxfLayerInfo& info : m_layers) {
        bool ticked = false;
        for (int i = 0; i < m_table->rowCount(); ++i)
            if (m_table->item(i, kColName)->text() == QString::fromStdString(info.name))
                ticked = m_table->item(i, kColTick)->checkState() == Qt::Checked;
        if (!ticked) continue;
        const QString key = QString::fromStdString(info.name);
        auto it = m_cache.find(key);
        if (it == m_cache.end()) {
            std::vector<DxfPreviewWidget::Stroke> layerStrokes;
            cc::core::DxfOptions options;
            options.layers = {info.name};
            const cc::core::DxfReadResult r = cc::core::readDxf(m_content, options);
            QColor colour = QColor::fromRgb(cc::core::aciToRgb(info.colorIndex));
            if (colour == QColor(Qt::white)) colour = QColor(0x20, 0x20, 0x20);  // colour 7 is drawn black on white
            const auto position = [&r](long long number) {
                for (const auto& n : r.geometry.nodes)
                    if (n.number == number) return n.position;
                return cc::core::Point3{};
            };
            for (const auto& bar : r.geometry.bars) {
                const auto a = position(bar.startNumber), b = position(bar.endNumber);
                layerStrokes.push_back({a.x, a.y, a.z, b.x, b.y, b.z, colour});
            }
            for (const auto* group : {&r.geometry.shells, &r.geometry.panels})
                for (const auto& poly : *group)
                    for (std::size_t k = 0; k < poly.nodeNumbers.size(); ++k) {
                        const auto a = position(poly.nodeNumbers[k]), b = position(poly.nodeNumbers[(k + 1) % poly.nodeNumbers.size()]);
                        layerStrokes.push_back({a.x, a.y, a.z, b.x, b.y, b.z, colour});
                    }
            it = m_cache.insert(key, std::move(layerStrokes));
        }
        strokes.insert(strokes.end(), it->begin(), it->end());
    }
    m_preview->setStrokes(std::move(strokes));
}

// ---- ImportInsertionDialog -----------------------------------------------------------------------------------------------

ImportInsertionDialog::ImportInsertionDialog(QWidget* parent) : CypeDialog(tr("Importar"), parent) {
    m_inFile = new QRadioButton(tr("Importar en la posición definida en el fichero"), this);
    m_inFile->setChecked(true);  // [REA] default option 0
    m_pick = new QRadioButton(tr("Seleccionar el punto de referencia para la importación"), this);
    contentLayout()->addWidget(m_inFile);
    contentLayout()->addWidget(m_pick);
    auto* form = new QFormLayout();
    const auto spin = [this](const QString& label, QFormLayout* f) {
        auto* box = new QDoubleSpinBox(this);
        box->setRange(-50000.0, 50000.0);
        box->setDecimals(3);
        box->setSuffix(tr(" m"));
        box->setEnabled(false);
        f->addRow(label, box);
        return box;
    };
    m_x = spin(QStringLiteral("X"), form);
    m_y = spin(QStringLiteral("Y"), form);
    m_z = spin(QStringLiteral("Z"), form);
    connect(m_pick, &QRadioButton::toggled, this, [this](bool on) {
        for (QDoubleSpinBox* box : {m_x, m_y, m_z}) box->setEnabled(on);
    });
    form->setContentsMargins(22, 0, 0, 0);
    contentLayout()->addLayout(form);
    setupFooter(tr("Aceptar"), tr("Cancelar"));
}

bool ImportInsertionDialog::useReferencePoint() const { return m_pick->isChecked(); }
cc::core::Point3 ImportInsertionDialog::referencePoint() const { return {m_x->value(), m_y->value(), m_z->value()}; }
void ImportInsertionDialog::setUseReferencePoint(bool on) { (on ? m_pick : m_inFile)->setChecked(true); }
void ImportInsertionDialog::setReferencePoint(const cc::core::Point3& p) {
    m_x->setValue(p.x);
    m_y->setValue(p.y);
    m_z->setValue(p.z);
}

// ---- ImportLogDialog -----------------------------------------------------------------------------------------------------

ImportLogDialog::ImportLogDialog(const QStringList& log, bool success, QWidget* parent)
    : CypeDialog(tr("Lectura datos fichero texto"), parent), m_success(success) {
    resize(620, 380);
    m_log = new QPlainTextEdit(this);
    m_log->setReadOnly(true);
    m_log->setPlainText(log.join(QLatin1Char('\n')));
    QFont mono(QStringLiteral("Consolas"));
    mono.setStyleHint(QFont::Monospace);
    m_log->setFont(mono);
    contentLayout()->addWidget(m_log, 1);
    auto* question = new QLabel(success ? tr("Lectura finalizada correctamente. Desea importar los datos leídos...") : tr("Proceso finalizado con error"), this);
    question->setWordWrap(true);
    if (!success) question->setStyleSheet(QStringLiteral("color: #C00000;"));
    contentLayout()->addWidget(question);
    if (success) {
        setupFooter(tr("Sí"), tr("No"));
    } else {
        setupFooter(tr("Aceptar"), tr("Cancelar"));
        cancelButton()->hide();
    }
}

QString ImportLogDialog::logText() const { return m_log->toPlainText(); }

}  // namespace cc::app
