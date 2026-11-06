// Propiedades > Nudos (T-P6-01, GUI part): "Vinculación exterior" / "Vinculación interior" dialogs, pictograms, 3D symbols and the tool.
// Expected values come from docs/research/ui/vinculaciones.md section 5 (measured in the original). Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QGridLayout>
#include <QImage>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QTableWidget>
#include <QToolButton>
#include <cmath>
#include <memory>

#include "RangedEdit.h"
#include "SupportDialogs.h"
#include "SupportPictograms.h"
#include "SupportTool.h"
#include "Viewport3D.h"
#include "core/node_support_edit.h"

using namespace cc::core;
using cc::app::ExternalSupportDialog;
using cc::app::InternalLinkDialog;
using cc::app::RangedEdit;
using cc::app::SupportTool;
using cc::app::Viewport3D;

namespace {

QApplication* app() {
    static int argc = 1;
    static char name[] = "support_dialogs_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
    return &instance;
}

class SupportDialogsTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();
        RangedEdit::setWarningHandler([this](QWidget*, const QString& text) { warnings << text; });
        ExternalSupportDialog::setQuestionHandler([this](QWidget*, const QString& text) {
            questions << text;
            return answer;
        });
    }
    void TearDown() override {
        RangedEdit::setWarningHandler({});
        ExternalSupportDialog::setQuestionHandler({});
    }
    /// Types text into a ranged edit and leaves it (what the user does).
    static bool type(RangedEdit* edit, const QString& text) {
        edit->setText(text);
        return edit->commit();
    }
    QStringList warnings, questions;
    bool answer = true;
};

using Dofs = std::array<bool, 6>;

Dofs visibleDofs(const ExternalSupportDialog& d) {
    Dofs v{};
    for (int i = 0; i < 6; ++i) v[static_cast<std::size_t>(i)] = d.dofVisible(i);
    return v;
}

}  // namespace

// ----- Vinculación exterior ---------------------------------------------------------------------------------------------------

TEST_F(SupportDialogsTest, ExternalDialogHasTheMeasuredSizeTitleAndTwentyButtonsInTwoRows) {
    ExternalSupportDialog d;
    EXPECT_EQ(d.size(), QSize(591, 414));
    EXPECT_EQ(d.windowTitle(), QStringLiteral("Vinculación exterior"));
    EXPECT_EQ(d.acceptButton()->text(), QStringLiteral("Aceptar"));
    EXPECT_EQ(d.cancelButton()->text(), QStringLiteral("Cancelar"));
    // Grid order (orig_vinc_ext_dialogo_inicial.png): row 1 = 2, 3, 4..11; row 2 = 0, 1, 12..19.
    const int expected[2][10] = {{2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, {0, 1, 12, 13, 14, 15, 16, 17, 18, 19}};
    for (int v = 0; v < kExternalSupportTypeCount; ++v) {
        QToolButton* button = d.typeButtons()[static_cast<std::size_t>(v)];
        ASSERT_NE(button, nullptr) << v;
        auto* grid = qobject_cast<QGridLayout*>(button->parentWidget()->layout());
        ASSERT_NE(grid, nullptr);
        int row, col, rs, cs;
        grid->getItemPosition(grid->indexOf(button), &row, &col, &rs, &cs);
        EXPECT_EQ(expected[row][col], v);
        EXPECT_FALSE(button->icon().isNull());
        EXPECT_TRUE(button->isCheckable());
    }
}

TEST_F(SupportDialogsTest, ExternalTooltipsAreTheLiteralTextsOfTheOriginal) {
    using T = ExternalSupportType;
    EXPECT_EQ(ExternalSupportDialog::tooltipTitle(T::Free), "Libre");
    EXPECT_EQ(ExternalSupportDialog::tooltipText(T::Free), "Nudo sin vinculación exterior");
    EXPECT_EQ(ExternalSupportDialog::tooltipTitle(T::Pinned), "Articulación");
    EXPECT_EQ(ExternalSupportDialog::tooltipText(T::Pinned), "Apoyo fijo. Impide el desplazamiento en cualquier dirección.");
    EXPECT_EQ(ExternalSupportDialog::tooltipTitle(T::Generic), "Genérica");
    EXPECT_EQ(ExternalSupportDialog::tooltipText(T::Generic), "Nudo con vinculaciones definidas por el usuario.");
    EXPECT_EQ(ExternalSupportDialog::tooltipTitle(T::Fixed), "Empotramiento");
    EXPECT_EQ(ExternalSupportDialog::tooltipText(T::Fixed), "Empotramiento. Impide el desplazamiento y el giro en cualquier dirección.");
    EXPECT_EQ(ExternalSupportDialog::tooltipTitle(T::LineYFixed), "Desplazamiento libre sobre una recta en la dirección Y");
    EXPECT_EQ(ExternalSupportDialog::tooltipText(T::LineYFixed), "Empotramiento con desplazamiento libre en el eje Y (impedido en el plano XZ global).");
    EXPECT_EQ(ExternalSupportDialog::tooltipText(T::LineZFixed), "Empotramiento con desplazamiento libre en el eje Z (impedido en el plano XY global).");
    EXPECT_EQ(ExternalSupportDialog::tooltipTitle(T::LineUserFixed), "Desplazamiento libre sobre una recta cualquiera");
    EXPECT_EQ(ExternalSupportDialog::tooltipTitle(T::PlaneXZFixed), "Desplazamiento libre sobre un plano paralelo a los ejes XZ");
    EXPECT_EQ(ExternalSupportDialog::tooltipText(T::PlaneXZFixed), "Empotramiento con desplazamiento libre en el plano XZ (impedido en la dirección Y global).");
    EXPECT_EQ(ExternalSupportDialog::tooltipText(T::PlaneYZFixed), "Empotramiento con desplazamiento libre en el plano YZ (impedido en la dirección X global).");
    EXPECT_EQ(ExternalSupportDialog::tooltipTitle(T::PlaneUserFixed), "Desplazamiento libre sobre un plano cualquiera");
    EXPECT_EQ(ExternalSupportDialog::tooltipTitle(T::LineXPinned), "Desplazamiento libre sobre una recta en la dirección X con giros sin coaccionar");
    EXPECT_EQ(ExternalSupportDialog::tooltipText(T::LineXPinned),
              "Apoyo con desplazamiento libre en una recta paralela al X global y desplazamiento impedido en un plano paralelo al plano YZ global.");
    EXPECT_EQ(ExternalSupportDialog::tooltipTitle(T::PlaneXYPinned), "Desplazamiento libre sobre un plano paralelo a los ejes XY con giros sin coaccionar");
    EXPECT_EQ(ExternalSupportDialog::tooltipText(T::PlaneXYPinned),
              "Apoyo con desplazamiento libre en un plano paralelo al XY global y desplazamiento impedido en una recta paralela al eje Z global.");
    EXPECT_EQ(ExternalSupportDialog::tooltipText(T::PlaneUserPinned),
              "Apoyo con desplazamiento libre en un plano cualquiera (se define con su vector director) y desplazamiento impedido en cualquier recta perpendicular al plano donde tiene libre desplazamiento.");
    ExternalSupportDialog d;
    EXPECT_TRUE(d.typeButtons()[19]->toolTip().contains(QStringLiteral("con giros sin coaccionar")));
}

TEST_F(SupportDialogsTest, StartsOnFreeWithAnEmptyPanel) {
    ExternalSupportDialog d;
    EXPECT_EQ(d.selectedType(), ExternalSupportType::Free);
    EXPECT_TRUE(d.typeButtons()[0]->isChecked());
    EXPECT_EQ(visibleDofs(d), (Dofs{}));
    EXPECT_FALSE(d.vectorVisible());
    EXPECT_EQ(d.support().type, ExternalSupportType::Free);
}

TEST_F(SupportDialogsTest, PanelsOfTheBasicTypes) {
    ExternalSupportDialog d;
    d.selectType(ExternalSupportType::Fixed);
    EXPECT_EQ(visibleDofs(d), (Dofs{true, true, true, true, true, true}));
    for (int i = 0; i < 6; ++i) {
        EXPECT_TRUE(d.dof(i).fixed->isChecked()) << i;  // "Fijo" by default
        EXPECT_FALSE(d.dof(i).free->isVisibleTo(d.dof(i).row)) << "no 'Libre' in a predefined type";
    }
    d.selectType(ExternalSupportType::Pinned);
    EXPECT_EQ(visibleDofs(d), (Dofs{true, true, true, false, false, false})) << "no group Giros";
    EXPECT_FALSE(d.rotationsVisible());
}

TEST_F(SupportDialogsTest, LinesAndPlanesShowOnlyTheBlockedDisplacementsAndTheVector) {
    ExternalSupportDialog d;
    struct Case { ExternalSupportType type; Dofs dofs; Point3 vector; };
    const Case cases[] = {
        {ExternalSupportType::LineXFixed, {false, true, true, true, true, true}, {1, 0, 0}},
        {ExternalSupportType::LineYFixed, {true, false, true, true, true, true}, {0, 1, 0}},
        {ExternalSupportType::LineZFixed, {true, true, false, true, true, true}, {0, 0, 1}},
        {ExternalSupportType::LineUserFixed, {false, true, true, true, true, true}, {1, 0, 0}},
        {ExternalSupportType::PlaneXYFixed, {false, false, true, true, true, true}, {0, 0, 1}},
        {ExternalSupportType::PlaneXZFixed, {false, true, false, true, true, true}, {0, 1, 0}},
        {ExternalSupportType::PlaneYZFixed, {true, false, false, true, true, true}, {1, 0, 0}},
        {ExternalSupportType::PlaneUserFixed, {false, false, true, true, true, true}, {0, 0, 1}},
        {ExternalSupportType::LineZPinned, {true, true, false, false, false, false}, {0, 0, 1}},
        {ExternalSupportType::PlaneYZPinned, {true, false, false, false, false, false}, {1, 0, 0}},
    };
    // The measured panels show Dy,Dz for the recta X, Dz for the plano XY, ... (orig_vinc_ext_panels_tipos_*.png).
    for (const Case& c : cases) {
        d.selectType(c.type);
        const Dofs expected = c.dofs;
        Dofs shown{};
        for (int i = 0; i < 6; ++i) shown[static_cast<std::size_t>(i)] = d.dofVisible(i);
        // Plane types keep only the blocked displacement: reduce the expectation accordingly (Dx..Dz are the blocked ones).
        Dofs want{};
        const SupportTypeInfo info = supportTypeInfo(c.type);
        for (int i = 0; i < 3; ++i) {
            const bool blocked = info.kind == SupportKind::Line ? (info.userVector ? i != 0 : i != info.axis)
                                                                 : (info.userVector ? i == 2 : i == info.axis);
            want[static_cast<std::size_t>(i)] = blocked;
        }
        for (int i = 3; i < 6; ++i) want[static_cast<std::size_t>(i)] = info.rotationsFixed;
        EXPECT_EQ(shown, want) << supportTypeIndex(c.type);
        (void)expected;
        EXPECT_TRUE(d.vectorVisible());
        EXPECT_NEAR(d.vectorEdit(0)->value(), c.vector.x, 1e-12);
        EXPECT_NEAR(d.vectorEdit(1)->value(), c.vector.y, 1e-12);
        EXPECT_NEAR(d.vectorEdit(2)->value(), c.vector.z, 1e-12);
        EXPECT_EQ(d.vectorEditable(), info.userVector) << "disabled for the predefined axes, editable for the user ones";
        EXPECT_EQ(d.vectorEdit(0)->isEnabled(), info.userVector);
    }
}

TEST_F(SupportDialogsTest, UserVectorFollowsTheAxisItIsParallelTo) {
    ExternalSupportDialog d;
    d.selectType(ExternalSupportType::LineUserFixed);
    EXPECT_TRUE(type(d.vectorEdit(0), "0"));
    EXPECT_TRUE(type(d.vectorEdit(1), "1"));
    d.vectorEdit(1)->commit();
    EXPECT_FALSE(d.dofVisible(1)) << "line along Y: Dy is free";
    EXPECT_TRUE(d.dofVisible(0));
    EXPECT_TRUE(d.dofVisible(2));
    const ExternalSupport s = d.support();
    EXPECT_EQ(s.userVector.y, 1.0);
    EXPECT_EQ(restraintOf(s).fixed, (Dofs{true, false, true, true, true, true}));
    // Tooltip of the vector fields, literal (with the stray " ." of the original).
    EXPECT_TRUE(d.vectorEdit(0)->toolTip().contains(QStringLiteral("Debe estar entre -1000.000 y 1000.000 .")));
}

TEST_F(SupportDialogsTest, GenericPanelFollowsTheMeasuredModes) {
    ExternalSupportDialog d;
    d.selectType(ExternalSupportType::Generic);
    EXPECT_TRUE(d.modeRadio(GenericDisplacementMode::Independent)->isChecked());
    EXPECT_EQ(visibleDofs(d), (Dofs{true, true, true, true, true, true}));
    for (int i = 0; i < 6; ++i) {
        EXPECT_TRUE(d.dof(i).free->isChecked()) << "Libre by default";
        EXPECT_TRUE(d.dof(i).free->isVisibleTo(d.dof(i).row));
    }
    EXPECT_FALSE(d.vectorVisible());
    EXPECT_FALSE(d.support().type != ExternalSupportType::Generic);
    EXPECT_FALSE(restraintOf(d.support()).any()) << "everything Libre = a free node";

    d.modeRadio(GenericDisplacementMode::OnLine)->setChecked(true);
    EXPECT_TRUE(d.vectorVisible());
    EXPECT_EQ(d.vectorEdit(0)->value(), 1.0);
    EXPECT_EQ(visibleDofs(d), (Dofs{false, true, true, true, true, true})) << "only Dy and Dz, then the rotations";
    EXPECT_TRUE(d.dof(1).fixed->isChecked());
    EXPECT_FALSE(d.dof(1).free->isVisibleTo(d.dof(1).row));
    EXPECT_TRUE(d.dof(3).free->isChecked()) << "the rotations stay Libre";

    d.modeRadio(GenericDisplacementMode::OnPlane)->setChecked(true);
    EXPECT_EQ(d.vectorEdit(2)->value(), 1.0);
    EXPECT_EQ(visibleDofs(d), (Dofs{false, false, true, true, true, true})) << "only Dz";
}

TEST_F(SupportDialogsTest, ElasticRowKeepsItsOwnKAndSense) {
    ExternalSupportDialog d;
    d.selectType(ExternalSupportType::Fixed);
    d.dof(2).elastic->setChecked(true);
    d.dof(5).elastic->setChecked(true);
    EXPECT_EQ(d.dof(2).k->value(), 100000.0) << "default K";
    EXPECT_EQ(d.dof(2).unit->text(), QStringLiteral("kN/m"));
    EXPECT_EQ(d.dof(5).unit->text(), QStringLiteral("kN·m/rad"));
    ASSERT_EQ(d.dof(2).sense->count(), 3);
    EXPECT_EQ(d.dof(2).sense->itemText(0), QStringLiteral("Ambos"));
    EXPECT_EQ(d.dof(2).sense->itemText(1), QStringLiteral("Positivo"));
    EXPECT_EQ(d.dof(2).sense->itemText(2), QStringLiteral("Negativo"));
    EXPECT_TRUE(type(d.dof(2).k, "5000"));
    d.dof(2).sense->setCurrentIndex(2);
    d.dof(5).sense->setCurrentIndex(1);
    const ExternalSupport s = d.support();
    EXPECT_EQ(s.elasticDofs, (Dofs{false, false, true, false, false, true}));
    EXPECT_EQ(s.elasticConstants[2], 5000.0);
    EXPECT_EQ(s.senses[2], SupportSense::Negative);
    EXPECT_EQ(s.senses[5], SupportSense::Positive);
    EXPECT_EQ(s.senses[0], SupportSense::Both);
    EXPECT_EQ(restraintOf(s).elastic, (Dofs{false, false, true, false, false, true}));
}

TEST_F(SupportDialogsTest, KFieldValidationMessages) {
    ExternalSupportDialog d;
    d.selectType(ExternalSupportType::Pinned);
    RangedEdit* k = d.dof(0).k;
    EXPECT_EQ(k->decimals(), 0);
    EXPECT_FALSE(type(k, "0"));
    EXPECT_EQ(warnings.last(), QStringLiteral("El valor de 'K' debe estar entre 1 y 10000000."));
    EXPECT_EQ(k->value(), 100000.0) << "the field recovers its previous value";
    EXPECT_FALSE(type(k, "-5"));
    EXPECT_FALSE(type(k, "20000000"));
    EXPECT_EQ(warnings.last(), QStringLiteral("El valor de 'K' debe estar entre 1 y 10000000."));
    EXPECT_FALSE(type(k, "abc"));
    EXPECT_EQ(warnings.last(), QStringLiteral("La cadena introducida no es un número."));
    EXPECT_FALSE(type(k, "1,5")) << "a decimal comma is not a number";
    EXPECT_EQ(warnings.last(), QStringLiteral("La cadena introducida no es un número."));
    EXPECT_TRUE(type(k, "12.5678"));
    EXPECT_EQ(k->value(), 13.0) << "rounded to an integer";
    EXPECT_TRUE(type(k, "1"));
    EXPECT_TRUE(type(k, "10000000"));
    EXPECT_EQ(warnings.size(), 5);
}

TEST_F(SupportDialogsTest, VectorFieldRangeAndZeroVector) {
    ExternalSupportDialog d;
    d.selectType(ExternalSupportType::PlaneUserFixed);
    EXPECT_EQ(d.vectorEdit(0)->decimals(), 3);
    EXPECT_FALSE(type(d.vectorEdit(0), "1001"));
    EXPECT_EQ(warnings.last(), QStringLiteral("El valor de 'X' debe estar entre -1000.000 y 1000.000."));
    EXPECT_TRUE(type(d.vectorEdit(0), "-1000"));
    EXPECT_TRUE(type(d.vectorEdit(0), "0"));
    EXPECT_TRUE(type(d.vectorEdit(2), "0"));
    d.accept();  // the elastic question must not come first (measured)
    EXPECT_EQ(d.result(), QDialog::Rejected) << "a zero direction keeps the window open";
    EXPECT_EQ(warnings.last(), QStringLiteral("La dirección introducida no es correcta."));
    EXPECT_TRUE(questions.isEmpty());
}

TEST_F(SupportDialogsTest, ElasticDefaultKAsksQuestion65010) {
    ExternalSupportDialog d;
    d.selectType(ExternalSupportType::Fixed);
    d.dof(4).elastic->setChecked(true);  // a rotation: the %s is the literal "K" all the same
    answer = false;
    d.accept();
    ASSERT_EQ(questions.size(), 1);
    EXPECT_TRUE(questions[0].startsWith(QStringLiteral("El valor introducido para la constante de apoyo elástico (K) coincide con el valor por defecto.")));
    EXPECT_TRUE(questions[0].endsWith(QStringLiteral("¿Desea continuar?")));
    EXPECT_NE(d.result(), QDialog::Accepted) << "No keeps the dialog open";
    answer = true;
    d.accept();
    EXPECT_EQ(questions.size(), 2);
    EXPECT_EQ(d.result(), QDialog::Accepted);

    ExternalSupportDialog other;
    other.selectType(ExternalSupportType::Fixed);
    other.dof(4).elastic->setChecked(true);
    EXPECT_TRUE(type(other.dof(4).k, "5000"));
    other.accept();
    EXPECT_EQ(questions.size(), 2) << "another K does not ask";
    EXPECT_EQ(other.result(), QDialog::Accepted);
}

TEST_F(SupportDialogsTest, PressingGenericDoesNotPreloadThePreviousState) {
    ExternalSupport fixed;
    fixed.type = ExternalSupportType::Fixed;
    fixed.elasticDofs[0] = true;
    fixed.elasticConstants[0] = 777;
    ExternalSupportDialog d(fixed);
    EXPECT_EQ(d.selectedType(), ExternalSupportType::Fixed) << "reopened on the type of the node";
    EXPECT_TRUE(d.typeButtons()[3]->isChecked());
    EXPECT_TRUE(d.dof(0).elastic->isChecked());
    EXPECT_EQ(d.dof(0).k->value(), 777.0);
    EXPECT_EQ(d.support().elasticDofs[0], true);
    d.selectType(ExternalSupportType::Generic);
    for (int i = 0; i < 6; ++i) EXPECT_TRUE(d.dof(i).free->isChecked());
    EXPECT_EQ(d.dof(0).k->value(), 100000.0);
}

TEST_F(SupportDialogsTest, GenericSupportRoundTrip) {
    ExternalSupport g;
    g.type = ExternalSupportType::Generic;
    g.generic.mode = GenericDisplacementMode::OnLine;
    g.generic.vector = {0, 1, 0};
    g.generic.translations = {DofMode::Elastic, DofMode::Free, DofMode::Fixed};
    g.generic.rotations = {DofMode::Free, DofMode::Fixed, DofMode::Elastic};
    g.elasticConstants[0] = 2500;
    g.elasticConstants[5] = 3500;
    g.senses[5] = SupportSense::Negative;
    ExternalSupportDialog d(g);
    EXPECT_TRUE(d.modeRadio(GenericDisplacementMode::OnLine)->isChecked());
    EXPECT_EQ(d.vectorEdit(1)->value(), 1.0);
    EXPECT_FALSE(d.dofVisible(1)) << "line along Y";
    const ExternalSupport back = d.support();
    EXPECT_EQ(back.generic.mode, GenericDisplacementMode::OnLine);
    EXPECT_EQ(back.generic.translations[0], DofMode::Elastic);
    EXPECT_EQ(back.generic.translations[1], DofMode::Free);
    EXPECT_EQ(back.generic.translations[2], DofMode::Fixed);
    EXPECT_EQ(back.generic.rotations, g.generic.rotations);
    EXPECT_EQ(back.elasticConstants[0], 2500.0);
    EXPECT_EQ(back.elasticConstants[5], 3500.0);
    EXPECT_EQ(back.senses[5], SupportSense::Negative);
}

// ----- Vinculación interior ---------------------------------------------------------------------------------------------------

namespace {
std::vector<Point3> threePieces() { return {{-1, 0, 0}, {0, 0, 1}, {1, 0, 0}}; }  // A left, B up, C right
}

TEST_F(SupportDialogsTest, InternalDialogLayoutAndDefaults) {
    InternalLinkDialog d({}, threePieces(), false);
    EXPECT_EQ(d.size(), QSize(748, 449));
    EXPECT_EQ(d.windowTitle(), QStringLiteral("Vinculación interior"));
    EXPECT_EQ(d.selectedType(), InternalLinkType::Fixed) << "Nudo empotrado by default";
    EXPECT_TRUE(d.typeButtons()[0]->isChecked());
    EXPECT_EQ(InternalLinkDialog::tooltipText(InternalLinkType::PartialFixity),
              QStringLiteral("Nudo empotrado con coeficiente de empotramiento parcial para todas las piezas"));
    EXPECT_EQ(InternalLinkDialog::tooltipText(InternalLinkType::Pinned), QStringLiteral("Nudo articulado"));
    EXPECT_EQ(InternalLinkDialog::tooltipText(InternalLinkType::Generic), QStringLiteral("Nudo genérico"));
    EXPECT_EQ(d.link().type, InternalLinkType::Fixed);
    EXPECT_FALSE(d.warningBand()->isVisibleTo(&d));
}

TEST_F(SupportDialogsTest, PartialCoefficientField) {
    InternalLinkDialog d({}, threePieces(), false);
    d.selectType(InternalLinkType::PartialFixity);
    RangedEdit* e = d.coefficientEdit();
    EXPECT_TRUE(e->isVisibleTo(&d));
    EXPECT_EQ(e->value(), 0.0);
    EXPECT_EQ(e->text(), QStringLiteral("0.00"));
    EXPECT_EQ(e->decimals(), 2);
    EXPECT_TRUE(e->toolTip().contains(QStringLiteral("Debe estar entre 0.00 y 10000000.00.")));
    EXPECT_TRUE(e->toolTip().contains(QStringLiteral("Define el grado de empotramiento entre las piezas que llegan al nudo.")));
    EXPECT_FALSE(type(e, "-1"));
    EXPECT_EQ(warnings.last(), QStringLiteral("El valor de 'Coef.Emp.Parc.' debe estar entre 0.00 y 10000000.00."));
    EXPECT_FALSE(type(e, "20000000"));
    EXPECT_TRUE(type(e, "5000"));
    EXPECT_EQ(d.link().type, InternalLinkType::PartialFixity);
    EXPECT_EQ(d.link().partialCoefficient, 5000.0);
    d.selectType(InternalLinkType::Pinned);
    EXPECT_FALSE(e->isVisibleTo(&d)) << "Articulado: no controls";
}

TEST_F(SupportDialogsTest, GenericTableHasOneMarkPerPiece) {
    InternalLinkDialog d({}, threePieces(), false);
    d.selectType(InternalLinkType::Generic);
    QTableWidget* t = d.table();
    ASSERT_EQ(t->rowCount(), 3);
    ASSERT_EQ(t->columnCount(), 6);
    const QStringList headers{"Vínc", "", "Coef.Emp.Parc.", "A", "B", "C"};
    for (int c = 0; c < 6; ++c) EXPECT_EQ(t->horizontalHeaderItem(c)->text(), headers[c]);
    // Default (measured): row 0 has A, B and C marked; partial boxes empty, coefficient 0.000.
    InternalLink l = d.link();
    EXPECT_EQ(l.elementRow, (std::vector<int>{0, 0, 0}));
    for (int r = 0; r < 3; ++r) {
        EXPECT_FALSE(l.rows[static_cast<std::size_t>(r)].partialToNode);
        EXPECT_EQ(t->item(r, 2)->text(), QStringLiteral("0.000"));
    }
    // Marking A in link 1 takes it away from link 0.
    d.markPiece(0, 1);
    EXPECT_EQ(t->item(0, 3)->checkState(), Qt::Unchecked);
    EXPECT_EQ(t->item(1, 3)->checkState(), Qt::Checked);
    EXPECT_EQ(d.link().elementRow, (std::vector<int>{1, 0, 0}));
    // A piece cannot be left without a mark.
    t->item(1, 3)->setCheckState(Qt::Unchecked);
    EXPECT_EQ(t->item(1, 3)->checkState(), Qt::Checked);
    EXPECT_TRUE(cc::core::checkInternalLink(d.link(), 3).empty());
}

TEST_F(SupportDialogsTest, GenericSketchFollowsTheTable) {
    InternalLinkDialog d({}, threePieces(), false);
    d.selectType(InternalLinkType::Generic);
    auto* s = d.sketch();
    const double pi = 3.14159265358979323846;
    EXPECT_NEAR(s->angleOf(0), pi, 1e-9) << "A on the left";
    EXPECT_NEAR(s->angleOf(1), pi / 2, 1e-9) << "B up";
    EXPECT_NEAR(s->angleOf(2), 0.0, 1e-9) << "C on the right";
    EXPECT_TRUE(s->circleDrawn()) << "pieces fixed to each other are joined by a circle";
    d.markPiece(0, 1);
    d.markPiece(1, 2);
    EXPECT_FALSE(s->circleDrawn()) << "every piece alone: no circle (orig_vinc_int_generica_marcas.png, right)";
    d.markPiece(1, 0);
    EXPECT_TRUE(s->circleDrawn());
    // The partial box paints the pieces of its row red.
    EXPECT_FALSE(s->pieceRed(0));
    d.setPartial(1, true);
    EXPECT_TRUE(s->pieceRed(0));
    EXPECT_FALSE(s->pieceRed(1));
    EXPECT_TRUE(d.link().rows[1].partialToNode);
}

TEST_F(SupportDialogsTest, GenericCoefficientCellValidation) {
    InternalLinkDialog d({}, threePieces(), false);
    d.selectType(InternalLinkType::Generic);
    QTableWidgetItem* cell = d.table()->item(0, 2);
    cell->setText("1250.5");
    EXPECT_EQ(cell->text(), QStringLiteral("1250.500"));
    EXPECT_EQ(d.link().rows[0].coefficient, 1250.5);
    cell->setText("abc");
    EXPECT_EQ(warnings.last(), QStringLiteral("La cadena introducida no es un número."));
    EXPECT_EQ(cell->text(), QStringLiteral("1250.500")) << "the previous value comes back";
    cell->setText("-3");
    EXPECT_EQ(warnings.last(), QStringLiteral("El valor de 'Coef.Emp.Parc.' debe estar entre 0.00 y 10000000.00."));
    EXPECT_EQ(d.link().rows[0].coefficient, 1250.5);
}

TEST_F(SupportDialogsTest, MixedPieceCountsShowTheBandAndGrowTheWindow) {
    InternalLinkDialog d({}, threePieces(), true);
    EXPECT_FALSE(d.warningBand()->isVisibleTo(&d)) << "only in the generic mode";
    EXPECT_EQ(d.size(), QSize(748, 449));
    d.selectType(InternalLinkType::Generic);
    EXPECT_TRUE(d.warningBand()->isVisibleTo(&d));
    EXPECT_EQ(d.warningBand()->text(),
              QStringLiteral("Ha seleccionado nudos con distinto número de piezas. La configuración de la vinculación interior genérica editada sólo se aplicará a nudos con el mismo número de piezas."));
    EXPECT_EQ(d.size(), QSize(748, 503));
    d.selectType(InternalLinkType::Pinned);
    EXPECT_EQ(d.size(), QSize(748, 449));
}

TEST_F(SupportDialogsTest, GenericLinkIsRestoredFromTheNode) {
    InternalLink g;
    g.type = InternalLinkType::Generic;
    g.elementRow = {1, 0, 1};
    g.rows = {{false, 0.0}, {true, 250.0}, {false, 0.0}};
    InternalLinkDialog d(g, threePieces(), false);
    EXPECT_EQ(d.selectedType(), InternalLinkType::Generic) << "reopened as Genérico";
    EXPECT_EQ(d.link(), g);
}

// ----- Pictograms and 3D symbols ----------------------------------------------------------------------------------------------

namespace {

constexpr cc::app::SymbolAxes kAxes{QPointF(1, 0), QPointF(0.87, 0.5), QPointF(0, -1)};  // X right, Y right-down, Z up

QImage canvas() {
    QImage image(300, 260, QImage::Format_ARGB32);
    image.fill(Qt::white);
    return image;
}
int dark(const QImage& image, QRect area = {}) {
    int n = 0;
    const QRect r = area.isNull() ? image.rect() : area & image.rect();
    for (int y = r.top(); y <= r.bottom(); ++y)
        for (int x = r.left(); x <= r.right(); ++x)
            if (qAlpha(image.pixel(x, y)) != 0 && image.pixel(x, y) != 0xFFFFFFFF) ++n;
    return n;
}
}  // namespace

TEST(SupportPictograms, EveryTypeHasADistinctNonEmptyPictogram) {
    app();
    std::vector<QImage> images;
    for (ExternalSupportType t : allExternalSupportTypes()) {
        const QPixmap pm = cc::app::externalSupportPictogram(t, 38);
        ASSERT_FALSE(pm.isNull());
        images.push_back(pm.toImage());
        EXPECT_GT(dark(images.back().convertToFormat(QImage::Format_ARGB32)), 20) << supportTypeIndex(t);
    }
    for (std::size_t i = 0; i < images.size(); ++i)
        for (std::size_t j = i + 1; j < images.size(); ++j) EXPECT_NE(images[i], images[j]) << i << " vs " << j;
    for (InternalLinkType t : {InternalLinkType::Fixed, InternalLinkType::PartialFixity, InternalLinkType::Pinned, InternalLinkType::Generic})
        EXPECT_FALSE(cc::app::internalLinkPictogram(t, 32).isNull());
}

TEST(SupportPictograms, ExternalSymbolsFollowTheMeasuredGlyphs) {
    app();
    const QPointF node(150, 60);
    auto draw = [&](const ExternalSupport& s) {
        QImage image = canvas();
        QPainter p(&image);
        const bool drawn = cc::app::drawExternalSupportSymbol(p, s, node, kAxes, QColor(0, 102, 204));
        p.end();
        return std::make_pair(drawn, image);
    };
    ExternalSupport s;
    // Free: nothing at all (only the blue square of the node, drawn elsewhere).
    auto [freeDrawn, freeImage] = draw(s);
    EXPECT_FALSE(freeDrawn);
    EXPECT_EQ(dark(freeImage), 0);
    // Fixed: a horizontal line through the node with hatching below, about 90 px wide.
    s.type = ExternalSupportType::Fixed;
    auto [fixedDrawn, fixedImage] = draw(s);
    EXPECT_TRUE(fixedDrawn);
    EXPECT_GT(dark(fixedImage, QRect(105, 59, 91, 3)), 80);
    EXPECT_GT(dark(fixedImage, QRect(105, 62, 91, 20)), 20) << "hatching";
    EXPECT_EQ(dark(fixedImage, QRect(0, 0, 300, 50)), 0) << "it hangs below the node";
    EXPECT_EQ(dark(fixedImage, QRect(0, 0, 100, 260)), 0) << "width of 90 px";
    // Pinned: a triangle with the apex at the node.
    s.type = ExternalSupportType::Pinned;
    auto [pinnedDrawn, pinnedImage] = draw(s);
    EXPECT_TRUE(pinnedDrawn);
    EXPECT_GT(dark(pinnedImage, QRect(140, 60, 20, 40)), 30);
    // A generic restraint with the three displacements blocked looks the same (measured).
    ExternalSupport generic;
    generic.type = ExternalSupportType::Generic;
    generic.generic.translations = {DofMode::Fixed, DofMode::Fixed, DofMode::Elastic};
    auto [genericDrawn, genericImage] = draw(generic);
    EXPECT_TRUE(genericDrawn);
    EXPECT_EQ(genericImage, pinnedImage);
    // Lines and planes: guide lines with wheels; planes draw more than lines.
    s.type = ExternalSupportType::LineXFixed;
    const int line = dark(draw(s).second);
    s.type = ExternalSupportType::PlaneXYFixed;
    const int plane = dark(draw(s).second);
    EXPECT_GT(line, 100);
    EXPECT_GT(plane, line);
    s.type = ExternalSupportType::PlaneXYPinned;
    EXPECT_GT(dark(draw(s).second), plane) << "the articuladas add the triangle lines";
    // The glyph follows the view: the Z line is vertical, the X line horizontal.
    s.type = ExternalSupportType::LineZFixed;
    const QImage zLine = draw(s).second;
    EXPECT_GT(dark(zLine, QRect(148, 20, 5, 80)), 40);
    // No text: the symbol never uses text (checked by the painting only touching the drawn lines; the glyphs have no label).
}

TEST(SupportPictograms, SymbolSizeIsConstantOnScreen) {
    app();
    ExternalSupport s;
    s.type = ExternalSupportType::Fixed;
    QImage image = canvas();
    QPainter p(&image);
    cc::app::drawExternalSupportSymbol(p, s, QPointF(150, 60), kAxes, Qt::blue);
    p.end();
    int minX = 999, maxX = -1;
    for (int y = 0; y < image.height(); ++y)
        for (int x = 0; x < image.width(); ++x)
            if (image.pixel(x, y) != 0xFFFFFFFF) { minX = std::min(minX, x); maxX = std::max(maxX, x); }
    EXPECT_NEAR(maxX - minX, cc::app::kSupportSymbolWidth, 3.0);
}

TEST(SupportPictograms, InternalSymbols) {
    app();
    const QPointF node(60, 60);
    auto draw = [&](const InternalLink& l) {
        QImage image(120, 120, QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter p(&image);
        const bool drawn = cc::app::drawInternalLinkSymbol(p, l, node, QColor(0, 102, 204));
        p.end();
        return std::make_pair(drawn, dark(image));
    };
    InternalLink l;  // fixed: nothing
    EXPECT_FALSE(draw(l).first);
    EXPECT_EQ(draw(l).second, 0);
    l.type = InternalLinkType::Pinned;
    const auto pinned = draw(l);
    EXPECT_TRUE(pinned.first);
    EXPECT_GT(pinned.second, 15);
    l.type = InternalLinkType::PartialFixity;
    const auto partial = draw(l);
    EXPECT_TRUE(partial.first);
    EXPECT_GT(partial.second, pinned.second) << "double circle";
    // Generic: the default (one row with every element, no partial box) draws nothing; anything else a single circle.
    InternalLink g;
    g.type = InternalLinkType::Generic;
    g.elementRow = {0, 0, 0};
    g.rows = {{false, 0.0}, {false, 0.0}, {false, 0.0}};
    EXPECT_FALSE(cc::app::genericLinkIsNonTrivial(g));
    EXPECT_FALSE(draw(g).first);
    g.elementRow = {1, 0, 0};
    EXPECT_TRUE(cc::app::genericLinkIsNonTrivial(g));
    const auto generic = draw(g);
    EXPECT_TRUE(generic.first);
    EXPECT_EQ(generic.second, pinned.second) << "a simple circle like the articulado";
}

// ----- Tool -------------------------------------------------------------------------------------------------------------------

namespace {

void mouse(Viewport3D& v, QEvent::Type type, QPointF pos, Qt::MouseButton button, Qt::MouseButtons buttons) {
    QMouseEvent e(type, pos, v.mapToGlobal(pos), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(&v, &e);
}
void click(Viewport3D& v, QPointF pos, Qt::MouseButton button = Qt::LeftButton) {
    mouse(v, QEvent::MouseMove, pos, Qt::NoButton, Qt::NoButton);
    mouse(v, QEvent::MouseButtonPress, pos, button, button);
    mouse(v, QEvent::MouseButtonRelease, pos, button, Qt::NoButton);
}

/// Seen from the front: N1 (0,0,0), N2 (4,0,0), N3 (4,0,3) with the bars N1-N3 and N2-N3 (N3 has two pieces, N1 and N2 one each).
class SupportToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();
        auto& model = project.structure;
        model.addNode({0, 0, 0});
        model.addNode({4, 0, 0});
        model.addNode({4, 0, 3});
        model.addNode({8, 0, 0});  // N4: a lone node
        model.addBar(1, 3);
        model.addBar(2, 3);
        view = std::make_unique<Viewport3D>();
        view->resize(800, 600);
        view->setStandardView(cc::core::StandardView::Front);
        view->setOrthographic(true);
        view->camera().target = {4, 0, 1.5};
        view->setStructure(&model);
        view->setNodeSupports(&project.nodeSupports);
        tool = std::make_unique<SupportTool>();
        tool->setProject(&project, &undo);
        tool->addViewport(view.get());
        undo.setChangeCallback([this] { tool->structureChanged(); });
        QObject::connect(tool.get(), &SupportTool::statusChanged, [this](const QString& text) { status = text; });
        RangedEdit::setWarningHandler([this](QWidget*, const QString& text) { warnings << text; });
        undo.setClean();
    }
    void TearDown() override { RangedEdit::setWarningHandler({}); }
    QPointF screenOf(int id) {
        const Node& n = *project.structure.findNode(id);
        QPointF p;
        EXPECT_TRUE(view->worldToScreen({n.position.x, n.position.y, n.position.z}, p));
        return p;
    }
    Project project;
    UndoStack undo;
    std::unique_ptr<Viewport3D> view;
    std::unique_ptr<SupportTool> tool;
    QString status;
    QStringList warnings;
};

}  // namespace

TEST_F(SupportToolTest, ButtonsAndStatusTexts) {
    EXPECT_EQ(tool->externalAction()->text(), QStringLiteral("Vinculación exterior"));
    EXPECT_EQ(tool->internalAction()->text(), QStringLiteral("Vinculación interior"));
    EXPECT_TRUE(tool->externalAction()->isEnabled());
    SupportTool other;
    EXPECT_FALSE(other.externalAction()->isEnabled()) << "no project";
    tool->externalAction()->trigger();
    EXPECT_EQ(tool->mode(), SupportTool::Mode::External);
    EXPECT_EQ(status, QStringLiteral("Nudos - Vinculación exterior |  - LMB: Seleccione un grupo de elementos. - RMB: Actuar sobre los elementos seleccionados."));
    EXPECT_TRUE(view->selectionMode());
    tool->internalAction()->trigger();
    EXPECT_EQ(tool->mode(), SupportTool::Mode::Internal);
    EXPECT_FALSE(tool->externalAction()->isChecked());
    tool->internalAction()->trigger();
    EXPECT_EQ(tool->mode(), SupportTool::Mode::None);
    EXPECT_FALSE(view->selectionMode());
}

TEST_F(SupportToolTest, RightClickOnSelectedNodesAssignsTheExternalRestraintAsOneUndoStep) {
    bool opened = false;
    tool->setDialogRunner([&](QDialog& d) {
        auto& dialog = static_cast<ExternalSupportDialog&>(d);
        opened = true;
        EXPECT_EQ(dialog.selectedType(), ExternalSupportType::Free) << "a new node starts free";
        dialog.selectType(ExternalSupportType::Fixed);
        return static_cast<int>(QDialog::Accepted);
    });
    tool->externalAction()->trigger();
    click(*view, screenOf(1));
    click(*view, screenOf(2));  // a plain click replaces the selection: N2 only
    click(*view, screenOf(2), Qt::RightButton);
    EXPECT_TRUE(opened);
    ASSERT_EQ(project.nodeSupports.external.size(), 1u);
    EXPECT_EQ(project.nodeSupports.external.at(2).type, ExternalSupportType::Fixed);
    EXPECT_EQ(undo.count(), 1u);
    EXPECT_TRUE(view->selection().empty()) << "the selection is cleared after acting";
    undo.undo();
    EXPECT_TRUE(project.nodeSupports.empty());
    undo.redo();
    EXPECT_EQ(project.nodeSupports.external.count(2), 1u);
}

TEST_F(SupportToolTest, ReopeningStartsOnTheTypeOfTheNodeAndCancelChangesNothing) {
    ASSERT_EQ(assignExternalSupport(project, undo, {3}, [] { ExternalSupport s; s.type = ExternalSupportType::PlaneXZPinned; return s; }()), 1);
    ExternalSupportType seen = ExternalSupportType::Free;
    tool->setDialogRunner([&](QDialog& d) {
        seen = static_cast<ExternalSupportDialog&>(d).selectedType();
        return static_cast<int>(QDialog::Rejected);
    });
    const std::size_t before = undo.count();
    EXPECT_FALSE(tool->editExternal({3}));
    EXPECT_EQ(seen, ExternalSupportType::PlaneXZPinned);
    EXPECT_EQ(undo.count(), before);
}

TEST_F(SupportToolTest, InternalLinkUsesThePiecesOfTheNode) {
    std::size_t pieces = 0;
    bool mixed = false;
    tool->setDialogRunner([&](QDialog& d) {
        auto& dialog = static_cast<InternalLinkDialog&>(d);
        pieces = static_cast<std::size_t>(dialog.table()->rowCount());
        mixed = dialog.warningBand()->text().startsWith(QStringLiteral("Ha seleccionado")) && dialog.selectedType() == InternalLinkType::Generic;
        dialog.selectType(InternalLinkType::Generic);
        mixed = dialog.warningBand()->isVisibleTo(&dialog);
        dialog.markPiece(0, 1);  // A alone
        return static_cast<int>(QDialog::Accepted);
    });
    // N3 (two bars) together with N1 (one bar): the table has two columns and the band warns.
    EXPECT_TRUE(tool->editInternal({1, 3}));
    EXPECT_EQ(pieces, 2u);
    EXPECT_TRUE(mixed);
    // Only the node with the same number of pieces receives the generic link.
    ASSERT_EQ(project.nodeSupports.internal.size(), 1u);
    const InternalLink& link = project.nodeSupports.internal.at(3);
    EXPECT_EQ(link.type, InternalLinkType::Generic);
    EXPECT_EQ(link.elementRow.size(), 2u);
    EXPECT_EQ(link.elementRow[0], 1);
    undo.undo();
    EXPECT_TRUE(project.nodeSupports.internal.empty());
}

TEST_F(SupportToolTest, PinnedInternalLinkIsAppliedToEverySelectedNode) {
    tool->setDialogRunner([&](QDialog& d) {
        static_cast<InternalLinkDialog&>(d).selectType(InternalLinkType::Pinned);
        return static_cast<int>(QDialog::Accepted);
    });
    EXPECT_TRUE(tool->editInternal({1, 2, 3}));
    EXPECT_EQ(project.nodeSupports.internal.size(), 3u);
    EXPECT_EQ(undo.count(), 1u);
}

TEST_F(SupportToolTest, TheViewportKeepsAPointerToTheSupports) {
    EXPECT_EQ(view->nodeSupports(), &project.nodeSupports);
}

// Developer aid: with CC_DUMP_DIR set (and CC_REAL_GUI so that real fonts are used) the dialogs and symbols are written as PNG files.
TEST(SupportVisual, DumpImagesWhenAsked) {
    const QByteArray dir = qgetenv("CC_DUMP_DIR");
    if (dir.isEmpty()) GTEST_SKIP();
    app();
    const QString base = QString::fromLocal8Bit(dir) + QLatin1Char('/');
    const auto shot = [](QWidget& w) {
        w.show();
        QApplication::processEvents();
        return w.grab();
    };
    ExternalSupportDialog fixed;
    fixed.selectType(ExternalSupportType::Fixed);
    fixed.dof(2).elastic->setChecked(true);
    shot(fixed).save(base + QStringLiteral("ext_fixed_elastic.png"));
    ExternalSupportDialog generic;
    generic.selectType(ExternalSupportType::Generic);
    generic.dof(0).fixed->setChecked(true);
    generic.dof(3).elastic->setChecked(true);
    generic.show();
    QApplication::processEvents();
    shot(generic).save(base + QStringLiteral("ext_generic.png"));
    ExternalSupportDialog line;
    line.selectType(ExternalSupportType::LineUserFixed);
    shot(line).save(base + QStringLiteral("ext_line_user.png"));
    ExternalSupportDialog free;
    shot(free).save(base + QStringLiteral("ext_free.png"));
    InternalLinkDialog interior({}, threePieces(), true);
    interior.selectType(InternalLinkType::Generic);
    interior.markPiece(0, 1);
    interior.setPartial(1, true);
    shot(interior).save(base + QStringLiteral("int_generic.png"));
    InternalLinkDialog partial({}, threePieces(), false);
    partial.selectType(InternalLinkType::PartialFixity);
    shot(partial).save(base + QStringLiteral("int_partial.png"));
    // 3D symbols on a white sheet: one per type.
    QImage sheet(1100, 400, QImage::Format_ARGB32);
    sheet.fill(Qt::white);
    QPainter p(&sheet);
    p.setRenderHint(QPainter::Antialiasing);
    int i = 0;
    for (ExternalSupportType t : allExternalSupportTypes()) {
        ExternalSupport s;
        s.type = t;
        const QPointF at(60 + 105 * (i % 10), 70 + 190 * (i / 10));
        p.setPen(Qt::black);
        p.drawText(at + QPointF(-50, -45), QString::number(i));
        cc::app::drawExternalSupportSymbol(p, s, at, kAxes, QColor(0, 102, 204));
        p.drawEllipse(at, 2, 2);
        sheet.setPixel(0, 0, 0xFFFFFFFF);
        ++i;
    }
    p.end();
    sheet.save(base + QStringLiteral("symbols.png"));
    // The 3D window with supports (screen-size symbols, nothing for the fixed internal link).
    {
        Project project;
        for (int k = 0; k < 6; ++k) project.structure.addNode({3.0 * k, 0, 0});
        for (int k = 0; k < 6; ++k) project.structure.addNode({3.0 * k, 0, 3});
        for (int k = 1; k <= 6; ++k) project.structure.addBar(k, k + 6);
        UndoStack undo;
        const ExternalSupportType types[] = {ExternalSupportType::Fixed, ExternalSupportType::Pinned, ExternalSupportType::LineXFixed,
                                             ExternalSupportType::PlaneXYPinned, ExternalSupportType::LineZPinned, ExternalSupportType::Free};
        for (int k = 0; k < 5; ++k) {
            ExternalSupport support;
            support.type = types[k];
            assignExternalSupport(project, undo, {k + 1}, support);
        }
        InternalLink pinnedLink, partialLink;
        pinnedLink.type = InternalLinkType::Pinned;
        partialLink.type = InternalLinkType::PartialFixity;
        assignInternalLink(project, undo, {7, 8}, pinnedLink);
        assignInternalLink(project, undo, {9}, partialLink);
        Viewport3D view;
        view.resize(900, 500);
        view.setStandardView(cc::core::StandardView::Front);
        view.setOrthographic(true);
        view.setStructure(&project.structure);
        view.setNodeSupports(&project.nodeSupports);
        view.frameAll();
        view.show();
        QApplication::processEvents();
        view.grabFramebuffer().save(base + QStringLiteral("viewport.png"));
    }
    QImage buttons(10 * 46, 2 * 46, QImage::Format_ARGB32);
    buttons.fill(QColor(0xF0, 0xF0, 0xF0));
    QPainter q(&buttons);
    for (int v = 0; v < kExternalSupportTypeCount; ++v) {
        const int order[2][10] = {{2, 3, 4, 5, 6, 7, 8, 9, 10, 11}, {0, 1, 12, 13, 14, 15, 16, 17, 18, 19}};
        for (int r = 0; r < 2; ++r)
            for (int c = 0; c < 10; ++c)
                if (order[r][c] == v) q.drawPixmap(c * 46 + 4, r * 46 + 4, cc::app::externalSupportPictogram(static_cast<ExternalSupportType>(v), 38));
    }
    q.end();
    buttons.save(base + QStringLiteral("pictograms.png"));
}

int main(int argc, char** argv) {
    if (!qEnvironmentVariableIsSet("CC_REAL_GUI")) qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
