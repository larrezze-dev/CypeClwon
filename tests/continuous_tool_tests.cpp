// Geometria > Barras > Edición de pilares / vigas (T-P5-08, GUI part): the ContinuousTool menus and commands and the small
// dialogs. Never shown on screen (offscreen).
#include <gtest/gtest.h>

#include <QAction>
#include <QApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QSignalSpy>
#include <memory>

#include "ContinuousDialogs.h"
#include "ContinuousTool.h"
#include "Viewport3D.h"
#include "core/bar_edit.h"
#include "core/piece_edit.h"

using cc::app::ContinuousTool;
using cc::app::DeflectionLimitDialog;
using cc::app::ReferenceDialog;
using cc::app::SlabDepthDialog;
using cc::app::Viewport3D;
using namespace cc::core;
using Mode = ContinuousTool::Mode;

namespace {
QApplication* app() {
    static int argc = 1;
    static char name[] = "continuous_tool_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
    return &instance;
}

void mouse(Viewport3D& v, QEvent::Type type, QPointF pos, Qt::MouseButton button, Qt::MouseButtons buttons) {
    QMouseEvent e(type, pos, v.mapToGlobal(pos), button, buttons, Qt::NoModifier);
    QApplication::sendEvent(&v, &e);
}
void click(Viewport3D& v, QPointF pos, Qt::MouseButton button = Qt::LeftButton) {
    mouse(v, QEvent::MouseMove, pos, Qt::NoButton, Qt::NoButton);
    mouse(v, QEvent::MouseButtonPress, pos, button, button);
    mouse(v, QEvent::MouseButtonRelease, pos, button, Qt::NoButton);
}

/// A pillar (two pieces: bars b0+b1 and b2) at x = y = 0 (nodes at z = 0, 3, 6, 9) and a row of three beams at z = 3
/// from x = 4 (nodes at x = 4, 8, 12, 16) joined to the pillar; every beam bar is its own beam piece.
class ContinuousToolTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();
        tool = std::make_unique<ContinuousTool>();
        tool->setProject(&project, &undo);
        undo.setChangeCallback([this] { tool->structureChanged(); });
        QObject::connect(tool.get(), &ContinuousTool::messageRequested, [this](const QString& text) { message = text; });
        QObject::connect(tool.get(), &ContinuousTool::statusChanged, [this](const QString& text) { status = text; });
    }
    void buildPillars() {
        for (int i = 0; i < 4; ++i) col[i] = project.structure.addNode({0, 0, 3.0 * i});
        for (int i = 0; i < 3; ++i) colBar[i] = project.structure.addBar(col[i], col[i + 1]);
        ASSERT_EQ(createPiece(project, undo, col[0], col[2], &pieceA), PieceError::None);
        ASSERT_EQ(setPieceType(project, undo, {colBar[0]}, ElementType::Pillar), PieceError::None);
        ASSERT_EQ(setPieceType(project, undo, {colBar[2]}, ElementType::Pillar), PieceError::None);
        undo.clear();
    }
    void buildBeams() {
        const int steel = project.structure.addMaterial(Material{0, "S275", MaterialKind::Steel});
        for (int i = 0; i < 4; ++i) row[i] = project.structure.addNode({4.0 + 4.0 * i, 0, 3});
        for (int i = 0; i < 3; ++i) {
            beamBar[i] = project.structure.addBar(row[i], row[i + 1], steel);
            ASSERT_EQ(setPieceType(project, undo, {beamBar[i]}, ElementType::Beam), PieceError::None);
        }
        undo.clear();
    }
    Project project;
    UndoStack undo;
    std::unique_ptr<ContinuousTool> tool;
    QString message, status;
    int col[4] = {}, colBar[3] = {}, row[4] = {}, beamBar[3] = {}, pieceA = 0;
};
}  // namespace

TEST_F(ContinuousToolTest, MenusFollowTheOriginalOrderAndSeparators) {
    const auto texts = [](const QMenu* menu) {
        QStringList out;
        for (const QAction* action : menu->actions()) out << (action->isSeparator() ? QStringLiteral("-") : action->text());
        return out;
    };
    EXPECT_EQ(texts(tool->beamsMenu()),
              (QStringList{"Crear viga continua", "Editar nudos", "Editar forjados adyacentes", "-", "Editar referencias de vigas",
                           "Editar referencias de vigas continuas", "-", "Flecha límite de vigas", "Editar ambiente de vigas"}));
    EXPECT_EQ(texts(tool->pillarsMenu()), (QStringList{"Crear pilar continuo", "Editar canto del forjado", "Editar referencia", "Numerar"}));
    EXPECT_EQ(tool->beamsMenu()->title(), QStringLiteral("Edición de vigas"));
    EXPECT_EQ(tool->pillarsMenu()->title(), QStringLiteral("Edición de pilares"));
    EXPECT_TRUE(tool->createBeamAction()->toolTip().contains(QStringLiteral("pulse el botón derecho del ratón")));
    EXPECT_TRUE(tool->deflectionLimitAction()->toolTip().startsWith(QStringLiteral("Permite editar la flecha límite de las vigas.")));
}

TEST_F(ContinuousToolTest, ButtonsAreGreyWithoutPillarsOrBeams) {
    EXPECT_FALSE(tool->pillarsMenu()->menuAction()->isEnabled());
    EXPECT_FALSE(tool->beamsMenu()->menuAction()->isEnabled());
    buildPillars();
    tool->structureChanged();
    EXPECT_TRUE(tool->pillarsMenu()->menuAction()->isEnabled());
    EXPECT_FALSE(tool->beamsMenu()->menuAction()->isEnabled());
    buildBeams();
    tool->structureChanged();
    EXPECT_TRUE(tool->beamsMenu()->menuAction()->isEnabled());
    tool->setProject(nullptr, nullptr);
    EXPECT_FALSE(tool->pillarsMenu()->menuAction()->isEnabled());
}

TEST_F(ContinuousToolTest, EntriesWithoutCoreOnlySayTheyAreNotAvailable) {
    buildBeams();
    QSignalSpy spy(tool.get(), &ContinuousTool::notAvailable);
    for (QAction* action : {tool->editNodesAction(), tool->adjacentSlabsAction(), tool->beamEnvironmentAction()}) {
        EXPECT_TRUE(action->isEnabled());
        action->trigger();
    }
    EXPECT_EQ(spy.count(), 3);
    EXPECT_EQ(tool->mode(), Mode::None);
    EXPECT_EQ(undo.count(), 0u);
}

TEST_F(ContinuousToolTest, CreateColumnWithTwoNodeClicksAndUndo) {
    buildPillars();
    tool->structureChanged();
    Viewport3D view;
    view.resize(800, 600);
    view.setStandardView(StandardView::Front);
    view.setOrthographic(true);
    view.camera().target = {0, 0, 4.5};
    view.setStructure(&project.structure);
    tool->addViewport(&view);
    tool->createColumnAction()->trigger();
    EXPECT_EQ(tool->mode(), Mode::CreateColumn);
    EXPECT_TRUE(status.startsWith(QStringLiteral("Seleccione el nudo inicial de la primera pieza")));
    QPointF a, b;
    ASSERT_TRUE(view.worldToScreen({0, 0, 0}, a));
    ASSERT_TRUE(view.worldToScreen({0, 0, 9}, b));
    click(view, a);
    EXPECT_EQ(tool->chain().size(), 1u);
    click(view, b);
    EXPECT_TRUE(tool->chain().empty());
    ASSERT_EQ(project.continuous.columns.size(), 1u);
    EXPECT_EQ(project.continuous.columns.front().pieceIds.size(), 2u);
    undo.undo();
    EXPECT_TRUE(project.continuous.columns.empty());
    // Escape / another command stops it.
    tool->deactivate();
    EXPECT_EQ(tool->mode(), Mode::None);
}

TEST_F(ContinuousToolTest, CreateBeamReportsTheOriginalMessageWhenRefused) {
    buildBeams();
    EXPECT_FALSE(tool->createBeam({beamBar[0]}));  // a continuous beam needs two or more pieces
    EXPECT_FALSE(message.isEmpty());
    message.clear();
    ASSERT_TRUE(tool->createBeam({beamBar[0], beamBar[1], beamBar[2]}));
    EXPECT_EQ(project.continuous.beams.size(), 1u);
    undo.undo();
    EXPECT_TRUE(project.continuous.beams.empty());
    undo.redo();
    EXPECT_EQ(project.continuous.beams.size(), 1u);
}

TEST_F(ContinuousToolTest, EditReferencesAsOneUndoStepAndRefusesEmpty) {
    buildBeams();
    int dialogs = 0;
    tool->setDialogRunner([&](QDialog& dialog) {
        auto* ref = dynamic_cast<ReferenceDialog*>(&dialog);
        EXPECT_NE(ref, nullptr);
        ref->setReference(QStringLiteral("VIGA-%1").arg(++dialogs));
        return static_cast<int>(QDialog::Accepted);
    });
    ASSERT_TRUE(tool->editBeamReferences({beamBar[0], beamBar[1]}, false));
    EXPECT_EQ(dialogs, 2);
    EXPECT_EQ(project.continuous.pieceReferences.size(), 2u);
    EXPECT_EQ(undo.count(), 1u) << "one macro";
    undo.undo();
    EXPECT_TRUE(project.continuous.pieceReferences.empty());
    // A bar that is not a lone beam is refused before any dialog opens... continuous scope: no continuous beam exists.
    dialogs = 0;
    EXPECT_FALSE(tool->editBeamReferences({beamBar[0]}, true));
    EXPECT_EQ(dialogs, 0);
    // Cancel: nothing changes.
    const std::size_t before = undo.count();
    tool->setDialogRunner([](QDialog&) { return static_cast<int>(QDialog::Rejected); });
    EXPECT_FALSE(tool->editBeamReferences({beamBar[0]}, false));
    EXPECT_EQ(undo.count(), before);
    EXPECT_TRUE(project.continuous.pieceReferences.empty());
}

TEST_F(ContinuousToolTest, ReferenceOfAContinuousBeamAndOfAPillar) {
    buildBeams();
    buildPillars();
    ASSERT_TRUE(tool->createBeam({beamBar[0], beamBar[1], beamBar[2]}));
    ASSERT_EQ(createContinuousColumn(project, undo, col[0], col[3]), ContinuousError::None);
    tool->setDialogRunner([](QDialog& dialog) {
        static_cast<ReferenceDialog&>(dialog).setReference(QStringLiteral("VC1"));
        return static_cast<int>(QDialog::Accepted);
    });
    ASSERT_TRUE(tool->editBeamReferences({beamBar[1]}, true));
    EXPECT_EQ(project.continuous.beams.front().reference, "VC1");
    tool->setDialogRunner([](QDialog& dialog) {
        static_cast<ReferenceDialog&>(dialog).setReference(QStringLiteral("P-A"));
        return static_cast<int>(QDialog::Accepted);
    });
    ASSERT_TRUE(tool->editColumnReferences({colBar[0]}));
    EXPECT_EQ(project.continuous.columns.front().reference, "P-A");
}

TEST_F(ContinuousToolTest, NumberPillarsUsesTheInitialReferenceDialog) {
    buildPillars();
    QString title;
    tool->setDialogRunner([&](QDialog& dialog) {
        title = dialog.windowTitle();
        static_cast<ReferenceDialog&>(dialog).setReference(QStringLiteral("P10"));
        return static_cast<int>(QDialog::Accepted);
    });
    ASSERT_TRUE(tool->numberColumns());
    EXPECT_EQ(project.continuous.pieceReferences.size(), 2u);
    EXPECT_EQ(project.continuous.pieceReferences.front().text, "P10");
    // The dialog title comes from #64972.
    EXPECT_TRUE(title.contains(QStringLiteral("Referencia inicial")) || title.isEmpty());
    // A reference without a number is refused with the core message.
    tool->setDialogRunner([](QDialog& dialog) {
        static_cast<ReferenceDialog&>(dialog).setReference(QStringLiteral("PILAR"));
        return static_cast<int>(QDialog::Accepted);
    });
    EXPECT_FALSE(tool->numberColumns());
    EXPECT_FALSE(message.isEmpty());
}

TEST_F(ContinuousToolTest, SlabDepthAndDeflectionLimitUndo) {
    buildPillars();
    buildBeams();
    tool->setDialogRunner([](QDialog& dialog) {
        static_cast<SlabDepthDialog&>(dialog).setDepthCm(35.0);
        return static_cast<int>(QDialog::Accepted);
    });
    ASSERT_TRUE(tool->editSlabDepth({col[1], col[2]}));
    EXPECT_EQ(project.continuous.slabDepths.size(), 2u);
    EXPECT_NEAR(*slabDepthAt(project, col[1]), 0.35, 1e-9);
    EXPECT_EQ(undo.count(), 1u);
    // A node that belongs to no pillar refuses the whole batch.
    EXPECT_FALSE(tool->editSlabDepth({col[0], row[3]}));
    EXPECT_EQ(project.continuous.slabDepths.size(), 2u);
    EXPECT_FALSE(message.isEmpty());
    undo.undo();
    EXPECT_TRUE(project.continuous.slabDepths.empty());

    tool->setDialogRunner([](QDialog& dialog) {
        static_cast<DeflectionLimitDialog&>(dialog).setValues(300.0, 20.0);
        return static_cast<int>(QDialog::Accepted);
    });
    ASSERT_TRUE(tool->editDeflectionLimit({beamBar[0], beamBar[1]}));
    ASSERT_TRUE(deflectionLimitOfBar(project, beamBar[0]).has_value());
    EXPECT_NEAR(deflectionLimitOfBar(project, beamBar[1])->relative, 300.0, 1e-9);
    EXPECT_NEAR(deflectionLimitOfBar(project, beamBar[1])->absoluteMm, 20.0, 1e-9);
    undo.undo();
    EXPECT_FALSE(deflectionLimitOfBar(project, beamBar[0]).has_value());
}

TEST(ContinuousDialogs, ReferenceLimitsAndEmptyRefusal) {
    app();
    ReferenceDialog dialog(QStringLiteral("t"), QStringLiteral("l"), QStringLiteral("P1"));
    dialog.setReference(QString(40, QLatin1Char('x')));
    EXPECT_EQ(dialog.reference().size(), ReferenceDialog::kMaxLength);
    dialog.setReference(QStringLiteral("  "));
    dialog.accept();
    EXPECT_NE(dialog.result(), QDialog::Accepted);
    EXPECT_EQ(dialog.errorText(), QStringLiteral("La referencia no puede quedar vacía."));
    dialog.setReference(QStringLiteral("P2"));
    dialog.accept();
    EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST(ContinuousDialogs, SlabDepthAutomaticAndValue) {
    app();
    SlabDepthDialog automatic(std::nullopt);
    EXPECT_FALSE(automatic.depthM().has_value());
    SlabDepthDialog fixed(0.25);
    ASSERT_TRUE(fixed.depthM().has_value());
    EXPECT_NEAR(*fixed.depthM(), 0.25, 1e-9);
    fixed.setAutomatic(true);
    EXPECT_FALSE(fixed.depthM().has_value());
}

TEST(ContinuousTool, TargetOfBarScopes) {
    Project p;
    UndoStack stack;
    const int a = p.structure.addNode({0, 0, 0}), b = p.structure.addNode({4, 0, 0});
    const int bar = p.structure.addBar(a, b);
    EXPECT_FALSE(ContinuousTool::targetOfBar(p, bar, ElementType::Beam, ContinuousTool::Scope::Any).has_value()) << "implicit generic piece";
    ASSERT_EQ(setPieceType(p, stack, {bar}, ElementType::Beam), PieceError::None);
    const auto lone = ContinuousTool::targetOfBar(p, bar, ElementType::Beam, ContinuousTool::Scope::LoneOnly);
    ASSERT_TRUE(lone.has_value());
    EXPECT_EQ(lone->kind, ReferenceTarget::Kind::Piece);
    EXPECT_FALSE(ContinuousTool::targetOfBar(p, bar, ElementType::Beam, ContinuousTool::Scope::ContinuousOnly).has_value());
    EXPECT_FALSE(ContinuousTool::targetOfBar(p, bar, ElementType::Pillar, ContinuousTool::Scope::Any).has_value());
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
