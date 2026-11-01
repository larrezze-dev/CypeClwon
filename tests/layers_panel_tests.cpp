// Offscreen tests for the "Etiquetas" (layers) panel and its dialogs.
#include <gtest/gtest.h>

#include <memory>

#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QSignalSpy>
#include <QTableWidget>
#include <QToolButton>

#include "LayersPanel.h"

using namespace cc::app;
using namespace cc::core;

namespace {
QApplication* app() {
    static int argc = 1;
    static char name[] = "layers_panel_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
    return &instance;
}

/// Scripted answers instead of blocking dialogs.
struct FakePrompter : LayersPrompter {
    int warnings = 0;
    int confirmations = 0;
    bool confirmAnswer = true;
    std::optional<std::pair<LayerRemoval, int>> removalAnswer;
    std::optional<unsigned> colorAnswer;
    std::optional<std::pair<LayerOperation, std::vector<int>>> assignAnswer;
    std::vector<Layer> removalOffered;

    void warnLastLayer(QWidget*) override { ++warnings; }
    bool confirmRemoval(QWidget*, const QString&) override {
        ++confirmations;
        return confirmAnswer;
    }
    std::optional<std::pair<LayerRemoval, int>> chooseRemoval(QWidget*, const QString&,
                                                              const std::vector<Layer>& others) override {
        removalOffered = others;
        return removalAnswer;
    }
    std::optional<unsigned> chooseColor(QWidget*, unsigned) override { return colorAnswer; }
    std::optional<std::pair<LayerOperation, std::vector<int>>> chooseAssignment(QWidget*,
                                                                                const std::vector<Layer>&) override {
        return assignAnswer;
    }
};

class LayersTest : public ::testing::Test {
protected:
    void SetUp() override {
        app();  // the QApplication must exist before any widget
        panelOwner = std::make_unique<LayersPanel>();
        panel = panelOwner.get();
        panel->setStructure(&model);
        panel->setPrompter(&prompter);
    }
    void TearDown() override { panelOwner.reset(); }
    QString text(int row, int col) const { return panel->table()->item(row, col)->text(); }
    StructuralModel model;
    FakePrompter prompter;
    std::unique_ptr<LayersPanel> panelOwner;
    LayersPanel* panel = nullptr;
};
}  // namespace

TEST_F(LayersTest, ShowsTheDefaultLayerAndTooltips) {
    ASSERT_EQ(panel->table()->rowCount(), 1);
    EXPECT_EQ(text(0, LayersPanel::ColReference), "por defecto");
    EXPECT_EQ(panel->table()->item(0, LayersPanel::ColActive)->checkState(), Qt::Checked);
    EXPECT_EQ(panel->addButton()->toolTip(), "Añadir / Añadir nuevo elemento a la lista");
    EXPECT_EQ(panel->removeButton()->toolTip(), "Borrar / Suprimir elemento seleccionado de la lista");
    EXPECT_EQ(panel->assignToolButton()->toolTip(), "Asignar elementos a etiquetas");
    EXPECT_TRUE(panel->assignToolButton()->isCheckable());
}

TEST_F(LayersTest, AddCreatesAnInactiveSelectedLayer) {
    QSignalSpy spy(panel, &LayersPanel::layersChanged);
    panel->addButton()->click();
    ASSERT_EQ(model.layers.size(), 2u);
    EXPECT_EQ(model.layers[1].name, "Nueva capa 2");
    EXPECT_FALSE(model.layers[1].active);
    EXPECT_EQ(panel->selectedLayerId(), model.layers[1].id);
    EXPECT_EQ(spy.count(), 1);
    panel->addButton()->click();
    EXPECT_EQ(model.layers[2].name, "Nueva capa 3");
    EXPECT_EQ(panel->selectedLayerId(), model.layers[2].id);
}

TEST_F(LayersTest, ActiveCheckIsExclusive) {
    panel->addLayer();
    QSignalSpy spy(panel, &LayersPanel::layersChanged);
    panel->table()->item(1, LayersPanel::ColActive)->setCheckState(Qt::Checked);
    EXPECT_EQ(model.activeLayerId(), model.layers[1].id);
    EXPECT_EQ(panel->table()->item(0, LayersPanel::ColActive)->checkState(), Qt::Unchecked);
    EXPECT_EQ(spy.count(), 1);
    // Unchecking the only active layer is undone: there is always one.
    panel->table()->item(1, LayersPanel::ColActive)->setCheckState(Qt::Unchecked);
    EXPECT_EQ(panel->table()->item(1, LayersPanel::ColActive)->checkState(), Qt::Checked);
    EXPECT_EQ(model.activeLayerId(), model.layers[1].id);
}

TEST_F(LayersTest, MoveUpAndDownKeepTheSelection) {
    panel->addLayer();
    panel->addLayer();
    const int id = model.layers[2].id;
    panel->upButton()->click();
    EXPECT_EQ(model.layers[1].id, id);
    EXPECT_EQ(panel->selectedLayerId(), id);
    EXPECT_EQ(text(1, 0), "Nueva capa 3");
    panel->downButton()->click();
    EXPECT_EQ(model.layers[2].id, id);
    EXPECT_EQ(panel->selectedLayerId(), id);
    QSignalSpy spy(panel, &LayersPanel::layersChanged);
    panel->downButton()->click();  // already last
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(LayersTest, RenameThroughTheCell) {
    panel->addLayer();
    QSignalSpy spy(panel, &LayersPanel::layersChanged);
    panel->table()->item(1, 0)->setText("Pilares");
    EXPECT_EQ(model.layers[1].name, "Pilares");
    EXPECT_EQ(spy.count(), 1);
    panel->table()->item(1, 0)->setText("");  // empty names are refused and restored
    EXPECT_EQ(model.layers[1].name, "Pilares");
    EXPECT_EQ(text(1, 0), "Pilares");
    panel->table()->item(1, 0)->setText("POR DEFECTO");  // already used
    EXPECT_EQ(text(1, 0), "Pilares");
    EXPECT_EQ(spy.count(), 1);
}

TEST_F(LayersTest, EyeLockCubeAndColourUpdateTheModel) {
    QSignalSpy spy(panel, &LayersPanel::layersChanged);
    panel->toggleCell(0, LayersPanel::ColVisible);
    EXPECT_FALSE(model.layers[0].visible);
    panel->toggleCell(0, LayersPanel::ColLocked);
    EXPECT_TRUE(model.layers[0].locked);
    panel->toggleCell(0, LayersPanel::ColRendered);
    EXPECT_TRUE(model.layers[0].rendered);
    prompter.colorAnswer = 0xFF0000u;
    emit panel->table()->cellClicked(0, LayersPanel::ColColor);
    EXPECT_EQ(model.layers[0].color, 0xFF0000u);
    EXPECT_EQ(spy.count(), 4);
    prompter.colorAnswer.reset();  // cancelled
    panel->toggleCell(0, LayersPanel::ColColor);
    EXPECT_EQ(model.layers[0].color, 0xFF0000u);
    EXPECT_EQ(spy.count(), 4);
    panel->toggleCell(0, LayersPanel::ColVisible);
    EXPECT_TRUE(model.layers[0].visible);
}

TEST_F(LayersTest, DeletingTheLastLayerWarns) {
    panel->selectLayer(model.layers[0].id);
    QSignalSpy spy(panel, &LayersPanel::layersChanged);
    panel->removeButton()->click();
    EXPECT_EQ(prompter.warnings, 1);
    EXPECT_EQ(model.layers.size(), 1u);
    EXPECT_EQ(spy.count(), 0);
}

TEST_F(LayersTest, DeletingAnEmptyLayerAsksYesNo) {
    panel->addLayer();
    prompter.confirmAnswer = false;
    panel->removeButton()->click();
    EXPECT_EQ(model.layers.size(), 2u);
    prompter.confirmAnswer = true;
    panel->removeButton()->click();
    EXPECT_EQ(prompter.confirmations, 2);
    EXPECT_EQ(model.layers.size(), 1u);
    EXPECT_EQ(panel->table()->rowCount(), 1);
}

TEST_F(LayersTest, DeletingALayerWithElementsMovesOrDeletes) {
    const int n1 = model.addNode({0, 0, 0}), n2 = model.addNode({1, 0, 0}), n3 = model.addNode({2, 0, 0});
    const int b1 = model.addBar(n1, n2), b2 = model.addBar(n2, n3);
    const int extra = model.addLayer("Extra");
    ASSERT_TRUE(model.assignLayers({b1, b2}, {}, {extra}, LayerOperation::Replace));
    panel->setStructure(&model);
    panel->selectLayer(extra);

    prompter.removalAnswer.reset();  // cancel: nothing happens
    panel->removeButton()->click();
    EXPECT_EQ(model.layers.size(), 2u);
    ASSERT_EQ(prompter.removalOffered.size(), 1u);
    EXPECT_EQ(prompter.removalOffered[0].id, kDefaultLayerId);

    prompter.removalAnswer = std::make_pair(LayerRemoval::MoveElements, kDefaultLayerId);
    panel->removeButton()->click();
    EXPECT_EQ(model.layers.size(), 1u);
    ASSERT_NE(model.findBar(b1), nullptr);
    EXPECT_EQ(model.findBar(b1)->layerIds, std::vector<int>{kDefaultLayerId});

    const int again = model.addLayer("Otra");
    ASSERT_TRUE(model.assignLayers({b1}, {}, {again}, LayerOperation::Replace));
    panel->setStructure(&model);
    panel->selectLayer(again);
    prompter.removalAnswer = std::make_pair(LayerRemoval::DeleteElements, 0);
    panel->removeButton()->click();
    EXPECT_EQ(model.findBar(b1), nullptr);
    EXPECT_NE(model.findBar(b2), nullptr);
}

TEST_F(LayersTest, AssignToAppliesTheChosenOperation) {
    const int n1 = model.addNode({0, 0, 0}), n2 = model.addNode({1, 0, 0});
    const int bar = model.addBar(n1, n2);
    const int other = model.addLayer("Otra");
    prompter.assignAnswer = std::make_pair(LayerOperation::Assign, std::vector<int>{other});
    QSignalSpy spy(panel, &LayersPanel::layersChanged);
    EXPECT_TRUE(panel->assignTo({bar}, {}));
    EXPECT_EQ(model.findBar(bar)->layerIds, (std::vector<int>{kDefaultLayerId, other}));
    EXPECT_EQ(spy.count(), 1);
    prompter.assignAnswer.reset();
    EXPECT_FALSE(panel->assignTo({bar}, {}));
}

TEST_F(LayersTest, AssignToolButtonEmitsToggled) {
    QSignalSpy spy(panel, &LayersPanel::assignToolToggled);
    panel->assignToolButton()->click();
    ASSERT_EQ(spy.count(), 1);
    EXPECT_TRUE(spy.last().at(0).toBool());
}

TEST_F(LayersTest, WorksWithoutAModel) {
    panel->setStructure(nullptr);
    EXPECT_EQ(panel->table()->rowCount(), 0);
    panel->addButton()->click();
    panel->removeButton()->click();
    EXPECT_FALSE(panel->assignTo({1}, {}));
}

TEST_F(LayersTest, AssignDialogOperationsAndOkState) {
    Layer a{1, "por defecto"}, b{2, "Nueva capa 2"};
    AssignLayersDialog dialog({a, b});
    EXPECT_EQ(dialog.windowTitle(), "Asignar elementos a etiquetas");
    EXPECT_EQ(dialog.operation(), LayerOperation::Replace);  // original default
    ASSERT_EQ(dialog.table()->rowCount(), 2);
    EXPECT_EQ(dialog.table()->item(1, 1)->text(), "Nueva capa 2");
    QPushButton* ok = dialog.buttons()->button(QDialogButtonBox::Ok);
    EXPECT_EQ(ok->text(), "Aceptar");
    EXPECT_EQ(dialog.buttons()->button(QDialogButtonBox::Cancel)->text(), "Cancelar");
    EXPECT_FALSE(ok->isEnabled());  // Replace with nothing ticked
    dialog.setLayerChecked(2, true);
    EXPECT_TRUE(ok->isEnabled());
    EXPECT_EQ(dialog.selectedLayerIds(), std::vector<int>{2});
    dialog.setOperation(LayerOperation::Assign);
    EXPECT_EQ(dialog.operation(), LayerOperation::Assign);
    dialog.setLayerChecked(2, false);
    EXPECT_FALSE(ok->isEnabled());
    dialog.setOperation(LayerOperation::Remove);
    EXPECT_EQ(dialog.operation(), LayerOperation::Remove);
    EXPECT_TRUE(ok->isEnabled());  // removal needs no layer ticked
    dialog.setLayerChecked(1, true);
    dialog.setLayerChecked(2, true);
    EXPECT_EQ(dialog.selectedLayerIds(), (std::vector<int>{1, 2}));
}

TEST_F(LayersTest, DeleteDialogAccessors) {
    DeleteLayerDialog dialog("X", {Layer{1, "a"}, Layer{4, "b"}});
    EXPECT_EQ(dialog.windowTitle(), "Borrar capa");
    EXPECT_EQ(dialog.removal(), LayerRemoval::MoveElements);
    EXPECT_EQ(dialog.destinationLayerId(), 1);
    dialog.setDestinationLayerId(4);
    EXPECT_EQ(dialog.destinationLayerId(), 4);
    dialog.setRemoval(LayerRemoval::DeleteElements);
    EXPECT_EQ(dialog.removal(), LayerRemoval::DeleteElements);
    EXPECT_FALSE(dialog.destinationCombo()->isEnabled());
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
