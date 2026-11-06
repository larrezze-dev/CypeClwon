#pragma once
#include <QLineEdit>
#include <QList>
#include <QMap>
#include <QString>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include "CypeDialog.h"
#include "ProfileLibraryDialogs.h"
#include "core/bar_description.h"
#include "core/profile_library.h"
#include "core/project_settings.h"
#include "core/units.h"

class QAbstractButton;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QFrame;
class QLabel;
class QListWidget;
class QRadioButton;
class QStackedWidget;
class QToolButton;
class QTextBrowser;

namespace cc::app {

/// GUI of "Describir" for bars (T-P5-07, Geometria > Barras > Seccion / Material). Behaviour and texts come from
/// docs/research/ui/describir_barras.md (measured in the original); the core is core/bar_description.h.
///
/// Everything the dialog cannot show without blocking (information boxes, the properties box of the (i) button, the series editors
/// reached from the + / pencil / table buttons) goes through DescribeSink so tests never exec a nested dialog. "Aviso" boxes use
/// profileMessageSink().warn, like the profile library dialogs.

struct DescribeSink {
    /// Information box with the info icon and one Aceptar button (the (i) of "Caracteristicas mecanicas", #65241).
    std::function<void(QWidget*, const QString& title, const QString& text)> info;
    /// "Propiedades del perfil seleccionado".
    std::function<void(QWidget*, const cc::core::Section&)> properties;
    /// "Series de Perfiles" editor of one series (the + and pencil buttons); nullopt = cancelled.
    std::function<std::optional<cc::core::ProfileSeries>(QWidget*, const cc::core::ProfileSeries& initial, bool isNew,
                                                         cc::core::LengthUnit)>
        editSeries;
    /// "Series de Perfiles" manager (the table button).
    std::function<void(QWidget*, LibraryScratch&)> manageSeries;
    /// "Perfiles de madera" box (service class + fire resistance check); false = cancelled (the settings are left untouched).
    std::function<bool(QWidget*, cc::core::TimberSettings&)> timberService;
};
DescribeSink& describeSink();

/// Series / profile combos of the steel and timber profile selectors of the Pilar and Viga pages (PROVISIONAL: the original uses
/// piezamet.dll panels, not read; here the series of the library of the family, no shape icons).
struct ProfilePicker {
    QWidget* host = nullptr;
    QComboBox* series = nullptr;
    QComboBox* profile = nullptr;
    /// "Disposición" radios below the combos (steel pillar and steel beam pickers only; null for the embedded-profile picker of the
    /// mixed pillar). One radio per cc::core::Arrangement; only those of cc::core::arrangementsForElement are shown.
    QWidget* arrangementHost = nullptr;
    QButtonGroup* arrangementGroup = nullptr;
    QRadioButton* arrangementRadio(cc::core::Arrangement arrangement) const;
};

/// Rows (label with its colon, value with unit) of the "Propiedades del perfil seleccionado" box. Dimensions with one decimal,
/// properties with two (measured with the IPE 80). Measured (describir_info_perfiles.tsv): timber round (5 rows), aluminium I (12),
/// cold-formed U (13), built-up IPC (14). PROVISIONAL for the other shapes (and the lipped C): the dimension rows of the other shapes
/// were not measured (TODO(captura)); they list the dimensions of the shape followed by the same property rows.
std::vector<std::pair<QString, QString>> profilePropertyRows(const cc::core::Section& section);

/// Numeric field of the original's "Describir": right aligned, validated when it loses the focus (modal "Aviso" box; the field keeps
/// the previous value and takes the focus back). Only the decimal point is accepted ("12,5" is not a number). The tooltip is the name
/// in bold plus "Debe estar entre A y B.".
struct FieldSpec {
    QString name;
    double minimum = 0.0;
    double maximum = 0.0;
    int decimals = 2;
    /// The Poisson message of the original has stray spaces ("entre 0.001  y 50.000 ."); reproduced verbatim.
    bool poissonQuirk = false;
};

class ValueEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit ValueEdit(const FieldSpec& spec, QWidget* parent = nullptr);
    const FieldSpec& spec() const { return m_spec; }
    /// Changes name / range / decimals (the dimension fields change meaning with the section type); keeps the value.
    void setSpec(const FieldSpec& spec);
    double value() const { return m_value; }
    /// Sets the stored value and the text; emits nothing.
    void setValue(double value);
    /// Validates the text (what happens when the field loses the focus); true when accepted.
    bool commit();
    /// Message of a value out of range, as the original words it.
    QString rangeMessage() const;
    QString formatted(double value) const;

signals:
    void valueCommitted(double value);

private:
    FieldSpec m_spec;
    double m_value = 0.0;
    bool m_busy = false;
};

/// One entry of "Perfiles utilizados": the description shared by some bars. The dialog edits `description`; Aceptar applies it to
/// `barIds`.
struct DescribeEntry {
    cc::core::BarDescription description;
    std::vector<int> barIds;
};

/// "Describir" (1051x890 with the left panel "Perfiles utilizados", 859x890 without).
/// PROVISIONAL (not measured): with several entries each one is edited and applied to its own bars (the original only documents
/// that Aceptar applies the result to the selected bars).
class BarDescribeDialog : public CypeDialog {
    Q_OBJECT
public:
    /// `library`: library of the work; when it has no series at all the predefined ones are added to the working copy
    /// (PROVISIONAL: the clone's new works start with an empty library, the original's already hold the standard series).
    /// `showUsedList`: the left panel (not shown when the dialog is opened from the "Propiedades" panel).
    BarDescribeDialog(std::vector<DescribeEntry> entries, const cc::core::ProfileLibrary& library, cc::core::LengthUnit unit,
                      bool showUsedList = true, QWidget* parent = nullptr);

    const std::vector<DescribeEntry>& entries() const { return m_entries; }
    /// Entries as given to the constructor.
    const std::vector<DescribeEntry>& originalEntries() const { return m_original; }
    /// Library as edited from the + / pencil / table buttons.
    const cc::core::ProfileLibrary& library() const { return m_scratch.library(); }
    bool libraryChanged() const { return !(m_scratch.library() == m_initialLibrary); }
    int currentRow() const { return m_row; }
    void selectEntry(int row);

    // --- access for tests --------------------------------------------------------------------------------------------------
    QListWidget* usedList() const { return m_used; }
    /// 0 Generico, 1 Tirante, 2 Pilar, 3 Viga.
    QRadioButton* typeRadio(int index) const;
    int typeIndex() const;
    QComboBox* behaviourCombo() const { return m_behaviour; }
    QToolButton* materialButton(cc::core::BarMaterialIcon icon) const;
    const QList<QToolButton*>& shapeButtons() const { return m_shapeButtons; }
    QComboBox* seriesCombo() const { return m_series; }
    QComboBox* profileCombo() const { return m_profile; }
    QToolButton* button(const QString& name) const;  ///< "new", "edit", "table", "info" (profile), "genericInfo"
    QRadioButton* arrangementRadio(cc::core::Arrangement arrangement) const;
    QCheckBox* stiffenersCheck() const { return m_stiffeners; }
    /// Fields by object name: genericWidthLeft, genericWidthRight, genericDepthBottom, genericDepthTop, genericArea,
    /// genericShearY, genericShearZ, genericInertiaY, genericInertiaZ, genericTorsion, genericE, genericPoisson, genericAlpha,
    /// genericWeight, stiffenerSpacing, stiffenerThickness, dimension0..2, pillarX, pillarY.
    ValueEdit* field(const QString& name) const;
    QRadioButton* sectionTypeRadio(int index) const;  ///< concrete: 0 Circular, 1 Rectangular, 2 canto variable
    /// Tirante page (type 1): the five shape icons, series and profile combos (from cc::core::tieSeries(); no library involved).
    const QList<QToolButton*>& tieShapeButtons() const { return m_tieButtons; }
    QComboBox* tieSeriesCombo() const { return m_tieSeries; }
    QComboBox* tieProfileCombo() const { return m_tieProfile; }
    /// "Canto del perfil" group of the timber series of variable depth (fields depthStart / depthEnd).
    QWidget* depthBox() const { return m_depthBox; }
    /// Timber settings of the work ("Perfiles de madera": service class, fire resistance). The dialog shows that box on accepting when
    /// an entry becomes timber (PROVISIONAL trigger: the original shows it after the first Aceptar of a timber description).
    void setTimberSettings(const cc::core::TimberSettings& settings) { m_timberSettings = settings; m_timberOriginal = settings; }
    const cc::core::TimberSettings& timberSettings() const { return m_timberSettings; }
    bool timberSettingsChanged() const { return !(m_timberSettings == m_timberOriginal); }
    /// Pilar page (type 2): "Material" tiles (concrete, steel, mixed), concrete "Tipo" tiles (rectangular, circular), the six mixed
    /// tiles, the steel selector (series / profile) and the fields pillarX / pillarY / pillarDiameter (cm).
    const QList<QToolButton*>& pillarMaterialButtons() const { return m_pillarMaterialButtons; }
    const QList<QToolButton*>& pillarConcreteButtons() const { return m_pillarConcreteButtons; }
    const QList<QToolButton*>& pillarMixedButtons() const { return m_pillarMixedButtons; }
    const ProfilePicker& pillarPicker() const { return m_pillarPicker; }
    const ProfilePicker& mixedPicker() const { return m_mixedPicker; }  ///< embedded profile of the mixed pillar
    /// Viga page (type 3): "Familia" tiles (6), "Tipo" tiles (up to 8, the visible ones are those of the family), the check box
    /// "Viga bajo forjado" and the profile selector of the steel / timber families. Fields beamB, beamA, beamI, beamD, beamFlangeDepth,
    /// beamDepthStart, beamDepthEnd (cm); a field is visible only when the type has it (cc::core::beamFields).
    const QList<QToolButton*>& beamFamilyButtons() const { return m_beamFamilyButtons; }
    const QList<QToolButton*>& beamTypeButtons() const { return m_beamTypeButtons; }
    QCheckBox* beamBelowSlabCheck() const { return m_beamBelowSlab; }
    const ProfilePicker& beamPicker() const { return m_beamPicker; }
    /// Static document «CÁLCULO DE TIRANTES EN CYPE 3D» of the Tirante page.
    QTextBrowser* tieDocumentView() const { return m_tieDocument; }
    /// Whether the norm of the work contemplates pillar / beam editing (ed3norma_es_norma_pilares_contemplada / _vigas_...). When not,
    /// the radio is hidden and the entries of that type become generic with the warning #61376 / #61375. Default: both available (the
    /// norm rule was not investigated, TK-081).
    void setElementTypesAvailable(bool pillars, bool beams);
    /// Tooltip texts, by position, exactly as measured.
    static QStringList materialTooltips();
    static QStringList shapeTooltips(cc::core::BarMaterialIcon icon);
    /// Widgets that take the focus with Tab, in the order of the rule (docs/research/ui/tabulacion_regla.md).
    QList<QWidget*> tabChain() const { return m_tabChain; }

    void accept() override;

private:
    struct Binding {
        ValueEdit* edit;
        std::function<double(const cc::core::BarDescription&)> get;
        std::function<void(cc::core::BarDescription&, double)> set;
    };
    cc::core::BarDescription& current() { return m_entries[static_cast<std::size_t>(m_row)].description; }
    const cc::core::BarDescription& current() const { return m_entries[static_cast<std::size_t>(m_row)].description; }
    void build(bool showUsedList);
    QWidget* buildGenericPage();
    QWidget* buildTiePage();
    QWidget* buildPillarPage();
    QWidget* buildBeamPage();
    QWidget* buildExtraPages();
    ProfilePicker makePicker(QWidget* parent, const QString& prefix, cc::core::SectionFamily family);
    /// Shows / checks the "Disposición" radios of a steel pillar or beam picker (hidden when `shown` is false).
    void updateArrangementPanel(ProfilePicker& picker, cc::core::ElementType type, cc::core::ShapeKind shape,
                                cc::core::Arrangement arrangement, bool shown);
    void fillPicker(ProfilePicker& picker, cc::core::SectionFamily family, const std::string& seriesRef, const std::string& profileRef);
    void onPickerChanged(bool pillar, bool seriesChanged);
    void updatePillarPage();
    void updateBeamPage();
    void updateTiePage();
    ValueEdit* makeField(const QString& objectName, const FieldSpec& spec, QWidget* parent, int width = 80);
    void bind(ValueEdit* edit, std::function<double(const cc::core::BarDescription&)> get,
              std::function<void(cc::core::BarDescription&, double)> set);
    void syncUi();
    void rebuildShapes();
    void rebuildSeries();
    void rebuildProfiles();
    void updateExtra();
    void updateTabChain();
    void onIconChosen(cc::core::BarMaterialIcon icon);
    void onShapeChosen(int slot);
    void onSeriesChanged();
    void onProfileChanged();
    void onTypeChanged(int index);
    void onNewSeries();
    void onEditSeries();
    void onManageSeries();
    void ensureLibrarySeries();
    void selectTieShape(int index);
    void refreshUsedList();
    bool usesLibrary(cc::core::BarMaterialIcon icon) const;
    void applyProfile(const QString& seriesReference, const QString& profileReference);
    void setInlineShape(cc::core::ShapeKind kind, const std::vector<double>& dims);
    void updateInlineReference();
    QStringList dimensionLabels() const;

    std::vector<DescribeEntry> m_entries;
    std::vector<DescribeEntry> m_original;
    std::vector<int> m_typeUi, m_typeOriginal;  ///< 0..3, per entry
    std::vector<int> m_slotUi;                  ///< shape slot chosen per entry
    LibraryScratch m_scratch;
    cc::core::ProfileLibrary m_initialLibrary;
    cc::core::LengthUnit m_unit;
    int m_row = 0;
    bool m_syncing = false;

    QListWidget* m_used = nullptr;
    QButtonGroup* m_typeGroup = nullptr;
    QStackedWidget* m_typePages = nullptr;
    QComboBox* m_behaviour = nullptr;
    QButtonGroup* m_materialGroup = nullptr;
    QList<QToolButton*> m_materialButtons;
    QWidget* m_profileSection = nullptr;  ///< "Seleccion del perfil" header + shapes + series
    QWidget* m_shapeHost = nullptr;
    QList<QToolButton*> m_shapeButtons;
    QComboBox* m_series = nullptr;
    QComboBox* m_profile = nullptr;
    QList<QToolButton*> m_tools;
    QStackedWidget* m_extra = nullptr;
    QButtonGroup* m_arrangements = nullptr;
    QList<QRadioButton*> m_arrangementRadios;
    QCheckBox* m_stiffeners = nullptr;
    QWidget* m_stiffenerBox = nullptr;
    QButtonGroup* m_sectionTypes = nullptr;
    QList<QRadioButton*> m_sectionTypeRadios;
    QList<QLabel*> m_dimensionLabels;
    QList<QLabel*> m_dimensionUnits;
    QWidget* m_sectionTypeColumn = nullptr;
    QList<ValueEdit*> m_dimensionEdits;
    QList<QToolButton*> m_tieButtons;
    QComboBox* m_tieSeries = nullptr;
    QComboBox* m_tieProfile = nullptr;
    QTextBrowser* m_tieDocument = nullptr;
    // Pilar page
    QList<QToolButton*> m_pillarMaterialButtons, m_pillarConcreteButtons, m_pillarMixedButtons;
    QStackedWidget* m_pillarStack = nullptr;
    QWidget* m_pillarFields = nullptr;
    QWidget* m_pillarMixedPicker = nullptr;
    ProfilePicker m_pillarPicker, m_mixedPicker;
    QList<QWidget*> m_pillarRowX, m_pillarRowY, m_pillarRowDiameter;
    QWidget* m_pillarSketch = nullptr;
    // Viga page
    QList<QToolButton*> m_beamFamilyButtons, m_beamTypeButtons;
    QWidget* m_beamTypeHost = nullptr;
    QWidget* m_beamTypeHeader = nullptr;
    QWidget* m_beamFieldsBox = nullptr;
    QWidget* m_beamSketch = nullptr;
    QCheckBox* m_beamBelowSlab = nullptr;
    ProfilePicker m_beamPicker;
    QMap<QString, QList<QWidget*>> m_beamRows;  ///< field object name -> (label, edit, unit)
    bool m_pillarsAllowed = true, m_beamsAllowed = true;
    QWidget* m_depthBox = nullptr;
    cc::core::TimberSettings m_timberSettings, m_timberOriginal;
    QList<Binding> m_bindings;
    QList<ValueEdit*> m_fields;
    QList<QWidget*> m_tabChain;
};

/// "Perfiles de madera" box that follows the first Aceptar of a timber description (capture 167 of the user's manual captures): the
/// "Clase de servicio" radios 1 / 2 / 3 with the text of the class and the check box "Comprobar la resistencia al fuego (CTE DB SI.
/// Anejo E)". One Aceptar button.
class TimberServiceDialog : public CypeDialog {
    Q_OBJECT
public:
    explicit TimberServiceDialog(const cc::core::TimberSettings& initial, QWidget* parent = nullptr);
    cc::core::TimberSettings value() const;
    QRadioButton* serviceRadio(int serviceClass) const;
    QCheckBox* fireCheck() const { return m_fire; }
    QString serviceText() const;

private:
    cc::core::TimberSettings m_initial;
    QButtonGroup* m_service = nullptr;
    QLabel* m_text = nullptr;
    QCheckBox* m_fire = nullptr;
};

/// "Describir material" (Geometria > Barras > Material, ~570x375): one group per material present in the selection with the radios
/// "Material seleccionado en datos generales (<material>)" and "Seleccionar material para la barra" + combo. The core has no
/// per-bar grade yet, so the dialog only collects the choice (TK candidate: store it in the description).
struct MaterialChoice {
    bool useProjectMaterial = true;
    QString grade;       ///< when !useProjectMaterial
    QString timberType;  ///< timber, when !useProjectMaterial
};

class DescribeMaterialDialog : public CypeDialog {
    Q_OBJECT
public:
    enum class Group { RolledSteel, Concrete, Timber };
    struct GroupSpec {
        Group group;
        QString projectMaterial;  ///< "S275 (UNE-EN 10025-2)"; timber: the strength class of the work
        QStringList grades;       ///< items of the combo (timber: ignored, the classes depend on the type chosen)
        QString projectTimberType;  ///< timber only: the type of the work ("Aserrada, procedente de coníferas o chopos")
    };
    DescribeMaterialDialog(const std::vector<GroupSpec>& groups, QWidget* parent = nullptr);
    /// Choice of a group (default when the group is not shown).
    MaterialChoice choice(Group group) const;
    QRadioButton* projectRadio(Group group) const;
    QRadioButton* ownRadio(Group group) const;
    QComboBox* gradeCombo(Group group) const;
    QComboBox* timberTypeCombo() const;  ///< timber group only: the four types (nullptr when the group is not shown)
    QStringList groupTitles() const;

private:
    struct Row {
        Group group;
        QRadioButton* project;
        QRadioButton* own;
        QComboBox* combo;
        QString title;
        QComboBox* type = nullptr;  ///< timber
    };
    QList<Row> m_rows;
};

}  // namespace cc::app
