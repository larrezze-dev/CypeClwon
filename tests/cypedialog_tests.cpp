// Frame of the application dialogs, measured on the "Nueva obra" window of the original (docs/research/ui/nudos.md section 8):
// 5 px on the left, right and bottom (1 px #841E14 outside + 4 px #B1291C inside), a #B1291C title bar about 31 px high with 1 px
// #841E14 on top, interior #F0F0F0.
#include <gtest/gtest.h>

#include <QApplication>
#include <QImage>

#include "CypeDialog.h"

namespace {
QApplication* app() {
    static int argc = 1;
    static char name[] = "cypedialog_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
    return &instance;
}
constexpr QRgb kDark = 0xFF841E14, kRed = 0xFFB1291C, kGrey = 0xFFF0F0F0;
}  // namespace

TEST(CypeDialogFrame, FiveFramePixelsLeftRightAndBottomAndATitleBar) {
    app();
    cc::app::CypeDialog dialog(QStringLiteral("Prueba"));
    dialog.setFixedSize(300, 200);
    const QImage image = dialog.grab().toImage();
    ASSERT_EQ(image.size(), QSize(300, 200));
    const int w = 300, h = 200, y = 120;  // a row below the title bar, clear of any control
    EXPECT_EQ(image.pixel(0, y), kDark);
    for (int x = 1; x <= 4; ++x) EXPECT_EQ(image.pixel(x, y), kRed) << "left x=" << x;
    EXPECT_EQ(image.pixel(5, y), kGrey);
    EXPECT_EQ(image.pixel(w - 1, y), kDark);
    for (int x = w - 5; x <= w - 2; ++x) EXPECT_EQ(image.pixel(x, y), kRed) << "right x=" << x;
    EXPECT_EQ(image.pixel(w - 6, y), kGrey);
    const int x = 100;
    EXPECT_EQ(image.pixel(x, h - 1), kDark);
    for (int row = h - 5; row <= h - 2; ++row) EXPECT_EQ(image.pixel(x, row), kRed) << "bottom y=" << row;
    EXPECT_EQ(image.pixel(x, h - 6), kGrey);
    // Title bar: 1 px dark on top, then red down to row 30 (31 px with the outline), grey from row 31.
    EXPECT_EQ(image.pixel(20, 0), kDark);
    for (int row = 1; row <= 30; ++row) EXPECT_EQ(image.pixel(20, row), kRed) << "title y=" << row;
    EXPECT_EQ(image.pixel(20, 31), kGrey);
}

TEST(CypeDialogFrame, TheTitleIsWhiteAndCentred) {
    app();
    cc::app::CypeDialog dialog(QStringLiteral("Calculadora"));
    dialog.setFixedSize(322, 286);
    const QImage image = dialog.grab().toImage();
    int whites = 0, minX = 1000, maxX = -1;
    for (int y = 1; y < 31; ++y)
        for (int x = 40; x < 280; ++x)
            if (qRed(image.pixel(x, y)) > 200 && qGreen(image.pixel(x, y)) > 200 && qBlue(image.pixel(x, y)) > 200) {
                ++whites;
                minX = std::min(minX, x);
                maxX = std::max(maxX, x);
            }
    EXPECT_GT(whites, 50);
    EXPECT_NEAR((minX + maxX) / 2.0, 161.0, 6.0) << "centred in the 322 px window";
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
