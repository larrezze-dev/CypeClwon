// Geometria > Barras > Seccion (T-P5-07, GUI part): the "Describir" dialog, its numeric fields and "Describir material".
// Never shown on screen (offscreen); the nested dialogs go through replaceable sinks. Texts and values are the ones measured in
// the original (docs/research/ui/describir_barras.md).
#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QToolButton>

#include "BarDescribeDialog.h"
#include "core/describir_catalogue.h"
#include "core/predefined_profiles.h"

using namespace cc::app;
using namespace cc::core;

namespace {

struct Sinks {
    QStringList warnings;
    QStringList infos;
    std::vector<Section> properties;
    std::optional<ProfileSeries> nextSeries;
    int managed = 0;
    int timberBoxes = 0;
    bool timberBoxAccepts = false;
    TimberSettings timberToSet;
    Sinks() {
        describeSink().timberService = [this](QWidget*, TimberSettings& settings) {
            ++timberBoxes;
            if (timberBoxAccepts) settings = timberToSet;
            return timberBoxAccepts;
        };
        profileMessageSink().warn = [this](QWidget*, const QString&, const QString& text) { warnings << text; };
        describeSink().info = [this](QWidget*, const QString& title, const QString& text) { infos << title << text; };
        describeSink().properties = [this](QWidget*, const Section& s) { properties.push_back(s); };
        describeSink().editSeries = [this](QWidget*, const ProfileSeries&, bool, LengthUnit) { return nextSeries; };
        describeSink().manageSeries = [this](QWidget*, LibraryScratch&) { ++managed; };
    }
};

std::unique_ptr<BarDescribeDialog> make(std::vector<DescribeEntry> entries = {{defaultDescription(), {1}}}) {
    return std::make_unique<BarDescribeDialog>(std::move(entries), ProfileLibrary{}, LengthUnit::Centimetres);
}

void clickIcon(BarDescribeDialog& d, BarMaterialIcon icon) { d.materialButton(icon)->click(); }

}  // namespace

TEST(BarDescribeDialog, DefaultsOfTheOriginal) {
    Sinks sinks;
    const auto d = make();
    EXPECT_EQ(d->windowTitle(), "Describir");
    EXPECT_EQ(d->size(), QSize(1051, std::min(890, d->size().height())));
    EXPECT_TRUE(d->typeRadio(0)->isChecked());
    EXPECT_EQ(d->typeRadio(0)->text(), "Genérico");
    EXPECT_EQ(d->typeRadio(1)->text(), "Tirante");
    EXPECT_EQ(d->typeRadio(2)->text(), "Pilar");
    EXPECT_EQ(d->typeRadio(3)->text(), "Viga");
    EXPECT_EQ(d->behaviourCombo()->currentText(), "Lineal");
    EXPECT_EQ(d->behaviourCombo()->count(), 3);
    EXPECT_TRUE(d->materialButton(BarMaterialIcon::RolledSteel)->isChecked());
    EXPECT_EQ(d->seriesCombo()->currentText(), "IPE");
    EXPECT_EQ(d->profileCombo()->currentText(), "IPE 80");
    EXPECT_TRUE(d->arrangementRadio(Arrangement::Simple)->isChecked());
    EXPECT_EQ(d->acceptButton()->text(), "Aceptar");
    EXPECT_EQ(d->cancelButton()->text(), "Cancelar");
    // the clone starts with an empty library: the dialog seeds the predefined series in its working copy
    EXPECT_TRUE(d->libraryChanged());
}

TEST(BarDescribeDialog, LeftPanelListsTheUsedProfiles) {
    Sinks sinks;
    DescribeEntry generic{defaultDescription(BarMaterialIcon::Generic), {2}};
    BarDescribeDialog d({{defaultDescription(), {1}}, generic}, ProfileLibrary{}, LengthUnit::Centimetres);
    ASSERT_NE(d.usedList(), nullptr);
    ASSERT_EQ(d.usedList()->count(), 2);
    EXPECT_EQ(d.usedList()->item(0)->text(), "[Barra] IPE 80");
    EXPECT_EQ(d.usedList()->item(1)->text(), "[Barra] Gen.(19.6 kg/m)");
    d.usedList()->setCurrentRow(1);
    EXPECT_EQ(d.currentRow(), 1);
    EXPECT_TRUE(d.materialButton(BarMaterialIcon::Generic)->isChecked());
    EXPECT_EQ(d.size().width(), 1051);
    BarDescribeDialog without({{defaultDescription(), {1}}}, ProfileLibrary{}, LengthUnit::Centimetres, false);
    EXPECT_EQ(without.usedList(), nullptr);
    EXPECT_EQ(without.size().width(), 859);
}

TEST(BarDescribeDialog, TooltipsOfTheIconsAreTheMeasuredOnes) {
    EXPECT_EQ(BarDescribeDialog::materialTooltips(),
              (QStringList{"Perfil de acero laminado", "Perfil armado de chapas de acero laminado", "Perfil de acero conformado",
                           "Perfil de madera", "Perfil de aluminio extruido", "Sección especial de aluminio", "Barra de hormigón",
                           "Perfil genérico"}));
    EXPECT_EQ(BarDescribeDialog::shapeTooltips(BarMaterialIcon::RolledSteel),
              (QStringList{"Doble T", "C simple", "T simple", "Angular simétrico", "Angular", "Platabanda", "Redondo macizo", "Cuadrado macizo"}));
    EXPECT_EQ(BarDescribeDialog::shapeTooltips(BarMaterialIcon::RolledSteelFamily2).size(), 11);
    EXPECT_EQ(BarDescribeDialog::shapeTooltips(BarMaterialIcon::ColdFormedSteel).size(), 16);
    EXPECT_EQ(BarDescribeDialog::shapeTooltips(BarMaterialIcon::ExtrudedAluminium).size(), 10);
    EXPECT_EQ(BarDescribeDialog::shapeTooltips(BarMaterialIcon::Timber),
              (QStringList{"Redondo macizo", "Cuadrado macizo", "Rectángulo macizo", "Rectangular, canto variable"}));
    EXPECT_TRUE(BarDescribeDialog::shapeTooltips(BarMaterialIcon::Concrete).isEmpty());
    EXPECT_TRUE(BarDescribeDialog::shapeTooltips(BarMaterialIcon::Generic).isEmpty());
    Sinks sinks;
    const auto d = make();
    EXPECT_EQ(d->materialButton(BarMaterialIcon::Generic)->toolTip(), "Perfil genérico");
    ASSERT_EQ(d->shapeButtons().size(), 8);
    EXPECT_EQ(d->shapeButtons()[0]->toolTip(), "Doble T");
    EXPECT_EQ(d->button("info")->toolTip(), "Propiedades del perfil seleccionado");
    EXPECT_EQ(d->button("edit")->toolTip(), "Edición del tipo seleccionado.");
    EXPECT_EQ(d->button("table")->toolTip(), "Editar la lista de elementos");
    EXPECT_EQ(d->button("new")->toolTip(), "Nuevo");
}

TEST(BarDescribeDialog, ShapesPerMaterial) {
    Sinks sinks;
    const auto d = make();
    clickIcon(*d, BarMaterialIcon::ColdFormedSteel);
    EXPECT_EQ(d->shapeButtons().size(), 16);
    clickIcon(*d, BarMaterialIcon::Timber);
    EXPECT_EQ(d->shapeButtons().size(), 4);
    clickIcon(*d, BarMaterialIcon::Concrete);
    EXPECT_EQ(d->shapeButtons().size(), 0);
    clickIcon(*d, BarMaterialIcon::Generic);
    EXPECT_EQ(d->shapeButtons().size(), 0);
    clickIcon(*d, BarMaterialIcon::RolledSteel);
    EXPECT_EQ(d->shapeButtons().size(), 8);
    EXPECT_EQ(d->seriesCombo()->currentText(), "IPE");
}

TEST(BarDescribeDialog, SeriesFollowTheShape) {
    Sinks sinks;
    const auto d = make();
    d->shapeButtons()[1]->click();  // C simple: UPN
    EXPECT_EQ(d->seriesCombo()->currentText(), "UPN");
    EXPECT_EQ(d->entries()[0].description.shape, ShapeKind::Channel);
    d->shapeButtons()[3]->click();  // Angular simetrico: L
    EXPECT_EQ(d->seriesCombo()->currentText(), "L");
    d->shapeButtons()[0]->click();
    EXPECT_EQ(d->seriesCombo()->currentText(), "IPE");
    // choosing another series / profile of the combos
    d->seriesCombo()->setCurrentIndex(d->seriesCombo()->findText("HEA"));
    emit d->seriesCombo()->activated(d->seriesCombo()->currentIndex());
    EXPECT_EQ(d->entries()[0].description.seriesRef, "HEA");
    d->profileCombo()->setCurrentIndex(1);
    emit d->profileCombo()->activated(1);
    EXPECT_EQ(d->entries()[0].description.profileRef, d->profileCombo()->currentText().toStdString());
}

TEST(BarDescribeDialog, ArrangementsOfTheRolledI) {
    Sinks sinks;
    const auto d = make();
    int visible = 0;
    for (Arrangement a : {Arrangement::Simple, Arrangement::SimpleWithHaunches, Arrangement::WithConcreteSlab, Arrangement::DoubleBoxWelded,
                          Arrangement::DoubleBoxBattened, Arrangement::DoubleBoxGeneric, Arrangement::HalfProfile, Arrangement::SideFlatPlates,
                          Arrangement::Boyd, Arrangement::DoubleIWelded})
        if (d->arrangementRadio(a)->isVisibleTo(d.get())) ++visible;
    EXPECT_EQ(visible, 9);
    EXPECT_EQ(d->arrangementRadio(Arrangement::Boyd)->text(), "Boyd (alma aligerada)");
    d->arrangementRadio(Arrangement::HalfProfile)->click();
    EXPECT_EQ(d->entries()[0].description.arrangement, Arrangement::HalfProfile);
    // a shape that does not offer it brings the arrangement back to "Perfil simple"
    d->shapeButtons()[1]->click();
    EXPECT_EQ(d->entries()[0].description.arrangement, Arrangement::Simple);
    EXPECT_FALSE(d->arrangementRadio(Arrangement::Boyd)->isVisibleTo(d.get()));
}

TEST(BarDescribeDialog, GenericSectionDefaultsAndFieldFormats) {
    Sinks sinks;
    const auto d = make();
    clickIcon(*d, BarMaterialIcon::Generic);
    EXPECT_EQ(d->field("genericWidthLeft")->text(), "25.0");
    EXPECT_EQ(d->field("genericDepthTop")->text(), "25.0");
    EXPECT_EQ(d->field("genericArea")->text(), "25.00");
    EXPECT_EQ(d->field("genericShearY")->text(), "20.83");
    EXPECT_EQ(d->field("genericInertiaY")->text(), "52.08");
    EXPECT_EQ(d->field("genericTorsion")->text(), "87.50");
    EXPECT_EQ(d->field("genericE")->text(), "206010.00");
    EXPECT_EQ(d->field("genericPoisson")->text(), "0.300");
    EXPECT_EQ(d->field("genericAlpha")->text(), "0.000012");
    EXPECT_EQ(d->field("genericWeight")->text(), "77.01");
}

TEST(BarDescribeDialog, GenericCharacteristicsAreNotRecomputedFromTheGeometry) {
    Sinks sinks;
    const auto d = make();
    clickIcon(*d, BarMaterialIcon::Generic);
    d->field("genericWidthLeft")->setText("50");
    EXPECT_TRUE(d->field("genericWidthLeft")->commit());
    EXPECT_EQ(d->field("genericWidthLeft")->text(), "50.0");
    EXPECT_DOUBLE_EQ(d->entries()[0].description.generic.widthLeftM, 0.05);
    EXPECT_EQ(d->field("genericArea")->text(), "25.00");  // measured: A stays 25.00
    EXPECT_DOUBLE_EQ(d->entries()[0].description.generic.areaM2, 25.0e-4);
}

TEST(BarDescribeDialog, ValidationMessagesOfTheOriginal) {
    Sinks sinks;
    const auto d = make();
    clickIcon(*d, BarMaterialIcon::Generic);
    ValueEdit* width = d->field("genericWidthLeft");
    width->setText("0");
    EXPECT_FALSE(width->commit());
    ASSERT_EQ(sinks.warnings.size(), 1);
    EXPECT_EQ(sinks.warnings[0], "El valor de 'Ancho izquierda (1)' debe estar entre 0.1 y 10000.0.");
    EXPECT_EQ(width->text(), "25.0");  // keeps the previous value
    sinks.warnings.clear();
    width->setText("abc");
    EXPECT_FALSE(width->commit());
    EXPECT_EQ(sinks.warnings.value(0), "La cadena introducida no es un número.");
    width->setText("12,5");  // only the decimal point is accepted
    EXPECT_FALSE(width->commit());
    EXPECT_EQ(sinks.warnings.value(1), "La cadena introducida no es un número.");
    width->setText("");
    EXPECT_FALSE(width->commit());
    sinks.warnings.clear();
    width->setText("10000.0");  // the limits are accepted
    EXPECT_TRUE(width->commit());
    width->setText("0.1");
    EXPECT_TRUE(width->commit());
    width->setText("10000.1");
    EXPECT_FALSE(width->commit());
    EXPECT_EQ(sinks.warnings.size(), 1);
    sinks.warnings.clear();
    d->field("genericPoisson")->setText("60");
    EXPECT_FALSE(d->field("genericPoisson")->commit());
    EXPECT_EQ(sinks.warnings.value(0), "El valor de 'Módulo de Poisson' debe estar entre 0.001  y 50.000 .");  // verbatim, stray spaces
    sinks.warnings.clear();
    d->field("genericArea")->setText("0");
    EXPECT_FALSE(d->field("genericArea")->commit());
    EXPECT_EQ(sinks.warnings.value(0), "El valor de 'Área (A)' debe estar entre 0.01 y 1000000.00.");
    sinks.warnings.clear();
    d->field("genericE")->setText("0.05");
    EXPECT_FALSE(d->field("genericE")->commit());
    EXPECT_EQ(sinks.warnings.value(0), "El valor de 'Módulo de elasticidad' debe estar entre 0.10 y 10000000.00.");
    sinks.warnings.clear();
    d->field("genericAlpha")->setText("0");
    EXPECT_FALSE(d->field("genericAlpha")->commit());
    EXPECT_EQ(sinks.warnings.value(0), "El valor de 'Coeficiente de dilatación' debe estar entre 0.000001 y 100.000000.");
}

TEST(BarDescribeDialog, FieldTooltipIsNameInBoldPlusRange) {
    Sinks sinks;
    const auto d = make();
    clickIcon(*d, BarMaterialIcon::Generic);
    EXPECT_EQ(d->field("genericWidthLeft")->toolTip(), "<b>Ancho izquierda (1)</b><br>Debe estar entre 0.1 y 10000.0.");
    EXPECT_EQ(d->field("genericTorsion")->toolTip(), "<b>Inercia a torsión (It)</b><br>Debe estar entre 0.01 y 100000000.00.");
}

TEST(BarDescribeDialog, GenericInfoBoxText) {
    Sinks sinks;
    const auto d = make();
    clickIcon(*d, BarMaterialIcon::Generic);
    d->button("genericInfo")->click();
    ASSERT_EQ(sinks.infos.size(), 2);
    EXPECT_EQ(sinks.infos[0], "Características mecánicas");
    EXPECT_EQ(sinks.infos[1], "En la sección transversal, se considera que los ejes de inercia principales coinciden con los ejes locales Y y Z.");
}

TEST(BarDescribeDialog, ConcreteDimensions) {
    Sinks sinks;
    const auto d = make();
    clickIcon(*d, BarMaterialIcon::Concrete);
    EXPECT_EQ(d->field("dimension0")->text(), "30.0");
    EXPECT_EQ(d->field("dimension1")->text(), "30.0");
    EXPECT_TRUE(d->sectionTypeRadio(1)->isChecked());
    EXPECT_EQ(d->sectionTypeRadio(0)->text(), "Circular");
    EXPECT_EQ(d->sectionTypeRadio(2)->text(), "Rectangular de canto variable");
    EXPECT_FALSE(d->field("dimension2")->isVisibleTo(d.get()));
    d->field("dimension0")->setText("0.9");
    EXPECT_FALSE(d->field("dimension0")->commit());
    EXPECT_EQ(sinks.warnings.value(0), "El valor de 'Ancho' debe estar entre 1.0 y 1000.0.");
    d->field("dimension0")->setText("1000");  // 1 and 1000 are accepted
    EXPECT_TRUE(d->field("dimension0")->commit());
    d->field("dimension0")->setText("40");
    EXPECT_TRUE(d->field("dimension0")->commit());
    EXPECT_NEAR(d->entries()[0].description.inlineSection->dims[0], 0.40, 1e-12);
    d->sectionTypeRadio(0)->click();  // circular: one diameter
    EXPECT_EQ(d->entries()[0].description.inlineSection->kind, ShapeKind::ConcreteCircular);
    EXPECT_FALSE(d->field("dimension1")->isVisibleTo(d.get()));
    d->sectionTypeRadio(2)->click();  // variable: width, initial and final depth
    EXPECT_TRUE(d->field("dimension2")->isVisibleTo(d.get()));
}

TEST(BarDescribeDialog, TimberStartsAsRound100) {
    Sinks sinks;
    const auto d = make();
    clickIcon(*d, BarMaterialIcon::Timber);
    const BarDescription& desc = d->entries()[0].description;
    // The captured series of timber are in the library: "Redondo" / "Ø100" is the default of the original.
    EXPECT_EQ(desc.shape, ShapeKind::TimberRound);
    EXPECT_EQ(desc.seriesRef, "Redondo");
    EXPECT_EQ(desc.profileRef, "Ø100");
    EXPECT_EQ(barLabelText(desc), "Ø100");
    EXPECT_EQ(d->usedList()->item(0)->text(), "[Barra] Ø100");
    d->shapeButtons()[2]->click();  // Rectángulo macizo
    EXPECT_EQ(d->entries()[0].description.shape, ShapeKind::TimberRectangular);
    EXPECT_EQ(d->entries()[0].description.seriesRef, "Cabios/Viguetas");
    EXPECT_EQ(d->entries()[0].description.profileRef, "100x70");
}

TEST(BarDescribeDialog, TimberSeriesAndProfilesOfTheCaptures) {
    Sinks sinks;
    const auto d = make();
    clickIcon(*d, BarMaterialIcon::Timber);
    const auto items = [](QComboBox* c) {
        QStringList out;
        for (int i = 0; i < c->count(); ++i) out << c->itemText(i);
        return out;
    };
    EXPECT_EQ(items(d->seriesCombo()), (QStringList{"Redondo"}));
    EXPECT_EQ(items(d->profileCombo()), (QStringList{"Ø100", "Ø120", "Ø130", "Ø140", "Ø150", "Ø180", "Ø200"}));
    d->shapeButtons()[1]->click();  // Cuadrado macizo: Maciza, DUO/TRIO, Laminada
    EXPECT_EQ(items(d->seriesCombo()), (QStringList{"Maciza", "DUO/TRIO", "Laminada"}));
    EXPECT_EQ(d->profileCombo()->currentText(), "S-100x100");
    d->shapeButtons()[2]->click();  // Rectángulo macizo: 32 series, Cabios/Viguetas first
    EXPECT_EQ(d->seriesCombo()->count(), 32);
    EXPECT_EQ(d->seriesCombo()->itemText(0), "Cabios/Viguetas");
    EXPECT_EQ(d->seriesCombo()->itemText(1), "Maciza h100");
    EXPECT_EQ(d->seriesCombo()->itemText(31), "Laminada b260");
    EXPECT_EQ(d->profileCombo()->count(), 11);
    d->seriesCombo()->setCurrentIndex(d->seriesCombo()->findText("Laminada b60"));
    emit d->seriesCombo()->activated(d->seriesCombo()->currentIndex());
    EXPECT_EQ(d->profileCombo()->count(), 57);
    EXPECT_EQ(d->profileCombo()->itemText(56), "GL-1280x60");
    // The properties are computed from the designation: S-100x40 is a 40 wide, 100 deep rectangle.
    d->seriesCombo()->setCurrentIndex(d->seriesCombo()->findText("Maciza h100"));
    emit d->seriesCombo()->activated(d->seriesCombo()->currentIndex());
    EXPECT_EQ(d->profileCombo()->itemText(0), "S-100x40");
    const BarDescription& desc = d->entries()[0].description;
    const auto section = resolveSection(desc, d->library());
    ASSERT_TRUE(section.has_value());
    const auto props = effectiveProperties(*section);
    ASSERT_TRUE(props.has_value());
    EXPECT_NEAR(props->area, 0.1 * 0.04, 1e-12);
}

TEST(BarDescribeDialog, VariableDepthTimberHasTheCantoGroup) {
    Sinks sinks;
    const auto d = make();
    clickIcon(*d, BarMaterialIcon::Timber);
    d->shapeButtons()[3]->click();  // Rectangular, canto variable
    const BarDescription& desc = d->entries()[0].description;
    EXPECT_EQ(desc.shape, ShapeKind::TimberRectangularVariable);
    EXPECT_EQ(desc.profileRef, "RV-60");
    EXPECT_EQ(d->profileCombo()->count(), 11);
    EXPECT_EQ(d->profileCombo()->itemText(10), "RV-260");
    EXPECT_TRUE(d->depthBox()->isVisibleTo(d.get()));
    EXPECT_EQ(d->field("depthStart")->text(), "100.0");
    EXPECT_EQ(d->field("depthEnd")->text(), "100.0");
    EXPECT_TRUE(d->arrangementRadio(Arrangement::Simple)->isChecked());
    d->field("depthEnd")->setText("150");
    d->field("depthEnd")->commit();
    EXPECT_NEAR(d->entries()[0].description.depthEndM, 0.15, 1e-12);
    const auto section = resolveSection(d->entries()[0].description, d->library());
    ASSERT_TRUE(section.has_value());
    EXPECT_NEAR(section->dims[2], 0.15, 1e-12);
    d->shapeButtons()[0]->click();
    EXPECT_FALSE(d->depthBox()->isVisibleTo(d.get()));
}

TEST(BarDescribeDialog, ShapesWithCatalogueDataOfTheOtherFamilies) {
    Sinks sinks;
    const auto d = make();
    clickIcon(*d, BarMaterialIcon::RolledSteelFamily2);
    EXPECT_EQ(d->seriesCombo()->currentText(), "IPC");
    EXPECT_EQ(d->seriesCombo()->count(), 1);
    EXPECT_EQ(d->profileCombo()->count(), 85);
    EXPECT_EQ(d->profileCombo()->currentText(), "IPC 533x203x62.6");
    d->shapeButtons()[1]->click();  // a shape without data: no series
    EXPECT_EQ(d->seriesCombo()->count(), 0);
    clickIcon(*d, BarMaterialIcon::ColdFormedSteel);
    EXPECT_EQ(d->seriesCombo()->currentText(), "C");  // first shape with data (U conformado rigidizado)
    EXPECT_EQ(d->profileCombo()->currentText(), "CF-60x2.0");
    d->shapeButtons()[9]->click();  // U conformado simple
    EXPECT_EQ(d->seriesCombo()->currentText(), "U");
    EXPECT_EQ(d->profileCombo()->count(), 14);
    EXPECT_EQ(d->profileCombo()->currentText(), "UF-60x3");
    clickIcon(*d, BarMaterialIcon::ExtrudedAluminium);
    EXPECT_EQ(d->seriesCombo()->currentText(), "(EP) I");
    EXPECT_EQ(d->profileCombo()->count(), 11);
    EXPECT_EQ(d->profileCombo()->currentText(), "I-50x80x6");
}

TEST(BarDescribeDialog, TiePageOffersTheFiveShapesWithTheirSeries) {
    Sinks sinks;
    const auto d = make();
    d->typeRadio(1)->click();
    ASSERT_EQ(d->tieShapeButtons().size(), 5);
    EXPECT_EQ(d->tieSeriesCombo()->currentText(), "Pletinas");  // the original opens on Platabanda
    EXPECT_EQ(d->tieProfileCombo()->currentText(), "FL 20 x 4");
    EXPECT_EQ(d->tieProfileCombo()->count(), 331);
    d->tieShapeButtons()[0]->click();
    EXPECT_EQ(d->tieSeriesCombo()->currentText(), "L");
    EXPECT_EQ(d->tieProfileCombo()->currentText(), "L 35 x 35 x 4");
    EXPECT_EQ(d->tieProfileCombo()->count(), 179);
    d->tieShapeButtons()[1]->click();
    EXPECT_EQ(d->tieSeriesCombo()->currentText(), "L");
    EXPECT_EQ(d->tieProfileCombo()->currentText(), "L 100 x 65 x 7");
    EXPECT_EQ(d->tieProfileCombo()->count(), 31);
    d->tieShapeButtons()[3]->click();
    EXPECT_EQ(d->tieSeriesCombo()->currentText(), "R");
    EXPECT_EQ(d->tieProfileCombo()->count(), 70);
    d->tieShapeButtons()[4]->click();
    EXPECT_EQ(d->tieSeriesCombo()->currentText(), "Barras cuadradas");
    EXPECT_EQ(d->tieProfileCombo()->currentText(), "SQ 90 x 90");
    EXPECT_EQ(d->tieProfileCombo()->count(), 8);
    // The tie series are not in the library: the rolled steel "L" of the library keeps its own profiles.
    EXPECT_EQ(d->library().find("L")->kind, ShapeKind::AngleEqual);
}

TEST(TimberServiceDialog, ShowsTheClassTextAndTheFireCheck) {
    TimberSettings settings;
    TimberServiceDialog dialog(settings);
    EXPECT_EQ(dialog.windowTitle(), "Perfiles de madera");
    EXPECT_TRUE(dialog.serviceRadio(1)->isChecked());
    EXPECT_EQ(dialog.serviceText(), QString::fromStdString(timberServiceClassText(1)));
    EXPECT_TRUE(dialog.serviceText().startsWith("Clase de servicio 1: se caracteriza"));
    EXPECT_EQ(dialog.fireCheck()->text(), "Comprobar la resistencia al fuego (CTE DB SI. Anejo E)");
    EXPECT_FALSE(dialog.fireCheck()->isChecked());
    dialog.serviceRadio(2)->setChecked(true);
    dialog.fireCheck()->setChecked(true);
    EXPECT_EQ(dialog.serviceText(), QString::fromStdString(timberServiceClassText(2)));
    EXPECT_EQ(dialog.value().serviceClass, 2);
    EXPECT_TRUE(dialog.value().fireResistance);
}

TEST(BarDescribeDialog, TimberBoxShownWhenAnEntryBecomesTimber) {
    Sinks sinks;
    sinks.timberBoxAccepts = true;
    sinks.timberToSet.serviceClass = 3;
    auto d = make();
    d->accept();  // steel stays steel: no box
    EXPECT_EQ(sinks.timberBoxes, 0);
    auto t = make();
    clickIcon(*t, BarMaterialIcon::Timber);
    t->setTimberSettings(TimberSettings{});
    t->accept();
    EXPECT_EQ(sinks.timberBoxes, 1);
    EXPECT_TRUE(t->timberSettingsChanged());
    EXPECT_EQ(t->timberSettings().serviceClass, 3);
}

TEST(BarDescribeDialog, ProfilePropertiesRowsOfTheIpe80) {
    const ProfileSeries* ipe = nullptr;
    for (const ProfileSeries& s : predefinedSeries())
        if (s.reference == "IPE") ipe = &s;
    ASSERT_NE(ipe, nullptr);
    const Section* section = ipe->findProfile("IPE 80");
    ASSERT_NE(section, nullptr);
    const auto rows = profilePropertyRows(*section);
    QStringList labels;
    for (const auto& r : rows) labels << r.first;
    EXPECT_EQ(labels, (QStringList{"Canto total:", "Anchura del ala:", "Espesor del alma:", "Espesor del ala:", "Radio de acuerdo entre ala y alma:",
                                   "Área:", "Inercia a flexión Iyy:", "Módulo plástico Zyy:", "Inercia a flexión Izz:", "Módulo plástico Zzz:",
                                   "Inercia a torsión:", "Módulo de alabeo:", "Peso:"}));
    EXPECT_EQ(rows[0].second, "80.0 mm");   // dimensions with one decimal
    EXPECT_EQ(rows[1].second, "46.0 mm");
    EXPECT_EQ(rows[2].second, "3.8 mm");
    EXPECT_EQ(rows[3].second, "5.2 mm");
    EXPECT_EQ(rows[4].second, "5.0 mm");
    EXPECT_TRUE(rows[5].second.endsWith(" cm²"));
    EXPECT_TRUE(rows[6].second.endsWith(" cm4"));
    EXPECT_TRUE(rows[7].second.endsWith(" cm³"));
    EXPECT_TRUE(rows[11].second.endsWith(" cm6"));
    EXPECT_TRUE(rows[12].second.endsWith(" kg/m"));
    // properties with two decimals
    EXPECT_EQ(rows[5].second.section(' ', 0, 0).section('.', 1).size(), 2);
    for (const QString& label : labels) {
        EXPECT_FALSE(label.contains("Wel"));
        EXPECT_FALSE(label.contains("cortante"));
    }
    EXPECT_NEAR(rows[5].second.section(' ', 0, 0).toDouble(), 7.60, 0.1);
    EXPECT_NEAR(rows[12].second.section(' ', 0, 0).toDouble(), 5.97, 0.1);
}

namespace {
const cc::core::Section* catalogueProfile(const std::string& series, const std::string& profile) {
    for (const auto& s : cc::core::describirSeries())
        if (s.reference == series)
            if (const auto* p = s.findProfile(profile)) return p;
    return nullptr;
}
QStringList valuesOf(const std::vector<std::pair<QString, QString>>& rows, QStringList* labels = nullptr) {
    QStringList values;
    for (const auto& r : rows) {
        values << r.second;
        if (labels) *labels << r.first;
    }
    return values;
}
}  // namespace

// The rows of the four (i) dialogs captured in the original (describir_info_perfiles.tsv): the values are the ones of the captures.
TEST(BarDescribeDialog, ProfilePropertiesOfTheCatalogueShapesMatchTheCaptures) {
    {   // armado IPC 533x203x62.6: 14 rows
        const auto* p = catalogueProfile("IPC", "IPC 533x203x62.6");
        ASSERT_NE(p, nullptr);
        QStringList labels;
        const QStringList v = valuesOf(profilePropertyRows(*p), &labels);
        EXPECT_EQ(labels.size(), 14);
        EXPECT_EQ(labels.front(), "Canto exterior del alma:");
        EXPECT_EQ(labels[11], "Módulo plástico Zyy:");
        EXPECT_EQ(v, (QStringList{"533.4 mm", "203.2 mm", "7.9 mm", "9.5 mm", "79.25 cm²", "35455.70 cm4", "1330.56 cm4", "20.07 cm4",
                                  "28.96 cm²", "36.57 cm²", "912999.76 cm6", "1533.94 cm³", "204.15 cm³", "62.21 kg/m"}));
    }
    {   // aluminio I-50x80x6: 12 rows
        const auto* p = catalogueProfile("(EP) I", "I-50x80x6");
        ASSERT_NE(p, nullptr);
        const QStringList v = valuesOf(profilePropertyRows(*p));
        EXPECT_EQ(v, (QStringList{"50.0 mm", "80.0 mm", "6.0 mm", "6.0 mm", "11.88 cm²", "49.50 cm4", "23.29 cm³", "51.27 cm4", "19.54 cm³",
                                  "1.43 cm4", "248.14 cm6", "3.21 kg/m"}));
    }
    {   // conformado UF-60x3: 13 rows, A / Iyy tabulated, the rest approximate (tolerances documented in describir_barras_capturas_manuales.md)
        const auto* p = catalogueProfile("U", "UF-60x3");
        ASSERT_NE(p, nullptr);
        QStringList labels;
        const QStringList v = valuesOf(profilePropertyRows(*p), &labels);
        ASSERT_EQ(v.size(), 13);
        EXPECT_EQ(labels[5], "Coordenada 'y' del centro de gravedad:");
        EXPECT_EQ(v[0], "60.0 mm");
        EXPECT_EQ(v[1], "30.0 mm");
        EXPECT_EQ(v[2], "3.0 mm");
        EXPECT_EQ(v[3], "3.0 mm");
        EXPECT_EQ(v[4], "3.30 cm²");   // tabulated
        EXPECT_EQ(v[6], "17.50 cm4");  // tabulated
        EXPECT_EQ(v[12], "2.59 kg/m");
        const auto num = [&](int i) { return v[i].section(' ', 0, 0).toDouble(); };
        EXPECT_NEAR(num(5), -6.1, 0.1);        // y of the centre of gravity
        EXPECT_NEAR(num(7), 2.82, 2.82 * 0.02);  // Izz, approximate (original 2.82)
        EXPECT_NEAR(num(8), 0.10, 0.01);       // It (original 0.10)
        EXPECT_NEAR(num(9), 1.42, 1.42 * 0.02);   // Avy
        EXPECT_NEAR(num(10), 1.42, 1.42 * 0.02);  // Avz
        EXPECT_NEAR(num(11), 14.78, 14.78 * 0.06);  // warping, approximate
    }
    {   // madera redondo Ø100: 5 rows
        const auto* p = catalogueProfile("Redondo", "\xC3\x98" "100");
        ASSERT_NE(p, nullptr);
        QStringList labels;
        const QStringList v = valuesOf(profilePropertyRows(*p), &labels);
        EXPECT_EQ(labels, (QStringList{"Diámetro:", "Área:", "Inercia a flexión:", "Módulo plástico:", "Inercia a torsión:"}));
        EXPECT_EQ(v, (QStringList{"100.0 mm", "78.54 cm²", "490.87 cm4", "166.67 cm³", "981.75 cm4"}));
    }
}

TEST(BarDescribeDialog, ProfileInfoButtonPassesTheSelectedProfile) {
    Sinks sinks;
    const auto d = make();
    d->button("info")->click();
    ASSERT_EQ(sinks.properties.size(), 1u);
    EXPECT_EQ(sinks.properties[0].reference, "IPE 80");
}

TEST(BarDescribeDialog, SeriesButtons) {
    Sinks sinks;
    const auto d = make();
    ProfileSeries mine;
    mine.reference = "MIA";
    mine.description = "Prueba";
    mine.kind = ShapeKind::IParallel;
    Section p;
    p.reference = "MIA 1";
    p.kind = ShapeKind::IParallel;
    p.dims = {0.2, 0.1, 0.006, 0.009, 0.01};
    mine.profiles = {p};
    sinks.nextSeries = mine;
    d->button("new")->click();
    EXPECT_NE(d->library().find("MIA"), nullptr);
    EXPECT_EQ(d->seriesCombo()->currentText(), "MIA");
    EXPECT_EQ(d->profileCombo()->currentText(), "MIA 1");
    EXPECT_EQ(d->entries()[0].description.seriesRef, "MIA");
    sinks.nextSeries.reset();  // cancel: nothing changes
    d->button("edit")->click();
    EXPECT_EQ(d->seriesCombo()->currentText(), "MIA");
    d->button("table")->click();
    EXPECT_EQ(sinks.managed, 1);
}

TEST(BarDescribeDialog, ToolButtonsAreNotTabStopsAndOrderFollowsTheRule) {
    Sinks sinks;
    const auto d = make();
    for (const char* name : {"new", "edit", "table", "info"}) EXPECT_EQ(d->button(QString::fromLatin1(name))->focusPolicy(), Qt::NoFocus);
    const QList<QWidget*> chain = d->tabChain();
    const auto at = [&](QWidget* w) { return chain.indexOf(w); };
    ASSERT_GE(at(d->usedList()), 0);
    EXPECT_LT(at(d->usedList()), at(d->typeRadio(0)));
    EXPECT_LT(at(d->typeRadio(0)), at(d->typeRadio(3)));
    EXPECT_LT(at(d->typeRadio(3)), at(d->behaviourCombo()));
    EXPECT_LT(at(d->behaviourCombo()), at(d->materialButton(BarMaterialIcon::RolledSteel)));
    EXPECT_LT(at(d->materialButton(BarMaterialIcon::Generic)), at(d->shapeButtons()[0]));
    EXPECT_LT(at(d->shapeButtons()[7]), at(d->seriesCombo()));
    EXPECT_LT(at(d->seriesCombo()), at(d->profileCombo()));
    EXPECT_LT(at(d->profileCombo()), at(d->arrangementRadio(Arrangement::Simple)));
    EXPECT_EQ(chain[chain.size() - 2], d->acceptButton());  // the footer buttons are the last ones
    EXPECT_EQ(chain.last(), d->cancelButton());
    // generic: geometry column, then mechanical characteristics, then material (top to bottom, left to right)
    EXPECT_LT(at(d->field("genericDepthTop")), at(d->field("genericArea")));
    EXPECT_LT(at(d->field("genericTorsion")), at(d->field("genericE")));
    // real focus chain of Qt follows the stored order
    // (Qt keeps labels and the like in the chain; they never take the focus, so the first widget that can is the one that counts)
    QWidget* next = d->behaviourCombo()->nextInFocusChain();
    while (next && !(next->focusPolicy() & Qt::TabFocus)) next = next->nextInFocusChain();
    EXPECT_EQ(next, d->materialButton(BarMaterialIcon::RolledSteel));
}

TEST(BarDescribeDialog, TieColumnAndBeamTypesAreNotYetApplicable) {
    Sinks sinks;
    auto d = make();
    d->typeRadio(1)->click();  // Tirante
    EXPECT_EQ(d->typeIndex(), 1);
    d->accept();
    EXPECT_EQ(sinks.warnings.value(0), "Esta función aún no está disponible.");
    EXPECT_NE(d->result(), QDialog::Accepted);
    sinks.warnings.clear();
    d->typeRadio(2)->click();  // Pilar
    d->accept();
    EXPECT_EQ(sinks.warnings.value(0), "Esta función aún no está disponible.");
    sinks.warnings.clear();
    d->typeRadio(0)->click();  // back to Generico: accepted
    d->accept();
    EXPECT_TRUE(sinks.warnings.isEmpty());
    EXPECT_EQ(d->result(), QDialog::Accepted);
}

TEST(BarDescribeDialog, PillarAndBeamPagesCarryTheMeasuredControls) {
    Sinks sinks;
    const auto d = make();
    d->typeRadio(2)->click();
    EXPECT_EQ(d->field("pillarX")->text(), "30");
    EXPECT_EQ(d->field("pillarY")->text(), "30");
    d->field("pillarX")->setText("5");
    EXPECT_FALSE(d->field("pillarX")->commit());
    EXPECT_EQ(sinks.warnings.value(0), "El valor de 'Dimensión X' debe estar entre 10 y 1000.");
    d->typeRadio(3)->click();
    EXPECT_EQ(d->field("beamB")->text(), "30.0");
    EXPECT_EQ(d->field("beamI")->text(), "15.0");
}

TEST(BarDescribeDialog, AcceptValidatesTheDescription) {
    Sinks sinks;
    auto d = make();
    clickIcon(*d, BarMaterialIcon::SpecialAluminium);
    d->accept();
    EXPECT_EQ(sinks.warnings.value(0), "Esta función aún no está disponible.");
    clickIcon(*d, BarMaterialIcon::Generic);
    d->field("genericShearY")->setText("100");  // Avy > A: refused on accepting (provisional)
    d->field("genericShearY")->commit();
    sinks.warnings.clear();
    d->accept();
    EXPECT_EQ(sinks.warnings.value(0), "Los valores de las áreas de cortante 'Avy' y 'Avz' no pueden ser mayores que el valor del área de la sección.");
    d->field("genericShearY")->setText("20");
    d->field("genericShearY")->commit();
    sinks.warnings.clear();
    d->accept();
    EXPECT_TRUE(sinks.warnings.isEmpty());
    EXPECT_EQ(d->result(), QDialog::Accepted);
    EXPECT_EQ(d->entries()[0].description.materialIcon, BarMaterialIcon::Generic);
}

TEST(BarDescribeDialog, BehaviourComboIsStored) {
    Sinks sinks;
    const auto d = make();
    d->behaviourCombo()->setCurrentIndex(1);
    EXPECT_EQ(d->entries()[0].description.behaviour, BehaviourKind::TensionOnly);
    EXPECT_EQ(d->behaviourCombo()->itemText(1), "Sólo tracción");
    EXPECT_EQ(d->behaviourCombo()->itemText(2), "Sólo compresión");
}

TEST(DescribeMaterialDialog, GroupsAndChoices) {
    DescribeMaterialDialog dialog({{DescribeMaterialDialog::Group::RolledSteel, "S275 (UNE-EN 10025-2)", {"S235", "S275", "S355"}},
                                   {DescribeMaterialDialog::Group::Concrete, "C25/30", {"C12/15", "C16/20"}}});
    EXPECT_EQ(dialog.windowTitle(), "Describir material");
    EXPECT_EQ(dialog.groupTitles(), (QStringList{"Acero laminado", "Hormigón para vigas de forjado"}));
    EXPECT_EQ(dialog.projectRadio(DescribeMaterialDialog::Group::RolledSteel)->text(),
              "Material seleccionado en datos generales (S275 (UNE-EN 10025-2))");
    EXPECT_EQ(dialog.ownRadio(DescribeMaterialDialog::Group::RolledSteel)->text(), "Seleccionar material para la barra");
    EXPECT_TRUE(dialog.projectRadio(DescribeMaterialDialog::Group::RolledSteel)->isChecked());
    EXPECT_TRUE(dialog.choice(DescribeMaterialDialog::Group::RolledSteel).useProjectMaterial);
    EXPECT_FALSE(dialog.gradeCombo(DescribeMaterialDialog::Group::RolledSteel)->isVisibleTo(&dialog));
    dialog.ownRadio(DescribeMaterialDialog::Group::RolledSteel)->setChecked(true);
    EXPECT_TRUE(dialog.gradeCombo(DescribeMaterialDialog::Group::RolledSteel)->isVisibleTo(&dialog));
    const MaterialChoice choice = dialog.choice(DescribeMaterialDialog::Group::RolledSteel);
    EXPECT_FALSE(choice.useProjectMaterial);
    EXPECT_EQ(choice.grade, "S235");  // the first of the list, not the material of the work
    EXPECT_EQ(dialog.choice(DescribeMaterialDialog::Group::Concrete).grade, "");
}

TEST(DescribeMaterialDialog, CombosOfTheCaptures) {
    QStringList steel, concrete;
    for (const auto& g : hotRolledSteelGrades()) steel << QString::fromStdString(g);
    for (const auto& g : concreteGrades()) concrete << QString::fromStdString(g);
    DescribeMaterialDialog dialog({{DescribeMaterialDialog::Group::RolledSteel, "S275 (UNE-EN 10025-2)", steel},
                                   {DescribeMaterialDialog::Group::Concrete, "C25/30", concrete}});
    EXPECT_EQ(dialog.gradeCombo(DescribeMaterialDialog::Group::RolledSteel)->count(), 18);
    EXPECT_EQ(dialog.gradeCombo(DescribeMaterialDialog::Group::RolledSteel)->itemText(17), "S460 HISTAR®");
    EXPECT_EQ(dialog.gradeCombo(DescribeMaterialDialog::Group::Concrete)->count(), 17);
    EXPECT_EQ(dialog.gradeCombo(DescribeMaterialDialog::Group::Concrete)->itemText(0), "C12/15");
    EXPECT_EQ(dialog.gradeCombo(DescribeMaterialDialog::Group::Concrete)->itemText(16), "Bc 60");
}

TEST(DescribeMaterialDialog, TimberTypeAndStrengthClass) {
    DescribeMaterialDialog dialog({{DescribeMaterialDialog::Group::Timber, "C24", {}, "Aserrada, procedente de coníferas o chopos"}});
    EXPECT_EQ(dialog.groupTitles(), (QStringList{"Madera"}));
    EXPECT_EQ(dialog.projectRadio(DescribeMaterialDialog::Group::Timber)->text(), "Material seleccionado en datos generales (C24)");
    QComboBox* type = dialog.timberTypeCombo();
    QComboBox* classes = dialog.gradeCombo(DescribeMaterialDialog::Group::Timber);
    ASSERT_NE(type, nullptr);
    EXPECT_FALSE(type->isVisibleTo(&dialog));
    dialog.ownRadio(DescribeMaterialDialog::Group::Timber)->setChecked(true);
    EXPECT_TRUE(type->isVisibleTo(&dialog));
    EXPECT_TRUE(classes->isVisibleTo(&dialog));
    EXPECT_EQ(type->count(), 4);
    EXPECT_EQ(type->itemText(0), "Aserrada, procedente de coníferas o chopos");
    EXPECT_EQ(type->itemText(3), "Laminada encolada, combinada");
    ASSERT_EQ(classes->count(), 12);
    EXPECT_EQ(classes->itemText(0), "C14");
    EXPECT_EQ(classes->itemText(11), "C50");
    EXPECT_EQ(classes->currentText(), "C24");  // the class of the work is preselected
    type->setCurrentIndex(1);  // the classes of the other types come from the REA enumeration of ec5.dll
    ASSERT_EQ(classes->count(), 8);
    EXPECT_TRUE(classes->isEnabled());
    EXPECT_EQ(classes->itemText(0), "D18");
    EXPECT_EQ(classes->itemText(7), "D70");
    EXPECT_EQ(dialog.choice(DescribeMaterialDialog::Group::Timber).timberType, "Aserrada, procedente de especies frondosas");
    type->setCurrentIndex(2);
    ASSERT_EQ(classes->count(), 8);
    EXPECT_EQ(classes->itemText(0), "GL20h");
    EXPECT_EQ(classes->itemText(7), "GL36h");
    type->setCurrentIndex(3);
    ASSERT_EQ(classes->count(), 8);
    EXPECT_EQ(classes->itemText(0), "GL20c");
    EXPECT_EQ(classes->itemText(7), "GL36c");
}

// Visual comparison aid: CC_SNAPSHOT_DIR=<dir> writes PNGs of the dialog (offscreen) to compare with orig_describir_*.png.
TEST(BarDescribeDialog, SnapshotsForVisualComparison) {
    const QByteArray dir = qgetenv("CC_SNAPSHOT_DIR");
    if (dir.isEmpty()) GTEST_SKIP() << "CC_SNAPSHOT_DIR not set";
    Sinks sinks;
    const QString base = QString::fromLocal8Bit(dir);
    const auto d = make();
    d->grab().save(base + "/clone_describir_laminado.png");
    clickIcon(*d, BarMaterialIcon::Generic);
    d->grab().save(base + "/clone_describir_generica.png");
    clickIcon(*d, BarMaterialIcon::Concrete);
    d->grab().save(base + "/clone_describir_hormigon.png");
    clickIcon(*d, BarMaterialIcon::ColdFormedSteel);
    d->grab().save(base + "/clone_describir_conformado.png");
    d->typeRadio(2)->click();
    d->grab().save(base + "/clone_describir_pilar.png");
    DescribeMaterialDialog material({{DescribeMaterialDialog::Group::RolledSteel, "S275 (UNE-EN 10025-2)", {"S235", "S275"}}});
    material.grab().save(base + "/clone_describir_material.png");
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc, argv);  // shared by every test of this executable
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
