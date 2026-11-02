#include "LeftPanel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QSplitter>
#include <QSplitterHandle>
#include <QToolButton>
#include <QVBoxLayout>

#include "CategoriesPanel.h"
#include "DisplayOptionsPanel.h"
#include "LayersPanel.h"

namespace cc::app {

namespace {
/// Splitter handle showing the "...." grip of the original.
class DottedHandle : public QSplitterHandle {
public:
    using QSplitterHandle::QSplitterHandle;
    QSize sizeHint() const override { return {0, 8}; }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.fillRect(rect(), palette().window());
        p.setPen(QPen(QColor(110, 110, 110), 2));
        const qreal y = height() / 2.0;
        for (int i = -2; i <= 1; ++i) p.drawPoint(QPointF(width() / 2.0 + i * 4 + 2, y));
    }
};

class DottedSplitter : public QSplitter {
public:
    explicit DottedSplitter(QWidget* parent) : QSplitter(Qt::Vertical, parent) {}

protected:
    QSplitterHandle* createHandle() override { return new DottedHandle(orientation(), this); }
};
}  // namespace

PanelSection::PanelSection(const QString& title, QWidget* content, QWidget* parent) : QWidget(parent), m_content(content) {
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    auto* header = new QWidget(this);
    header->setObjectName(QStringLiteral("sectionHeader"));
    header->setStyleSheet(QStringLiteral("#sectionHeader { background: #f0f0f0; }"));
    auto* row = new QHBoxLayout(header);
    row->setContentsMargins(4, 2, 2, 2);
    row->addWidget(new QLabel(title, header), 1);
    auto makeButton = [&](const QString& glyph, const QString& tip) {
        auto* button = new QToolButton(header);
        button->setText(glyph);
        button->setToolTip(tip);
        button->setAutoRaise(true);
        row->addWidget(button);
        return button;
    };
    m_chevron = makeButton(QStringLiteral("˅"), tr("Plegar o desplegar"));
    m_close = makeButton(QStringLiteral("×"), tr("Cerrar"));
    layout->addWidget(header);
    layout->addWidget(content, 1);

    connect(m_chevron, &QToolButton::clicked, this, [this] { setFolded(!m_folded); });
    connect(m_close, &QToolButton::clicked, this, &QWidget::hide);
}

void PanelSection::setFolded(bool folded) {
    m_folded = folded;
    m_content->setVisible(!folded);
    m_chevron->setText(folded ? QStringLiteral("˄") : QStringLiteral("˅"));
}

LeftPanel::LeftPanel(QWidget* parent) : QWidget(parent) {
    setFixedWidth(kWidth);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    m_splitter = new DottedSplitter(this);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setHandleWidth(8);

    m_layersPanel = new LayersPanel;
    m_labels = new PanelSection(tr("Etiquetas"), m_layersPanel, m_splitter);
    m_categoriesPanel = new CategoriesPanel;
    m_categories = new PanelSection(tr("Categorías"), m_categoriesPanel, m_splitter);
    m_displayOptions = new DisplayOptionsPanel;
    m_display = new PanelSection(tr("Opciones de visualización"), m_displayOptions, m_splitter);
    m_splitter->addWidget(m_labels);
    m_splitter->addWidget(m_categories);
    m_splitter->addWidget(m_display);
    m_splitter->setStretchFactor(0, 1);
    m_splitter->setStretchFactor(1, 0);
    m_splitter->setStretchFactor(2, 0);
    layout->addWidget(m_splitter);
}

}  // namespace cc::app
