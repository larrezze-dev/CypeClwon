#pragma once
#include <QPointF>
#include <QWidget>
#include <QString>
#include <array>
#include <functional>
#include <optional>
#include <vector>

#include "CypeDialog.h"
#include "core/shell_properties.h"

class QCheckBox;
class QDialog;
class QToolButton;

namespace cc::app {

using BandZone = std::array<cc::core::Point2, 4>;

/// 2D viewer of the band editor (propiedades_laminas.md section 3, p7_42 / p7_n_montage): the contour of the first shell in green, magenta dimension
/// lines of its sides, the local axes at the lower left corner (X red, Y blue), the bands as magenta dotted rectangles with their centre line (orange
/// when selected) and its own zoom. The mouse logic of the commands lives here so that tests can drive it with model points.
///  - New: p1, p2 (a dotted rectangle follows the cursor); the band is created at the second click (a second click that would make it shorter
///    than the minimum length is ignored silently); RMB -> requestWidth (the dialog asks the width again; Cancelar ends the tool).
///  - Generate: four corners of the zone, RMB -> requestGenerate.
///  - Edit / Delete / Divide / Join: click on a band or window / crossing (two clicks, like the rest of the original), RMB -> requestAct.
///  - MoveEnd: click near an end of a band, then click the new position -> requestMoveEnd.
class BandViewer : public QWidget {
    Q_OBJECT
public:
    enum class Tool { None, New, Generate, Edit, Delete, MoveEnd, Join, Divide };

    explicit BandViewer(QWidget* parent = nullptr);

    void setShape(const std::vector<cc::core::Point2>& contour);
    void setBands(const std::vector<cc::core::ShellIntegrationBand>& bands);
    void setTool(Tool tool);
    Tool tool() const { return m_tool; }
    void setShowReferences(bool on);
    bool showReferences() const { return m_references; }
    void setShowIncidences(bool on);
    void setPreviewWidth(double widthM) { m_previewWidth = widthM; update(); }
    const std::vector<int>& selected() const { return m_selected; }
    void clearSelection();
    /// Points already entered by the current command.
    const std::vector<cc::core::Point2>& points() const { return m_points; }
    bool windowPending() const { return m_windowStart.has_value(); }

    // View
    void fitAll();
    void zoomBy(double factor);
    void previousView();
    void setPanMode(bool on);
    void setZoomWindowMode(bool on);
    double scale() const { return m_scale; }
    QPointF toScreen(const cc::core::Point2& p) const;
    cc::core::Point2 toModel(const QPointF& screen) const;

    /// Command input in model coordinates (the mouse events call these; tests too). `screenPos` only matters for the window selection.
    void click(const cc::core::Point2& model, Qt::KeyboardModifiers modifiers = Qt::NoModifier);
    void rightClick();
    void cancel();

signals:
    void requestNew(cc::core::Point2 a, cc::core::Point2 b);
    void requestGenerate(cc::app::BandZone zone);
    void requestAct(cc::app::BandViewer::Tool tool, std::vector<int> indices);
    void requestMoveEnd(int index, int end, cc::core::Point2 to);
    void requestWidth(cc::app::BandViewer::Tool tool);
    void hint(QString text);

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void keyPressEvent(QKeyEvent*) override;

private:
    int bandAt(const cc::core::Point2& p) const;
    std::optional<std::pair<int, int>> endAt(const cc::core::Point2& p) const;
    void applySelection(const std::vector<int>& hits, Qt::KeyboardModifiers modifiers);
    void pushView();
    void announce();

    std::vector<cc::core::Point2> m_contour;
    std::vector<cc::core::ShellIntegrationBand> m_bands;
    Tool m_tool = Tool::None;
    bool m_references = false;
    bool m_incidences = false;
    double m_previewWidth = cc::core::kBandWidthDefaultM;
    std::vector<cc::core::Point2> m_points;
    std::vector<int> m_selected;
    std::optional<cc::core::Point2> m_windowStart;
    std::optional<std::pair<int, int>> m_pendingEnd;
    cc::core::Point2 m_cursor;
    bool m_cursorInside = false;
    double m_scale = 100.0;
    QPointF m_centre;  ///< model point at the centre of the widget
    std::vector<std::array<double, 3>> m_views;  ///< previous views (scale, centre x, centre y)
    bool m_pan = false, m_zoomWindow = false;
    std::optional<QPointF> m_dragLast;
    std::optional<cc::core::Point2> m_zoomStart;
    bool m_fitted = false;
};

/// Dialog «Bandas de integración» (p7_42, about 800 x 520, resizable): toolbar (Deshacer, Rehacer, Integración de esfuerzos, Generar en zona, Nueva,
/// Editar, Borrar, Mover extremo, Unir, Dividir, Incidencias, then the tools of the viewer), the side panel «Opciones» with «Referencias de bandas» and the
/// viewer. Edits are kept in a history of its own; Aceptar hands the final list to the caller, Cancelar discards everything.
class ShellBandsDialog : public CypeDialog {
    Q_OBJECT
public:
    ShellBandsDialog(const std::vector<cc::core::Point2>& contour, std::vector<cc::core::ShellIntegrationBand> bands,
                     const cc::core::BandForceOptions& force, QWidget* parent = nullptr);

    const std::vector<cc::core::ShellIntegrationBand>& bands() const { return m_history[m_pos]; }
    cc::core::BandForceOptions force() const { return m_force; }
    BandViewer* viewer() const { return m_viewer; }
    QToolButton* button(const QString& name) const;
    QCheckBox* referencesCheck() const { return m_references; }
    bool canUndo() const { return m_pos > 0; }
    bool canRedo() const { return m_pos + 1 < m_history.size(); }
    void undo();
    void redo();
    /// Width kept for the next «Nueva» / «Generar» (metres).
    double width() const { return m_width; }
    void setWidth(double widthM);

    /// Runs the modal sub-dialogs; the default is exec(); tests replace it so that nothing blocks.
    static void setDialogRunner(std::function<int(QDialog&)> runner);

    // The actions of the toolbar (also called by the viewer signals).
    void beginNew();
    void beginGenerate();
    void editIntegration();
    /// Creates the band between two points (silent when it would be shorter than the minimum length).
    void doNew(const cc::core::Point2& a, const cc::core::Point2& b);
    void doGenerate(const std::array<cc::core::Point2, 4>& zone);
    void doAct(BandViewer::Tool tool, const std::vector<int>& indices);
    void doMoveEnd(int index, int end, const cc::core::Point2& to);

signals:
    void statusChanged(const QString& text);

private:
    void apply(const cc::core::BandResult& result);
    void activate(BandViewer::Tool tool, const QString& name);
    int run(QDialog& dialog);
    bool askWidth(const QString& title);
    void refresh();
    void reopenWidth(BandViewer::Tool tool);

    std::vector<std::vector<cc::core::ShellIntegrationBand>> m_history;
    std::size_t m_pos = 0;
    std::vector<cc::core::Point2> m_contour;
    cc::core::BandForceOptions m_force;
    double m_width = cc::core::kBandWidthDefaultM;
    BandViewer* m_viewer = nullptr;
    QCheckBox* m_references = nullptr;
    std::vector<std::pair<QString, QToolButton*>> m_buttons;
    QToolButton* m_undoButton = nullptr;
    QToolButton* m_redoButton = nullptr;
};

}  // namespace cc::app

Q_DECLARE_METATYPE(cc::app::BandViewer::Tool)
