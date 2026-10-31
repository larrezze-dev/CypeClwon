#pragma once
#include "CypeDialog.h"

class QRadioButton;

namespace cc::app {

/// Second step of "Nueva obra": choose between an empty project and automatic IFC import
/// (reference: docs/screenshots/orig_asistente_01.png).
class ProjectTypeDialog : public CypeDialog {
    Q_OBJECT
public:
    enum class ProjectType { Empty, Ifc };

    explicit ProjectTypeDialog(QWidget* parent = nullptr);

    ProjectType projectType() const;

    /// Whether the chosen type can be created yet; IFC import arrives in phase 17 (ticket TK-004).
    static bool isAvailable(ProjectType type) { return type == ProjectType::Empty; }

public slots:
    void accept() override;

private:
    void updateBoldSelection();

    QRadioButton* m_empty = nullptr;
    QRadioButton* m_ifc = nullptr;
};

}  // namespace cc::app
