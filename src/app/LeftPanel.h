#pragma once
#include <QWidget>

class QSplitter;
class QToolButton;

namespace cc::app {

class CategoriesPanel;
class DisplayOptionsPanel;
class LayersPanel;

/// One section of the left panel: a header (title, chevron that folds the content, x that closes the section) above a
/// content widget (reference: docs/screenshots/tp2_combo_open.png).
class PanelSection : public QWidget {
    Q_OBJECT
public:
    PanelSection(const QString& title, QWidget* content, QWidget* parent = nullptr);

    QWidget* content() const { return m_content; }
    bool isFolded() const { return m_folded; }
    void setFolded(bool folded);

    QToolButton* chevronButton() const { return m_chevron; }
    QToolButton* closeButton() const { return m_close; }

private:
    QWidget* m_content;
    QToolButton* m_chevron;
    QToolButton* m_close;
    bool m_folded = false;
};

/// Left panel of the main window: Etiquetas, Categorías and Opciones de visualización stacked in a vertical splitter
/// with dotted handles. Etiquetas holds the LayersPanel (T-P4-06); Categorías holds the CategoriesPanel (T-P4-07).
class LeftPanel : public QWidget {
    Q_OBJECT
public:
    static constexpr int kWidth = 382;  ///< width in the original (sn3_near_node.png)

    explicit LeftPanel(QWidget* parent = nullptr);

    PanelSection* labelsSection() const { return m_labels; }
    PanelSection* categoriesSection() const { return m_categories; }
    PanelSection* displaySection() const { return m_display; }
    DisplayOptionsPanel* displayOptions() const { return m_displayOptions; }
    LayersPanel* layersPanel() const { return m_layersPanel; }
    CategoriesPanel* categoriesPanel() const { return m_categoriesPanel; }
    QSplitter* splitter() const { return m_splitter; }

private:
    QSplitter* m_splitter;
    PanelSection* m_labels;
    PanelSection* m_categories;
    PanelSection* m_display;
    DisplayOptionsPanel* m_displayOptions;
    LayersPanel* m_layersPanel;
    CategoriesPanel* m_categoriesPanel;
};

}  // namespace cc::app
