#include "CategoriesPanel.h"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QTableWidget>
#include <QVBoxLayout>

namespace cc::app {

namespace {
constexpr int kIconSize = 18;
const QColor kLockColour(0x3a, 0x4a, 0x63);

QPixmap newPixmap() {
    QPixmap pm(kIconSize, kIconSize);
    pm.fill(Qt::transparent);
    return pm;
}

// Same drawings as the Etiquetas table (LayersPanel.cpp).
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

QString categoryName(int row) {
    switch (static_cast<cc::core::Category>(row)) {
        case cc::core::Category::Nodes: return QObject::tr("Nudos");
        case cc::core::Category::Bars: return QObject::tr("Barras");
        case cc::core::Category::Ties: return QObject::tr("Tirantes");
        case cc::core::Category::Columns: return QObject::tr("Pilares");
        case cc::core::Category::Beams: return QObject::tr("Vigas");
        case cc::core::Category::Panels: return QObject::tr("Paños");
        case cc::core::Category::Shells: return QObject::tr("Láminas");
    }
    return {};
}
}  // namespace

CategoriesPanel::CategoriesPanel(QWidget* parent) : QWidget(parent) {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 4);
    root->setSpacing(4);

    m_table = new QTableWidget(cc::core::kCategoryCount, ColumnCount, this);
    m_table->setObjectName(QStringLiteral("categoriesTable"));
    m_table->setHorizontalHeaderLabels({tr("Referencia"), QString(), QString()});
    m_table->verticalHeader()->hide();
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setIconSize(QSize(kIconSize, kIconSize));
    m_table->horizontalHeader()->setSectionResizeMode(ColReference, QHeaderView::Stretch);
    for (int c = ColVisible; c < ColumnCount; ++c) m_table->horizontalHeader()->setSectionResizeMode(c, QHeaderView::ResizeToContents);
    for (int row = 0; row < cc::core::kCategoryCount; ++row) {
        for (int c = 0; c < ColumnCount; ++c) {
            auto* item = new QTableWidgetItem;
            item->setFlags(Qt::ItemIsEnabled);
            m_table->setItem(row, c, item);
        }
        m_table->item(row, ColReference)->setText(categoryName(row));
    }
    refreshTable();
    // The original shows all seven rows without scrolling (TK-028): size the table to its content.
    m_table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    {
        int height = m_table->horizontalHeader()->sizeHint().height() + 2 * m_table->frameWidth();
        for (int row = 0; row < cc::core::kCategoryCount; ++row) height += m_table->rowHeight(row);
        m_table->setFixedHeight(height);
    }
    root->addWidget(m_table);

    m_layerColours = new QCheckBox(tr("Representar la estructura usando los colores de las capas"), this);
    m_layerColours->setObjectName(QStringLiteral("layerColours"));
    m_layerColours->setChecked(true);
    root->addWidget(m_layerColours);

    root->addWidget(new QLabel(tr("Elementos considerados en el análisis"), this));
    m_analysis = new QComboBox(this);
    m_analysis->setObjectName(QStringLiteral("analysisElements"));
    m_analysis->addItem(tr("Tensión / Deformación"));
    m_analysis->addItem(tr("Pandeo"));
    m_analysis->addItem(tr("Modal"));
    root->addWidget(m_analysis);

    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int column) {
        if (column == ColVisible) toggleVisible(row);
        else if (column == ColLocked) toggleLocked(row);
    });
    connect(m_layerColours, &QCheckBox::toggled, this, [this](bool on) {
        if (m_updating) return;
        m_settings.useLayerColours = on;
        emit settingsChanged(m_settings);
    });
    connect(m_analysis, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (m_updating || index < 0) return;
        m_settings.analysis = static_cast<cc::core::AnalysisElements>(index);
        emit settingsChanged(m_settings);
    });
}

void CategoriesPanel::refreshTable() {
    for (int row = 0; row < cc::core::kCategoryCount; ++row) {
        m_table->item(row, ColVisible)->setIcon(eyeIcon(m_settings.visible[row]));
        m_table->item(row, ColLocked)->setIcon(lockIcon(m_settings.locked[row]));
    }
}

void CategoriesPanel::setSettings(const cc::core::CategorySettings& settings) {
    m_updating = true;
    m_settings = settings;
    refreshTable();
    m_layerColours->setChecked(settings.useLayerColours);
    m_analysis->setCurrentIndex(static_cast<int>(settings.analysis));
    m_updating = false;
}

void CategoriesPanel::toggleVisible(int row) {
    if (row < 0 || row >= cc::core::kCategoryCount) return;
    m_settings.visible[row] = !m_settings.visible[row];
    refreshTable();
    emit settingsChanged(m_settings);
}

void CategoriesPanel::toggleLocked(int row) {
    if (row < 0 || row >= cc::core::kCategoryCount) return;
    m_settings.locked[row] = !m_settings.locked[row];  // honoured when editing commands exist (TK-029)
    refreshTable();
    emit settingsChanged(m_settings);
}

}  // namespace cc::app
