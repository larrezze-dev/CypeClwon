// GUI tests for the start screen, run on Qt's offscreen platform (nothing is displayed).
#include <gtest/gtest.h>

#include <QApplication>
#include <QPushButton>
#include <QSignalSpy>

#include "StartPage.h"

using cc::app::StartPage;

namespace {
void ensureApp() {
    static int argc = 1;
    static char name[] = "startpage_tests";
    static char* argv[] = {name, nullptr};
    static QApplication instance(argc, argv);
}

QPushButton* buttonWithText(StartPage& page, const QString& startsWith) {
    for (QPushButton* b : page.findChildren<QPushButton*>()) {
        if (b->text().startsWith(startsWith)) return b;
    }
    return nullptr;
}
}  // namespace

TEST(StartPage, EmptySlotsAreDisabledAndLabelled) {
    ensureApp();
    StartPage page;
    for (int i = 1; i <= StartPage::kRecentSlots; ++i) {
        QPushButton* b = buttonWithText(page, QString::number(i) + " Sin fichero");
        ASSERT_NE(b, nullptr) << i;
        EXPECT_FALSE(b->isEnabled());
    }
}

TEST(StartPage, MissingFilesAreGreyedAndMoreFollowsHistory) {
    ensureApp();
    StartPage page;
    page.setRecentFiles({"D:/obras/a.ed3", "D:/obras/b.ed3"}, {"D:/obras/b.ed3"}, false);
    int enabledRecents = 0;
    QPushButton* more = nullptr;
    for (QPushButton* b : page.findChildren<QPushButton*>()) {
        if (b->text() == "Más...") more = b;
        else if (b->text().startsWith("1 D:") && b->isEnabled()) ++enabledRecents;
        else if (b->text().startsWith("2 D:")) EXPECT_FALSE(b->isEnabled());
    }
    EXPECT_EQ(enabledRecents, 1);
    ASSERT_NE(more, nullptr);
    EXPECT_FALSE(more->isEnabled());
    page.setRecentFiles({"D:/obras/a.ed3"}, {}, true);
    EXPECT_TRUE(more->isEnabled());
}

TEST(StartPage, RecentFileEmitsItsPath) {
    ensureApp();
    StartPage page;
    page.setRecentFiles({"D:/obras/a.ed3", "D:/obras/b.ed3"});
    QSignalSpy spy(&page, &StartPage::recentFileRequested);
    QPushButton* second = buttonWithText(page, "2 ");
    ASSERT_NE(second, nullptr);
    EXPECT_TRUE(second->isEnabled());
    second->click();
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).toString(), QString("D:/obras/b.ed3"));
    EXPECT_NE(buttonWithText(page, "3 Sin fichero"), nullptr);
}

TEST(StartPage, ArchiveButtonEmitsSignalAndThereIsNoLicenceButton) {
    ensureApp();
    StartPage page;
    QSignalSpy newSpy(&page, &StartPage::newRequested);
    buttonWithText(page, "Nuevo")->click();
    EXPECT_EQ(newSpy.count(), 1);
    EXPECT_EQ(buttonWithText(page, "Gestión de licencias"), nullptr);
}

TEST(StartPage, HelpEntryEmitsItsId) {
    ensureApp();
    StartPage page;
    QSignalSpy spy(&page, &StartPage::helpRequested);
    buttonWithText(page, "Novedades")->click();
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).value<StartPage::HelpEntry>(), StartPage::HelpEntry::News);
}

int main(int argc, char** argv) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
