// Offscreen tests for the left panel and the "Opciones de visualización" section.
#include <gtest/gtest.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QSignalSpy>
#include <QSplitter>
#include <QToolButton>

#include "DisplayOptionsPanel.h"
#include "LeftPanel.h"
#include "Viewport3D.h"

using namespace cc::app;
using cc::core::DisplayOptions;
using cc::core::TooltipPlacement;

namespace {
QApplication* app() {
    static int argc = 1;
    static char name[] = "leftpanel_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
    return &instance;
}

class PanelTest : public ::testing::Test {
protected:
    void SetUp() override { app(); }
};
}  // namespace

Q_DECLARE_METATYPE(cc::core::DisplayOptions)

TEST_F(PanelTest, DefaultsMatchTheOriginal) {
    DisplayOptionsPanel panel;
    EXPECT_EQ(panel.options(), DisplayOptions{});
    EXPECT_TRUE(panel.findChild<QCheckBox*>("showInfo")->isChecked());
    EXPECT_TRUE(panel.findChild<QCheckBox*>("showTexts")->isChecked());
    EXPECT_TRUE(panel.findChild<QCheckBox*>("showAxes")->isChecked());
    EXPECT_FALSE(panel.findChild<QCheckBox*>("fillShells")->isChecked());
    EXPECT_TRUE(panel.findChild<QCheckBox*>("fillLoads")->isChecked());
    EXPECT_DOUBLE_EQ(panel.findChild<QDoubleSpinBox*>("nodeSize")->value(), 15.0);
    EXPECT_DOUBLE_EQ(panel.findChild<QDoubleSpinBox*>("textSize")->value(), 100.0);
    auto* combo = panel.findChild<QComboBox*>("tooltipPosition");
    ASSERT_EQ(combo->count(), 2);
    EXPECT_EQ(combo->itemText(0), "En la esquina superior derecha");
    EXPECT_EQ(combo->itemText(1), "Sobre el elemento");
}

TEST_F(PanelTest, EveryControlEmitsAChange) {
    qRegisterMetaType<DisplayOptions>();
    DisplayOptionsPanel panel;
    QSignalSpy spy(&panel, &DisplayOptionsPanel::optionsChanged);
    int expected = 0;
    for (const char* name : {"showInfo", "showTexts", "showAxes", "fillShells", "fillLoads"}) {
        auto* box = panel.findChild<QCheckBox*>(name);
        box->setChecked(!box->isChecked());
        EXPECT_EQ(spy.count(), ++expected) << name;
    }
    panel.findChild<QDoubleSpinBox*>("nodeSize")->setValue(20.0);
    EXPECT_EQ(spy.count(), ++expected);
    panel.findChild<QDoubleSpinBox*>("textSize")->setValue(150.0);
    EXPECT_EQ(spy.count(), ++expected);
    panel.findChild<QComboBox*>("tooltipPosition")->setCurrentIndex(1);
    EXPECT_EQ(spy.count(), ++expected);

    const DisplayOptions o = panel.options();
    EXPECT_FALSE(o.showElementInfo);
    EXPECT_FALSE(o.showTexts);
    EXPECT_FALSE(o.showLocalAxes);
    EXPECT_TRUE(o.fillShells);
    EXPECT_FALSE(o.fillLoads);
    EXPECT_DOUBLE_EQ(o.nodeSizeCm, 20.0);
    EXPECT_DOUBLE_EQ(o.textSizePercent, 150.0);
    EXPECT_EQ(o.tooltipPlacement, TooltipPlacement::OnElement);
    EXPECT_EQ(spy.last().at(0).value<DisplayOptions>(), o);
}

TEST_F(PanelTest, SetOptionsRoundTripsWithoutSignalAndIgnoresInvalid) {
    DisplayOptionsPanel panel;
    QSignalSpy spy(&panel, &DisplayOptionsPanel::optionsChanged);
    DisplayOptions o;
    o.fillShells = true;
    o.nodeSizeCm = 31.5;
    o.tooltipPlacement = TooltipPlacement::OnElement;
    panel.setOptions(o);
    EXPECT_EQ(panel.options(), o);
    EXPECT_EQ(spy.count(), 0);
    EXPECT_TRUE(panel.findChild<QCheckBox*>("fillShells")->isChecked());
    EXPECT_EQ(panel.findChild<QComboBox*>("tooltipPosition")->currentIndex(), 1);
    DisplayOptions bad;
    bad.nodeSizeCm = -3;
    panel.setOptions(bad);
    EXPECT_EQ(panel.options(), o);
}

TEST_F(PanelTest, ApplyToViewportHonoursWhatItCan) {
    Viewport3D viewport;
    EXPECT_FALSE(viewport.fillShells());  // original default: outline only
    DisplayOptions o;
    o.showElementInfo = false;
    o.fillShells = true;
    o.tooltipPlacement = TooltipPlacement::OnElement;
    DisplayOptionsPanel::applyTo(viewport, o);
    EXPECT_FALSE(viewport.showElementInfo());
    EXPECT_TRUE(viewport.fillShells());
    EXPECT_EQ(viewport.tooltipPosition(), TooltipPosition::OnElement);
    DisplayOptionsPanel::applyTo(viewport, DisplayOptions{});
    EXPECT_TRUE(viewport.showElementInfo());
    EXPECT_FALSE(viewport.fillShells());
    EXPECT_EQ(viewport.tooltipPosition(), TooltipPosition::TopRightCorner);
}

TEST_F(PanelTest, LeftPanelHasThreeSectionsWithFoldAndClose) {
    LeftPanel panel;
    panel.resize(LeftPanel::kWidth, 800);
    panel.show();
    EXPECT_EQ(panel.width(), 382);
    EXPECT_EQ(panel.splitter()->count(), 3);
    PanelSection* display = panel.displaySection();
    EXPECT_TRUE(display->content()->isVisible());
    display->chevronButton()->click();
    EXPECT_TRUE(display->isFolded());
    EXPECT_FALSE(display->content()->isVisible());
    display->chevronButton()->click();
    EXPECT_TRUE(display->content()->isVisible());
    panel.labelsSection()->closeButton()->click();
    EXPECT_FALSE(panel.labelsSection()->isVisible());
    EXPECT_TRUE(panel.categoriesSection()->isVisible());
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
