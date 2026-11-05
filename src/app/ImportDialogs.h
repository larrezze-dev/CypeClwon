#pragma once
#include <QColor>
#include <QMap>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <set>
#include <string>
#include <vector>

#include "CypeDialog.h"
#include "core/import_dxf.h"

class QDoubleSpinBox;
class QLabel;
class QPlainTextEdit;
class QRadioButton;
class QTableWidget;

namespace cc::app {

/// Simple 3D preview of the layers ticked in the entity selection dialog (importar.md 3.3: "vista 3D del dibujo ... las capas
/// desmarcadas se ocultan"). Own widget (isometric projection, drag with the left button to turn it): lines for the bars, outline
/// for the polygons, each in the colour of its layer.
class DxfPreviewWidget : public QWidget {
    Q_OBJECT
public:
    struct Stroke {
        double x1, y1, z1, x2, y2, z2;
        QColor colour;
    };
    explicit DxfPreviewWidget(QWidget* parent = nullptr);
    void setStrokes(std::vector<Stroke> strokes);
    int strokeCount() const { return static_cast<int>(m_strokes.size()); }

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;

private:
    std::vector<Stroke> m_strokes;
    double m_yaw = -35.0, m_pitch = 25.0;  ///< degrees
    QPoint m_last;
};

/// "Selección de entidades" (#60990), importar.md 3.3: layer list on the left (40 %: tick, "Capa", "Color" and the boolean column
/// "Importar polígonos como paños de carga", false by default) and the 3D preview on the right (60 %).
class DxfSelectionDialog : public CypeDialog {
    Q_OBJECT
public:
    /// `content` is the ASCII DXF text (read for the preview).
    DxfSelectionDialog(const std::string& content, const std::vector<cc::core::DxfLayerInfo>& layers, QWidget* parent = nullptr);

    int layerCount() const;
    std::set<std::string> selectedLayers() const;
    std::set<std::string> panelLayers() const;
    void setLayerChecked(int row, bool checked);
    void setPanelChecked(int row, bool checked);
    /// Message of the last rejected accept (#61019), empty when none.
    QString errorText() const;
    int previewStrokes() const { return m_preview->strokeCount(); }
    QTableWidget* table() const { return m_table; }

    void accept() override;

private:
    void refreshPreview();

    std::string m_content;
    std::vector<cc::core::DxfLayerInfo> m_layers;
    QTableWidget* m_table = nullptr;
    DxfPreviewWidget* m_preview = nullptr;
    QLabel* m_error = nullptr;
    QMap<QString, std::vector<DxfPreviewWidget::Stroke>> m_cache;
};

/// "Importar" (#29651), importar.md 3.4: where the geometry goes when the work already has data. The second option asks for the
/// reference point. DECISION (the original picks it in the viewport; here it is typed): the point is where the origin of the file
/// is placed, i.e. the imported geometry is translated by it.
class ImportInsertionDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit ImportInsertionDialog(QWidget* parent = nullptr);
    bool useReferencePoint() const;
    cc::core::Point3 referencePoint() const;
    void setUseReferencePoint(bool on);
    void setReferencePoint(const cc::core::Point3& p);

private:
    QRadioButton* m_inFile = nullptr;
    QRadioButton* m_pick = nullptr;
    QDoubleSpinBox* m_x = nullptr;
    QDoubleSpinBox* m_y = nullptr;
    QDoubleSpinBox* m_z = nullptr;
};

/// Reading window of "Fichero de texto" (importar.md 3.5): log of the reading and, when it went well, the Sí / No question
/// #106880. With errors the window only closes (Aceptar) and shows "Proceso finalizado con error" (#106881).
class ImportLogDialog : public CypeDialog {
    Q_OBJECT
public:
    ImportLogDialog(const QStringList& log, bool success, QWidget* parent = nullptr);
    QString logText() const;
    bool success() const { return m_success; }

private:
    QPlainTextEdit* m_log = nullptr;
    bool m_success = false;
};

}  // namespace cc::app
