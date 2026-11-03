#pragma once
#include <QString>
#include <functional>
#include <memory>

#include "CypeDialog.h"
#include "core/profile_library.h"
#include "core/project.h"
#include "core/undo_stack.h"

class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QTableWidget;
class QToolButton;
class QComboBox;

namespace cc::app {

/// GUI of the profile library (T-P5-05): Obra > Biblioteca de perfiles. Texts and sizes from
/// docs/research/ui/dialogs_biblioteca_perfiles.md and perfiles_biblioteca.md; the core is core/profile_library.h.
///
/// The dialogs edit a working copy (`LibraryScratch`) so that Cancelar leaves the work untouched; the main window pushes ONE
/// ReplaceLibraryCommand when the management dialog is accepted. Anything not measured in the original is marked
/// PROVISIONAL and listed in .tickets.md (TK-053, TK-058..).

/// Message boxes go through a replaceable sink so tests never block. Default: QMessageBox.
struct ProfileMessageSink {
    std::function<void(QWidget*, const QString& title, const QString& text)> warn;
    /// Returns true when the user picks the first choice (`first`), false for `second`.
    std::function<bool(QWidget*, const QString& title, const QString& text, const QString& first, const QString& second)> ask;
};
ProfileMessageSink& profileMessageSink();

/// File selectors, replaceable for the same reason.
struct ProfileFileSink {
    std::function<QString(QWidget*)> openFile;
    std::function<QString(QWidget*)> saveFile;
};
ProfileFileSink& profileFileSink();

/// Working copy of the library of a work: a scratch Project (only profileLibrary is used) with its own history.
struct LibraryScratch {
    explicit LibraryScratch(const cc::core::ProfileLibrary& library) { project.profileLibrary = library; }
    cc::core::Project project;
    cc::core::UndoStack undo;
    const cc::core::ProfileLibrary& library() const { return project.profileLibrary; }
};

/// Imports series from a file into the scratch (ask for each existing reference, #33822..#33824). Returns false on error
/// (already reported). `imported` receives the number of series.
bool importSeriesFile(QWidget* parent, LibraryScratch& scratch, const QString& path, int* imported = nullptr);
/// Exports every series (or the given ones) of the scratch to `path`; asks before overwriting (#138928).
bool exportSeriesFile(QWidget* parent, const std::vector<cc::core::ProfileSeries>& series, const QString& path);

/// "Nuevo perfil" / "Editar perfil" of one series (shape fixed by the series). Shows the properties computed by the core.
/// PROVISIONAL layout: the original's dialog was not captured (TK-053).
class ProfileEditDialog : public CypeDialog {
    Q_OBJECT
public:
    /// `shapeChangeable`: the first profile of a new series chooses the shape of the series (the original takes it from the
    /// shape icons of "Describir"; the clone has no such context, TK-059).
    /// `sectionUnit`: length unit of the work ("Unidades de acotación" > Dimensiones) used for concrete / timber dimensions;
    /// hot-rolled steel stays in millimetres.
    ProfileEditDialog(cc::core::Section initial, bool isNew, bool shapeChangeable = false, QWidget* parent = nullptr,
                      cc::core::LengthUnit sectionUnit = cc::core::LengthUnit::Centimetres);
    QComboBox* familyCombo() const { return m_familyCombo; }
    cc::core::SectionFamily family() const { return m_family; }
    QComboBox* shapeCombo() const { return m_shape; }
    /// Section as edited (tabulated values of the initial section are kept).
    cc::core::Section value() const;
    QLineEdit* referenceEdit() const { return m_reference; }
    QDoubleSpinBox* dimensionSpin(int index) const { return m_dims.value(index); }
    QLabel* propertiesLabel() const { return m_properties; }
    void accept() override;

private:
    void refreshProperties();
    void buildDims(cc::core::ShapeKind kind, const std::vector<double>& dims);
    void fillShapes(bool selectDefault);
    void updateShapeLabel();
    cc::core::Section m_initial;
    cc::core::ShapeKind m_kind;
    cc::core::SectionFamily m_family;
    cc::core::LengthUnit m_unit;
    QFormLayout* m_form;
    QComboBox* m_familyCombo;
    QLabel* m_shapeLabel;
    QComboBox* m_shape;
    QLineEdit* m_reference;
    QList<QDoubleSpinBox*> m_dims;
    QLabel* m_properties;
};

/// "Nueva serie" (330x421) / "Editar serie": Referencia, Descripción, Perfiles table with + / edit / duplicate / delete /
/// up / down. Accept validates with validateSeries() ("Debe introducir, al menos, un perfil en la serie.").
class SeriesEditDialog : public CypeDialog {
    Q_OBJECT
public:
    SeriesEditDialog(cc::core::ProfileSeries initial, bool isNew, QWidget* parent = nullptr,
                     cc::core::LengthUnit sectionUnit = cc::core::LengthUnit::Centimetres);
    const cc::core::ProfileSeries& value() const { return m_series; }
    QLineEdit* referenceEdit() const { return m_reference; }
    QPlainTextEdit* descriptionEdit() const { return m_description; }
    QTableWidget* table() const { return m_table; }
    QToolButton* button(const QString& name) const;
    void accept() override;
    bool importFile(const QString& path);  ///< loads reference, description and profiles of the first series of a file
    bool exportFile(const QString& path);

    // Operations behind the buttons (also used by tests); `row` is the selected profile.
    void addProfile(const cc::core::Section& profile);
    void replaceProfile(int row, const cc::core::Section& profile);
    void duplicateProfile(int row);
    void removeProfile(int row);
    void moveProfile(int row, int direction);

private:
    void refreshTable();
    void syncFromFields();
    void setShapeOfSeries(cc::core::ShapeKind kind);
    void onNew();
    void onEdit();
    cc::core::ProfileSeries m_series;
    cc::core::LengthUnit m_unit;
    QLineEdit* m_reference;
    QPlainTextEdit* m_description;
    QTableWidget* m_table;
    QList<QToolButton*> m_buttons;
};

/// "Series de Perfiles" (530x520): toolbar new / edit / duplicate / delete / up / down + import / export of series files,
/// table Referencia | Catálogo | Descripción. Works on a scratch; Cancelar restores it.
class SeriesManagerDialog : public CypeDialog {
    Q_OBJECT
public:
    SeriesManagerDialog(LibraryScratch& scratch, QWidget* parent = nullptr);
    QTableWidget* table() const { return m_table; }
    QToolButton* button(const QString& name) const;
    void reject() override;

    // Operations behind the buttons (a refusal is reported through profileMessageSink()).
    bool newSeries(const cc::core::ProfileSeries& series);
    bool editSeries(int row, const cc::core::ProfileSeries& series);
    bool duplicateSeries(int row);
    bool removeSeries(int row);
    bool moveSeries(int row, int direction);
    bool importFile(const QString& path);
    bool exportFile(const QString& path);

private:
    void refreshTable(int selectRow = -1);
    int currentRow() const;
    void onNew();
    void onEdit();
    void onImport();
    void onExport();
    bool report(const std::string& error);
    LibraryScratch& m_scratch;
    cc::core::ProfileLibrary m_snapshot;
    QTableWidget* m_table;
    QList<QToolButton*> m_buttons;
};

/// "Importación de series de perfiles predefinidas" (312x555): table Importar | Nombre Biblioteca | Editar.
class PredefinedImportDialog : public CypeDialog {
    Q_OBJECT
public:
    PredefinedImportDialog(LibraryScratch& scratch, QWidget* parent = nullptr);
    QTableWidget* table() const { return m_table; }
    /// Names of the libraries of the original, in its order.
    static const QStringList& libraryNames();
    /// Whether the clone has series for that library (only "Gama de perfiles europeos").
    static bool isAvailable(const QString& libraryName);
    void accept() override;
    /// Imports the series of the checked rows; false when nothing is checked or the import fails (reported).
    bool importChecked();
    /// The "Editar" cell of an available row opens the series manager (PROVISIONAL access, TK-059).
    void openSeriesManager();

private:
    LibraryScratch& m_scratch;
    QTableWidget* m_table;
};

/// "Gestión de la biblioteca de perfiles" (328x224): the four buttons of the original (the series manager is reached from
/// "Describir" in the original, T-P5-07; the clone reaches it from "Editar" in the predefined import, TK-059).
class ProfileLibraryDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit ProfileLibraryDialog(const cc::core::ProfileLibrary& library, QWidget* parent = nullptr,
                                  cc::core::LengthUnit sectionUnit = cc::core::LengthUnit::Centimetres);
    /// Library as edited.
    const cc::core::ProfileLibrary& library() const { return m_scratch.library(); }
    QPushButton* button(const QString& name) const;

    void openPredefined();
    void openManufacturerCatalogues();
    void openInitialImport();
    void exportSeries();

private:
    LibraryScratch m_scratch;
    QList<QPushButton*> m_buttons;
};

}  // namespace cc::app
