// Tests of the GUI of combinations and user situations (T-P7-06), run on Qt's offscreen platform.
#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QToolButton>
#include <algorithm>
#include <QComboBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>

#include "ClickTooltip.h"
#include "LimitStatesDialog.h"
#include "RangedEdit.h"
#include "StandardsDialog.h"
#include "UserMessages.h"
#include "UserSituationsDialogs.h"

using namespace cc::app;
using namespace cc::core;

namespace {
void ensureApp() {
    static int argc = 1;
    static char name[] = "user_situations_dialog_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
}

/// Replaces the message windows: records the texts, answers "Sí" to questions.
struct MessageSpy {
    std::vector<QString> texts;
    std::vector<messages::Kind> kinds;
    bool answer = true;
    MessageSpy() {
        messages::handler() = [this](QWidget*, messages::Kind kind, const QString& text) {
            texts.push_back(text);
            kinds.push_back(kind);
            return answer;
        };
    }
    ~MessageSpy() { messages::handler() = nullptr; }
};

bool hasLabel(QWidget* parent, const QString& text) {
    for (QLabel* l : parent->findChildren<QLabel*>())
        if (l->text().contains(text)) return true;
    return false;
}

int countLabels(QWidget* parent, const QString& exact) {
    int n = 0;
    for (QLabel* l : parent->findChildren<QLabel*>())
        if (l->text() == exact) ++n;
    return n;
}

UserSituationSet sampleSet(const std::string& reference = "Conjunto") {
    UserSituationSet set;
    set.reference = reference;
    set.categories = {"A"};
    UserSituation s = UserSituation::withDefaults(1);
    s.name = "S1";
    set.situations.push_back(s);
    return set;
}
}  // namespace

TEST(SituationEditDialog, LayoutAndDefaultsAreTheMeasuredOnes) {
    ensureApp();
    SituationEditDialog d(UserSituation::withDefaults(1), 1, CoefficientMode::PartialAndCombination, {});
    EXPECT_EQ(d.windowTitle(), "Situación de proyecto definida por el usuario");
    EXPECT_EQ(d.size(), QSize(488, 451));  // measured, research 14.4
    EXPECT_FALSE(d.editor()->seismicRowVisible());
    EXPECT_TRUE(hasLabel(&d, "Nombre de la situación"));
    EXPECT_TRUE(hasLabel(&d, "Coeficientes parciales de seguridad"));
    EXPECT_TRUE(hasLabel(&d, "Coeficientes de combinación"));
    EXPECT_TRUE(hasLabel(&d, "Sobrecarga (Uso 1)"));
    EXPECT_TRUE(hasLabel(&d, "Empujes del terreno"));
    ASSERT_EQ(d.typeCombo()->count(), 4);
    EXPECT_EQ(d.typeCombo()->itemText(0), "Situación persistente o transitoria");
    EXPECT_EQ(d.typeCombo()->itemText(3), "Pretensado en vacío");
    // Defaults 1.000, favourable factor of the variable actions 0.000; "-" cells are not editors.
    EXPECT_EQ(d.cell(0, 0)->text(), "1.000");
    EXPECT_EQ(d.cell(0, 2), nullptr);  // permanent: no principal / accompanying
    EXPECT_EQ(d.cell(3, 0)->text(), "0.000");  // imposed
    EXPECT_EQ(d.cell(3, 2)->text(), "1.000");
    EXPECT_EQ(d.cell(7, 0)->text(), "1.000");  // earth thrust
    EXPECT_EQ(d.cell(7, 2), nullptr);
    EXPECT_EQ(d.cell(0, 0)->toolTip(), "Debe estar entre 0.000 y 10.000 .");
}

TEST(SituationEditDialog, TabOrderIsRowByRowSkippingDisabledCells) {
    ensureApp();
    SituationEditDialog d(UserSituation::withDefaults(1), 1, CoefficientMode::PartialAndCombination, {});
    const auto chain = d.tabChain();
    // type combo, then permanent (2), prestress (2), shrinkage (2), imposed (4), wind (4), snow (4), temperature (4), earth (2).
    ASSERT_EQ(chain.size(), 1u + 2 + 2 + 2 + 4 + 4 + 4 + 4 + 2);
    EXPECT_EQ(chain[0], d.typeCombo());
    EXPECT_EQ(chain[1], d.cell(0, 0));
    EXPECT_EQ(chain[2], d.cell(0, 1));
    EXPECT_EQ(chain[3], d.cell(1, 0));
    EXPECT_EQ(chain[7], d.cell(3, 0));
    EXPECT_EQ(chain[10], d.cell(3, 3));
    EXPECT_EQ(chain[11], d.cell(4, 0));  // wind starts after the imposed row
    // The Qt focus chain follows it: walking it from the name, the editable widgets appear in that order.
    std::vector<QWidget*> walked;
    for (QWidget* w = d.nameEdit()->nextInFocusChain(); w != d.nameEdit(); w = w->nextInFocusChain())
        if (std::find(chain.begin(), chain.end(), w) != chain.end()) walked.push_back(w);
    EXPECT_EQ(walked, chain);
}

TEST(SituationEditDialog, PartialModeHasNoCombinationColumns) {
    ensureApp();
    SituationEditDialog d(UserSituation::withDefaults(1), 1, CoefficientMode::PartialFactors, {});
    EXPECT_FALSE(hasLabel(&d, "Coeficientes de combinación"));
    EXPECT_NE(d.cell(0, 0), nullptr);
    EXPECT_EQ(d.cell(3, 2), nullptr);
    EXPECT_EQ(d.tabChain().size(), 1u + 8 * 2);
}

TEST(SituationEditDialog, ValidationOrderAndMessages) {
    ensureApp();
    MessageSpy spy;
    SituationEditDialog d(UserSituation::withDefaults(1), 1, CoefficientMode::PartialAndCombination, {"Otra"});
    EXPECT_EQ(d.check(), SituationError::EmptyName);
    d.nameEdit()->setText("Otra");
    EXPECT_EQ(d.check(), SituationError::DuplicateName);
    d.nameEdit()->setText("Nombre1");
    EXPECT_EQ(d.check(), SituationError::None);
    // Both mistakes at once: the favourable / unfavourable message comes first.
    d.cell(0, 0)->setValue(2.0);
    d.cell(0, 1)->setValue(1.0);
    d.cell(4, 2)->setValue(0.5);  // wind principal
    d.cell(4, 3)->setValue(0.9);  // wind accompanying
    spy.texts.clear();
    EXPECT_EQ(d.check(), SituationError::FavourableAboveUnfavourable);
    ASSERT_EQ(spy.texts.size(), 1u);
    EXPECT_EQ(spy.texts[0], "No puede introducir un coeficiente favorable mayor que el desfavorable.");
    d.cell(0, 0)->setValue(1.0);
    spy.texts.clear();
    EXPECT_EQ(d.check(), SituationError::PrincipalBelowAccompanying);
    EXPECT_EQ(spy.texts[0], "No puede introducir un coeficiente principal menor que el de acompañamiento.");
    d.cell(4, 2)->setValue(1.0);
    EXPECT_EQ(d.check(), SituationError::None);
    const UserSituation s = d.situation();
    EXPECT_EQ(s.name, "Nombre1");
    EXPECT_DOUBLE_EQ(s.wind.accompanying, 0.9);
}

TEST(SituationEditDialog, CellsRefuseOutOfRangeValuesAndRecover) {
    ensureApp();
    RangedEdit::setWarningHandler([](QWidget*, const QString&) {});
    SituationEditDialog d(UserSituation::withDefaults(0), 0, CoefficientMode::PartialFactors, {});
    d.cell(0, 1)->setText("10.5");
    EXPECT_FALSE(d.cell(0, 1)->commit());
    EXPECT_EQ(d.cell(0, 1)->text(), "1.000");
    d.cell(0, 1)->setText("10");
    EXPECT_TRUE(d.cell(0, 1)->commit());
    EXPECT_EQ(d.cell(0, 1)->text(), "10.000");
    RangedEdit::setWarningHandler(nullptr);
}

TEST(SituationEditDialog, KeepsTheSituationItWasGiven) {
    ensureApp();
    UserSituation s = UserSituation::withDefaults(2);
    s.name = "Sismo";
    s.type = SituationType::Seismic;
    s.imposed[1].unfavourable = 4.0;
    SituationEditDialog d(s, 2, CoefficientMode::PartialAndCombination, {});
    EXPECT_EQ(d.typeCombo()->currentIndex(), 2);
    EXPECT_EQ(d.situation(), s);
}

TEST(CoefficientModeDialog, DefaultIsPartialFactors) {
    ensureApp();
    CoefficientModeDialog d;
    EXPECT_EQ(d.mode(), CoefficientMode::PartialFactors);
    EXPECT_EQ(d.windowTitle(), "Coeficientes de combinación");
    int radios = 0;
    for (QRadioButton* r : d.findChildren<QRadioButton*>()) {
        ++radios;
        if (r->text() == "Introducir coeficientes de mayoración y combinación") r->setChecked(true);
    }
    EXPECT_EQ(radios, 2);
    EXPECT_EQ(d.mode(), CoefficientMode::PartialAndCombination);
}

TEST(SituationCategoriesDialog, AddCopyDeleteTrackTheOrigin) {
    ensureApp();
    SituationCategoriesDialog d({"A", "B"});
    EXPECT_EQ(d.table()->horizontalHeaderItem(0)->text(), "Categoría de uso");
    // With categories already defined the radios appear and the existing ones are the default (research 14.4).
    ASSERT_NE(d.alreadyDefinedRadio(), nullptr);
    EXPECT_TRUE(d.alreadyDefinedRadio()->isChecked());
    EXPECT_FALSE(d.table()->isEnabled());
    EXPECT_EQ(d.categories(), (std::vector<std::string>{"A", "B"}));
    d.defineNewRadio()->setChecked(true);
    EXPECT_TRUE(d.table()->isEnabled());
    EXPECT_EQ(d.origin(), (std::vector<int>{0, 1}));
    d.table()->selectRow(1);
    d.copyRow();
    EXPECT_EQ(d.categories(), (std::vector<std::string>{"A", "B", "B"}));
    EXPECT_EQ(d.origin(), (std::vector<int>{0, 1, 1}));
    d.addRow();
    EXPECT_EQ(d.origin().back(), -1);
    d.table()->selectRow(0);
    d.deleteRow();
    EXPECT_EQ(d.categories().size(), 3u);
    EXPECT_EQ(d.origin(), (std::vector<int>{1, 1, -1}));
}

TEST(SituationSetDialog, ChecksReferenceAndSituationsInTheMeasuredOrder) {
    ensureApp();
    MessageSpy spy;
    SituationSetDialog d(UserSituationSet{}, {"Existente"}, true);
    EXPECT_EQ(d.windowTitle(), "Crear - [Situaciones]");
    EXPECT_EQ(d.size(), QSize(406, 355));
    EXPECT_FALSE(d.masterDetail());
    EXPECT_EQ(d.check(), SituationError::EmptyReference);
    EXPECT_EQ(spy.texts.back(), "Debe introducir una referencia");
    EXPECT_EQ(spy.kinds.back(), messages::Kind::Error);  // #94341 is an Error, not an Aviso
    // The repeated reference is NOT detected when creating the set (only when accepting the limit state).
    d.referenceEdit()->setText("Existente");
    spy.texts.clear();
    EXPECT_EQ(d.check(), SituationError::NoSituations);
    EXPECT_EQ(spy.texts.at(0), "La lista no puede quedar vacía.");
    EXPECT_EQ(spy.kinds.back(), messages::Kind::Warning);
    // A set whose only situation is not persistent: persistent first, then the seismic one when the work has seismic cases.
    UserSituationSet accidental = sampleSet();
    accidental.situations[0].type = SituationType::Accidental;
    SituationSetDialog noPersistent(accidental, {}, false);
    EXPECT_EQ(noPersistent.check(), SituationError::NoPersistentSituation);
    EXPECT_EQ(spy.texts.back(), "Se debe definir, al menos, una situación persistente o transitoria.");
    SituationSetDialog edit(sampleSet(), {}, false);
    EXPECT_EQ(edit.windowTitle(), "Editar - [Situaciones]");
    EXPECT_EQ(edit.check(), SituationError::None);
    EXPECT_EQ(edit.list()->count(), 1);
    edit.setProjectHasSeismic(true);
    EXPECT_EQ(edit.check(), SituationError::NoSeismicSituation);
    EXPECT_EQ(spy.texts.back(), "Se debe definir, al menos, una situación con sismo.");
}

TEST(SituationSetDialog, MasterDetailHasTheMeasuredSizesAndEditsTheSelectedSituation) {
    ensureApp();
    SituationSetDialog d(sampleSet(), {}, false);
    EXPECT_TRUE(d.masterDetail());
    EXPECT_EQ(d.size(), QSize(566, 475));
    EXPECT_EQ(d.list()->currentRow(), 0);
    UserSituationSet service = sampleSet();
    service.service = true;
    service.situations[0].type = SituationType::Service;
    SituationSetDialog els(service, {}, false);
    EXPECT_EQ(els.size(), QSize(406, 380));
    EXPECT_FALSE(els.editor()->typeCombo()->isVisibleTo(&els));  // E.L.S.: no type combo
    // Two situations: editing one, selecting the other and coming back keeps what was typed.
    d.copySituation();
    ASSERT_EQ(d.set().situations.size(), 2u);
    EXPECT_EQ(d.list()->currentRow(), 1);
    d.editor()->cell(3, 1)->setValue(7.0);  // imposed, unfavourable of "Copia de S1"
    d.list()->setCurrentRow(0);
    EXPECT_DOUBLE_EQ(d.editor()->cell(3, 1)->value(), 1.0);
    d.list()->setCurrentRow(1);
    EXPECT_DOUBLE_EQ(d.editor()->cell(3, 1)->value(), 7.0);
    EXPECT_DOUBLE_EQ(d.set().situations[1].imposed[0].unfavourable, 7.0);
    // Double click renames in the list.
    d.list()->item(1)->setText("Renombrada");
    EXPECT_EQ(d.set().situations[1].name, "Renombrada");
    EXPECT_TRUE(d.list()->item(1)->flags() & Qt::ItemIsEditable);
}

TEST(SituationSetDialog, BarTooltipsAreTheMeasuredOnesAndAppearAfterAClick) {
    ensureApp();
    SituationSetDialog d(UserSituationSet{}, {}, true);
    EXPECT_EQ(d.addButton()->toolTip(), "Nuevo");
    EXPECT_EQ(d.copyButton()->toolTip(), "Copiar");
    EXPECT_EQ(d.deleteButton()->toolTip(), "Borrar");
    // The hover tooltip is eaten by a ClickTooltip filter child of each button: tooltips only show after a click.
    EXPECT_FALSE(d.copyButton()->findChildren<QObject*>().isEmpty());
}

TEST(SituationSetDialog, CopyAndDeleteAskAndRequireASelection) {
    ensureApp();
    MessageSpy spy;
    SituationSetDialog empty(UserSituationSet{}, {}, true);
    empty.deleteSituation();
    EXPECT_EQ(spy.texts.back(), "Debe seleccionar una situación de proyecto.");
    SituationSetDialog d(sampleSet(), {}, false);
    d.copySituation();
    ASSERT_EQ(d.set().situations.size(), 2u);
    EXPECT_EQ(d.set().situations[1].name, "Copia de S1");
    spy.answer = false;
    d.list()->setCurrentRow(1);
    d.deleteSituation();
    EXPECT_EQ(spy.texts.back(), "¿Desea borrar la situación de proyecto seleccionada?");
    EXPECT_EQ(d.set().situations.size(), 2u);  // answered "No"
    spy.answer = true;
    d.deleteSituation();
    EXPECT_EQ(d.set().situations.size(), 1u);
}

TEST(SituationEditor, SeismicRowHasTheFractionCellAndItsRange) {
    ensureApp();
    RangedEdit::setWarningHandler([](QWidget*, const QString&) {});
    UserSituation s = UserSituation::withDefaults(1);
    s.type = SituationType::Seismic;
    SituationEditDialog d(s, 1, CoefficientMode::PartialAndCombination, {});
    EXPECT_TRUE(d.editor()->seismicRowVisible());
    EXPECT_TRUE(hasLabel(&d, "(*) Porcentaje de las solicitaciones producidas por el sismo en la dirección ortogonal"));
    RangedEdit* favourable = d.cell(8, 0);  // the "Sismo" row follows Empujes del terreno
    RangedEdit* fraction = d.cell(8, 3);
    ASSERT_NE(favourable, nullptr);
    ASSERT_NE(fraction, nullptr);
    EXPECT_EQ(d.cell(8, 2), nullptr);  // principal 1.000 is fixed, no cell
    EXPECT_EQ(favourable->text(), "-1.000");
    EXPECT_EQ(d.cell(8, 1)->text(), "1.000");
    EXPECT_EQ(fraction->text(), "0.000");
    fraction->setText("0.3");  // fraction 0..1: 0.3 accepted and shown 0.300; 30 and 2 refused
    EXPECT_TRUE(fraction->commit());
    EXPECT_EQ(fraction->text(), "0.300");
    fraction->setText("30");
    EXPECT_FALSE(fraction->commit());
    fraction->setText("2");
    EXPECT_FALSE(fraction->commit());
    EXPECT_EQ(fraction->text(), "0.300");
    EXPECT_DOUBLE_EQ(d.situation().seismic.accompanying, 0.3);
    // The seismic cells join the tab chain only for the seismic type.
    const std::size_t seismicChain = d.tabChain().size();
    d.typeCombo()->setCurrentIndex(0);
    EXPECT_FALSE(d.editor()->seismicRowVisible());
    EXPECT_EQ(d.tabChain().size(), seismicChain - 3);
    RangedEdit::setWarningHandler(nullptr);
}

TEST(SituationEditDialog, ServiceSituationHasNoTypeComboAndTheMeasuredSize) {
    ensureApp();
    UserSituation s = UserSituation::withDefaults(1);
    s.type = SituationType::Service;
    SituationEditDialog d(s, 1, CoefficientMode::PartialAndCombination, {});  // the mode is ignored: no combination columns
    EXPECT_EQ(d.size(), QSize(475, 357));
    EXPECT_FALSE(d.typeCombo()->isVisibleTo(&d));
    EXPECT_FALSE(hasLabel(&d, "Coeficientes de combinación"));
    EXPECT_EQ(d.cell(3, 2), nullptr);
    EXPECT_EQ(d.situation().type, SituationType::Service);
}

// ----------------------------------------------------------------------------- Estados límite

TEST(LimitStatesDialogVariants, CteButtonSetShowsRadiosAndStandaloneGround) {
    ensureApp();
    LimitStatesDialog d{StandardsSelection::cteSet(), LimitStatesSettings{}};
    EXPECT_EQ(d.size().width(), 374);  // wider: the long foundation heading
    EXPECT_TRUE(hasLabel(&d, "Hormigón: Código Estructural"));
    EXPECT_TRUE(hasLabel(&d, "Hormigón en cimentaciones: Código Estructural / CTE DB-SE C"));
    EXPECT_TRUE(hasLabel(&d, "Acero laminado: CTE DB SE-A"));
    ASSERT_EQ(d.codeRadios().size(), 3u);
    EXPECT_EQ(d.codeRadios()[2]->text(), "Hormigón: EC, Acero: CTE");
    EXPECT_TRUE(d.codeRadios()[2]->isChecked());  // the third radio is marked by default (measured)
    EXPECT_EQ(d.snowCombos().size(), 3u);         // concrete (CE), CTE steel + timber, aluminium
    EXPECT_TRUE(hasLabel(&d, "Cota de nieve"));
    EXPECT_TRUE(hasLabel(&d, "Acciones características"));
}

TEST(LimitStatesDialogVariants, EcRadioRetitlesTheSnowLabel) {
    ensureApp();
    LimitStatesDialog d{StandardsSelection::structuralCodeSet(), LimitStatesSettings{}};
    ASSERT_GE(d.codeRadios().size(), 3u);
    EXPECT_EQ(countLabels(&d, "Cota de nieve"), 2);  // structural-code block (radio CTE / mixed) and the CTE one
    d.codeRadios()[1]->setChecked(true);  // EC
    EXPECT_EQ(countLabels(&d, "Cota de nieve"), 1);
    EXPECT_EQ(countLabels(&d, "Nieve"), 2);  // that block now says "Nieve" like the Eurocode block (cold-formed, aluminium)
    EXPECT_EQ(d.value().hotRolledCode, CombinationCode::EC);
    EXPECT_TRUE(d.value().codeChosen);
    d.codeRadios()[0]->setChecked(true);  // CTE
    EXPECT_EQ(countLabels(&d, "Cota de nieve"), 2);
    EXPECT_EQ(d.value().hotRolledCode, CombinationCode::CTE);
}

TEST(LimitStatesDialogVariants, AllEurocodesHaveOneComboWithThreeZones) {
    ensureApp();
    StandardsSelection s;
    s.coldFormedSteel = "Eurocódigos 3 y 4 (UE)";
    s.hotRolledSteel = "Eurocódigos 3 y 4 (UE)";
    s.timber = "Eurocódigo 5 (UE)";
    LimitStatesSettings settings;
    settings.groundSnow = "Altitud superior a 1000 m";  // normalised to the Eurocode list
    LimitStatesDialog d{s, settings};
    ASSERT_EQ(d.snowCombos().size(), 1u);
    EXPECT_EQ(d.snowCombos()[0]->count(), 3);
    EXPECT_EQ(d.snowCombos()[0]->currentText(), "Resto de los Estados miembro del CEN, H > 1000 m");
    EXPECT_EQ(d.size().width(), 426);
    EXPECT_TRUE(d.codeRadios().empty());
    d.snowCombos()[0]->setCurrentIndex(0);
    EXPECT_EQ(d.value().groundSnow, "Finlandia, Islandia, Noruega, Suecia");
    EXPECT_TRUE(d.value().isValid());
}

TEST(LimitStatesDialogVariants, DefaultStillMeasuredSizeAndCoteComboFeedsTimber) {
    ensureApp();
    LimitStatesDialog d{StandardsSelection{}, LimitStatesSettings{}};
    EXPECT_EQ(d.size(), QSize(358, 438));
    d.snowCombos()[1]->setCurrentText("Altitud superior a 1000 m");
    EXPECT_EQ(d.value().coldFormedSnow, "Altitud superior a 1000 m");
    EXPECT_EQ(d.value().timberSnow, "Altitud superior a 1000 m");
}

TEST(LimitStatesVerifyDialogGui, ButtonsAreEnabledAndShowTheCombination) {
    ensureApp();
    LimitStatesVerifyDialog d{LimitStatesSettings{}, StandardsSelection{}};
    ASSERT_EQ(d.kindButtons().size(), 8u);
    for (QPushButton* b : d.kindButtons()) EXPECT_TRUE(b->isEnabled());
    EXPECT_TRUE(hasLabel(&d, "EC - Altitud inferior o igual a 1000 m"));
    EXPECT_TRUE(hasLabel(&d, "CTE - Altitud inferior o igual a 1000 m"));
}

TEST(LimitStateKindDialogGui, DefaultRadioAndUserAssignment) {
    ensureApp();
    MessageSpy spy;
    LimitStatesSettings settings;
    settings.userSets.push_back(sampleSet("Mi norma"));
    LimitStateKindDialog d(LimitStateKind::Aluminium, StandardsSelection{}, settings);
    EXPECT_EQ(d.windowTitle(), "Aluminio");
    EXPECT_TRUE(d.generalRadio()->isChecked());
    EXPECT_EQ(d.setCombo()->currentIndex(), -1);  // the combo starts empty (measured)
    EXPECT_FALSE(d.setCombo()->isEnabled());
    EXPECT_TRUE(hasLabel(&d, "Nieve: Altitud inferior o igual a 1000 m"));
    // User radio without a selected set: refused.
    d.userRadio()->setChecked(true);
    EXPECT_TRUE(d.setCombo()->isEnabled());
    d.accept();
    EXPECT_EQ(d.result(), QDialog::Rejected);
    EXPECT_EQ(spy.texts.back(), "Debe introducir, al menos, una situación de proyecto.");  // #156481
    EXPECT_EQ(spy.kinds.back(), messages::Kind::Error);  // red icon, not an Aviso
    d.setCombo()->setCurrentIndex(0);
    d.accept();
    EXPECT_EQ(d.result(), QDialog::Accepted);
    EXPECT_EQ(d.settings().userSetFor(LimitStateKind::Aluminium), "Mi norma");
}

TEST(LimitStateKindDialogGui, UserPencilWithoutASetAsksForOne) {
    ensureApp();
    MessageSpy spy;
    LimitStateKindDialog d(LimitStateKind::Aluminium, StandardsSelection{}, LimitStatesSettings{});
    EXPECT_EQ(d.size(), QSize(383, 230));
    d.viewSet();
    EXPECT_EQ(spy.texts.back(), "Debe introducir, al menos, una situación de proyecto.");  // measured in the original
}

TEST(LimitStateKindDialogGui, CopyFromNormAndDeleteGuards) {
    ensureApp();
    MessageSpy spy;
    LimitStateKindDialog d(LimitStateKind::Concrete, StandardsSelection{}, LimitStatesSettings{});
    d.userRadio()->setChecked(true);
    d.fromNorm();  // arrow: create from the situations of the code
    ASSERT_EQ(d.settings().userSets.size(), 1u);
    EXPECT_EQ(d.settings().userSets[0].situations.size(), 3u);
    EXPECT_EQ(validateSet(d.settings().userSets[0]), SituationError::None);
    d.copySet();
    EXPECT_EQ(d.settings().userSets.size(), 2u);
    EXPECT_EQ(d.settings().userSets[1].reference, "Copia de Norma");
    // A set used by another limit state cannot be deleted (#156484).
    LimitStatesSettings used;
    used.userSets.push_back(sampleSet("Usada"));
    used.setUserSetFor(LimitStateKind::Timber, "Usada");
    LimitStateKindDialog other(LimitStateKind::Concrete, StandardsSelection{}, used);
    other.userRadio()->setChecked(true);
    other.setCombo()->setCurrentIndex(0);
    other.deleteSet();
    EXPECT_EQ(spy.texts.back(), "No es posible borrar esta situación de proyecto, ya que está siendo usada para la comprobación de otros estados límite.");
    EXPECT_EQ(other.settings().userSets.size(), 1u);
    // The same set used only by this limit state can be deleted after the question.
    LimitStateKindDialog own(LimitStateKind::Timber, StandardsSelection{}, used);
    own.setCombo()->setCurrentIndex(0);
    own.deleteSet();
    EXPECT_TRUE(own.settings().userSets.empty());
}

// ----------------------------------------------------------------------------- Selección de normas

TEST(LimitStateKindDialogGui, ConcreteHasSeparateEluAndElsRows) {
    ensureApp();
    MessageSpy spy;
    LimitStatesSettings settings;
    settings.userSets.push_back(sampleSet("CA"));
    UserSituationSet service = sampleSet("CA");  // an E.L.U. and an E.L.S. set may share a reference
    service.service = true;
    service.situations[0].type = SituationType::Service;
    settings.userSets.push_back(service);
    settings.userSets.push_back(sampleSet("Otra"));
    LimitStateKindDialog d(LimitStateKind::Concrete, StandardsSelection{}, settings);
    ASSERT_TRUE(d.hasServiceRow());
    EXPECT_TRUE(hasLabel(&d, "E.L.U. de rotura"));
    EXPECT_TRUE(hasLabel(&d, "E.L.S. de fisuración"));
    EXPECT_EQ(d.setCombo(false)->count(), 2);  // CA, Otra
    EXPECT_EQ(d.setCombo(true)->count(), 1);   // only the E.L.S. library
    // No bar tooltip appears by hovering, but they carry the measured texts.
    QStringList tips;
    for (QToolButton* b : d.findChildren<QToolButton*>()) tips << b->toolTip();
    for (const char* t : {"Nuevo", "Borrar", "Copiar", "Editar", "Gestión de biblioteca", "Crear a partir de las situaciones de la norma"})
        EXPECT_TRUE(tips.contains(t)) << t;
    EXPECT_EQ(LimitStateKindDialog::libraryMenuEntries(), (QStringList{"Editar biblioteca", "Importar de biblioteca", "Exportar a biblioteca"}));
    // Accepting needs both combos; the assignments are separate.
    d.userRadio()->setChecked(true);
    d.setCombo(false)->setCurrentIndex(0);
    d.accept();
    EXPECT_EQ(d.result(), QDialog::Rejected);
    EXPECT_EQ(spy.kinds.back(), messages::Kind::Error);
    d.setCombo(true)->setCurrentIndex(0);
    d.accept();
    EXPECT_EQ(d.result(), QDialog::Accepted);
    EXPECT_EQ(d.settings().userSetFor(LimitStateKind::Concrete), "CA");
    EXPECT_EQ(d.settings().userServiceSetFor(LimitStateKind::Concrete), "CA");
}

TEST(LimitStateKindDialogGui, RepeatedReferenceIsOnlyDetectedWhenAcceptingTheLimitState) {
    ensureApp();
    MessageSpy spy;
    LimitStatesSettings settings;
    settings.userSets.push_back(sampleSet("CA"));
    settings.userSets.push_back(sampleSet("CA"));  // "Crear" does not detect it
    LimitStateKindDialog d(LimitStateKind::Aluminium, StandardsSelection{}, settings);
    EXPECT_EQ(d.setCombo()->count(), 2);
    d.userRadio()->setChecked(true);
    d.setCombo()->setCurrentIndex(0);
    d.accept();
    EXPECT_EQ(d.result(), QDialog::Rejected);
    EXPECT_EQ(spy.texts.back(), "La referencia 'CA' aparece repetida en la caja de selección.\nDebe renombrarla o borrarla.");
    EXPECT_EQ(spy.kinds.back(), messages::Kind::Error);
}

TEST(LimitStateKindDialogGui, DeleteAndEditWithAnEmptyComboDoNothing) {
    ensureApp();
    MessageSpy spy;
    LimitStateKindDialog d(LimitStateKind::Aluminium, StandardsSelection{}, LimitStatesSettings{});
    d.userRadio()->setChecked(true);
    d.deleteSet();
    d.editSet();
    d.copySet();
    EXPECT_TRUE(spy.texts.empty());
}

TEST(LimitStatesDialogVariants, UserSetsReplaceTheSnowComboWithTheMeasuredLines) {
    ensureApp();
    LimitStatesSettings settings;
    settings.userSets.push_back(sampleSet("CA"));
    UserSituationSet service = sampleSet("CF");
    service.service = true;
    service.situations[0].type = SituationType::Service;
    settings.userSets.push_back(service);
    settings.setUserSetFor(LimitStateKind::Concrete, "CA");
    settings.setUserServiceSetFor(LimitStateKind::Concrete, "CF");
    LimitStatesDialog d{StandardsSelection{}, settings};
    EXPECT_TRUE(hasLabel(&d, "E.L.U. de rotura: CA"));
    EXPECT_TRUE(hasLabel(&d, "E.L.S. de fisuración: CF"));
    // The default dialog has no such lines.
    LimitStatesDialog plain{StandardsSelection{}, LimitStatesSettings{}};
    EXPECT_FALSE(hasLabel(&plain, "E.L.U. de rotura: "));
}

TEST(LimitStatesDialogVariants, AllEurocodeDefaultSnowIsRestoDelCenLessOrEqual1000) {
    ensureApp();
    StandardsSelection s;
    s.coldFormedSteel = "Eurocódigos 3 y 4 (UE)";
    s.hotRolledSteel = "Eurocódigos 3 y 4 (UE)";
    s.timber = "Eurocódigo 5 (UE)";
    LimitStatesDialog d{s, LimitStatesSettings{}};
    ASSERT_EQ(d.snowCombos().size(), 1u);
    EXPECT_EQ(d.snowCombos()[0]->currentText(), "Resto de los Estados miembro del CEN, H <= 1000 m");  // measured default, research 14.5
}

TEST(StandardsDialogButtons, CteAndCeButtonsAssignTheMeasuredSets) {
    ensureApp();
    StandardsDialog d{StandardsSelection{}};
    EXPECT_EQ(d.foundationCombo()->isVisibleTo(&d), false);  // Eurocode concrete: no "Cimentación"
    d.applySet(StandardsSelection::cteSet());
    EXPECT_EQ(d.selection().concrete, "Código Estructural (España)");
    EXPECT_EQ(d.selection().hotRolledSteel, "CTE DB SE-A (España)");
    EXPECT_EQ(d.selection().foundation, "Criterio del CTE DB-SE-C");
    EXPECT_TRUE(d.foundationCombo()->isVisibleTo(&d));
    d.applySet(StandardsSelection::structuralCodeSet());
    EXPECT_EQ(d.selection().coldFormedSteel, "Eurocódigos 3 y 4 (España)");
    EXPECT_EQ(d.selection().foundation, "Criterio del Código Estructural");
    EXPECT_TRUE(d.selection().isValid());
    int shortcuts = 0;
    for (QPushButton* b : d.findChildren<QPushButton*>())
        if (b->text() == "C.E." || b->text() == "CTE") {
            ++shortcuts;
            EXPECT_TRUE(b->isEnabled());
        }
    EXPECT_EQ(shortcuts, 2);
}

TEST(StandardsDialogButtons, AcceptWarnsOnlyWhenAMaterialStandardChanged) {
    ensureApp();
    MessageSpy spy;
    StandardsDialog same{StandardsSelection{}};
    same.accept();
    EXPECT_TRUE(spy.texts.empty());
    StandardsDialog changed{StandardsSelection{}};
    changed.applySet(StandardsSelection::cteSet());
    changed.accept();
    ASSERT_EQ(spy.texts.size(), 1u);
    EXPECT_EQ(spy.texts[0], StandardsDialog::materialsWarning());
    EXPECT_TRUE(spy.texts[0].startsWith("Al cambiar la norma correspondiente a un tipo de material"));
}

TEST(StandardsDialogButtons, FoundationComboHasTwoEntriesAndOnlyShowsWithTheStructuralCode) {
    ensureApp();
    StandardsDialog d{StandardsSelection::cteSet()};
    ASSERT_TRUE(d.foundationCombo()->isVisibleTo(&d));
    EXPECT_EQ(d.foundationCombo()->count(), 2);
    EXPECT_EQ(d.foundationCombo()->itemText(0), "Criterio del CTE DB-SE-C");
    EXPECT_EQ(d.foundationCombo()->itemText(1), "Criterio del Código Estructural");
    d.applySet(StandardsSelection{});  // Eurocode 2: the combo disappears
    EXPECT_FALSE(d.foundationCombo()->isVisibleTo(&d));
}

TEST(UseCategorySelectionDialogGui, EurocodesAllUntickedAndCteHasG2) {
    ensureApp();
    using Scheme = UseCategorySelectionDialog::Scheme;
    bool ok = false;
    EXPECT_EQ(UseCategorySelectionDialog::schemeFor(StandardsSelection{StandardsSelection{}}, &ok), Scheme::Cte);  // default has CTE steel: mixed
    EXPECT_FALSE(ok);
    StandardsSelection ec;
    ec.coldFormedSteel = ec.hotRolledSteel = "Eurocódigos 3 y 4 (UE)";
    ec.timber = "Eurocódigo 5 (UE)";
    EXPECT_EQ(UseCategorySelectionDialog::schemeFor(ec, &ok), Scheme::Eurocode);
    EXPECT_TRUE(ok);
    UseCategorySelectionDialog eurocode(Scheme::Eurocode, std::vector<bool>(8, false));
    EXPECT_EQ(eurocode.windowTitle(), "Categorías de uso");
    ASSERT_EQ(eurocode.boxes().size(), 8u);
    EXPECT_EQ(eurocode.boxes()[0]->text(), "A. Domésticos y residenciales");
    EXPECT_EQ(eurocode.boxes()[7]->text(), "H. Cubiertas");
    EXPECT_TRUE(eurocode.checkedReferences().empty());  // none may be ticked
    EXPECT_EQ(UseCategorySelectionDialog::schemeFor(StandardsSelection::cteSet(), &ok), Scheme::Cte);
    EXPECT_TRUE(ok);
    std::vector<bool> initial(7, false);
    initial.back() = true;
    UseCategorySelectionDialog cte(Scheme::Cte, initial);
    ASSERT_EQ(cte.boxes().size(), 7u);
    EXPECT_EQ(cte.boxes()[5]->text().left(2), "G1");
    EXPECT_EQ(cte.checkedReferences(), (std::vector<std::string>{"G2"}));
    cte.boxes()[0]->setChecked(true);
    EXPECT_EQ(cte.checkedReferences(), (std::vector<std::string>{"A", "G2"}));
    EXPECT_NE(cte.cancelButton(), nullptr);
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
