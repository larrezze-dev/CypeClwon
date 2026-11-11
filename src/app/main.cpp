#include <QApplication>
#include <QDir>
#include <QCommandLineParser>
#include <QLocale>
#include <QMdiSubWindow>
#include <QWidget>
#include <QTranslator>

#include "MainWindow.h"
#include "ViewBar.h"
#include "Viewport3D.h"
#include "NewProjectDialog.h"
#include <QListWidget>
#include <QRadioButton>
#include <QTimer>

#include "EmptyProjectWizard.h"
#include "FileManagerDialog.h"
#include "LimitStatesDialog.h"
#include "UserSituationsDialogs.h"
#include "ProjectTypeDialog.h"
#include "StandardsDialog.h"
#include "WorkArea.h"

namespace {

/// Loads translations/cypeclwon_<locale>.qm next to the executable. Spanish (es) is the source
/// language, so no catalogue is needed for it.
void installTranslator(QApplication& app, const QString& override) {
    const QString locale = override.isEmpty() ? QLocale::system().name() : override;  // e.g. "en_GB"
    if (locale.startsWith(QLatin1String("es"))) return;
    auto* translator = new QTranslator(&app);
    const QString dir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("translations"));
    if (translator->load(QStringLiteral("cypeclwon_") + locale, dir)) {
        app.installTranslator(translator);
    }
}

}  // namespace

namespace {
/// Shows a preview dialog; with CC_PREVIEW_GRAB=<file.png> it saves a picture of the window and quits (comparison with the original).
int runPreview(QApplication& app, QWidget& dialog) {
    dialog.show();
    const QByteArray grab = qgetenv("CC_PREVIEW_GRAB");
    if (!grab.isEmpty())
        QTimer::singleShot(400, &app, [&app, &dialog, grab] {
            dialog.grab().save(QString::fromLocal8Bit(grab));
            app.quit();
        });
    return app.exec();
}
}  // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("CypeClwon"));
    QCoreApplication::setApplicationName(QStringLiteral("CypeClwon"));

    // Developer aids: --locale forces a UI catalogue; --demo-windows opens N placeholder MDI windows.
    QCommandLineParser parser;
    QCommandLineOption localeOpt(QStringLiteral("locale"), QStringLiteral("Force UI locale."), QStringLiteral("name"));
    QCommandLineOption demoOpt(QStringLiteral("demo-windows"), QStringLiteral("Open N demo MDI windows."), QStringLiteral("n"));
    QCommandLineOption previewOpt(QStringLiteral("preview-dialog"), QStringLiteral("Show a dialog on its own (new-project, project-type, standards, empty-project)."), QStringLiteral("name"));
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("Project (.ed3) to open."), QStringLiteral("[file]"));
    parser.addOptions({localeOpt, demoOpt, previewOpt});
    parser.process(app);
    installTranslator(app, parser.value(localeOpt));
    if (parser.value(previewOpt) == QLatin1String("new-project")) {
        cc::app::NewProjectDialog dialog(cc::app::NewProjectDialog::defaultFolder());
        return runPreview(app, dialog);
    }
    if (parser.value(previewOpt) == QLatin1String("project-type")) {
        cc::app::ProjectTypeDialog dialog;
        return runPreview(app, dialog);
    }
    if (parser.value(previewOpt) == QLatin1String("standards")) {
        cc::app::StandardsDialog dialog{cc::core::StandardsSelection{}};
        return runPreview(app, dialog);
    }
    if (parser.value(previewOpt) == QLatin1String("file-manager")) {
        cc::app::FileManagerDialog dialog{cc::app::NewProjectDialog::defaultFolder()};
        return runPreview(app, dialog);
    }
    if (parser.value(previewOpt) == QLatin1String("limit-states")) {
        cc::app::LimitStatesDialog dialog{cc::core::StandardsSelection{}, cc::core::LimitStatesSettings{}};
        return runPreview(app, dialog);
    }
    // Previews of the variants of "Estados límite" and of the user-situation dialogs (comparison with docs/screenshots/cmb_*.png).
    const QString preview = parser.value(previewOpt);
    if (preview == QLatin1String("limit-states-cte") || preview == QLatin1String("limit-states-ce") || preview == QLatin1String("limit-states-ec")) {
        cc::core::StandardsSelection s = preview == QLatin1String("limit-states-cte") ? cc::core::StandardsSelection::cteSet()
                                         : preview == QLatin1String("limit-states-ce") ? cc::core::StandardsSelection::structuralCodeSet()
                                                                                       : cc::core::StandardsSelection{};
        if (preview == QLatin1String("limit-states-ec")) {
            s.coldFormedSteel = "Eurocódigos 3 y 4 (UE)";
            s.hotRolledSteel = "Eurocódigos 3 y 4 (UE)";
            s.timber = "Eurocódigo 5 (UE)";
        }
        cc::app::LimitStatesDialog dialog{s, cc::core::LimitStatesSettings{}};
        return runPreview(app, dialog);
    }
    if (preview == QLatin1String("limit-state-kind")) {
        cc::app::LimitStateKindDialog dialog{cc::core::LimitStateKind::Aluminium, cc::core::StandardsSelection{}, cc::core::LimitStatesSettings{}};
        return runPreview(app, dialog);
    }
    if (preview == QLatin1String("situation-edit")) {
        cc::app::SituationEditDialog dialog{cc::core::UserSituation::withDefaults(1), 1, cc::core::CoefficientMode::PartialAndCombination, {}};
        return runPreview(app, dialog);
    }
    if (preview == QLatin1String("situation-set")) {
        cc::app::SituationSetDialog dialog{cc::core::UserSituationSet{}, {}, true};
        return runPreview(app, dialog);
    }
    if (preview == QLatin1String("situation-mode")) {
        cc::app::CoefficientModeDialog dialog;
        return runPreview(app, dialog);
    }
    if (preview == QLatin1String("situation-categories")) {
        cc::app::SituationCategoriesDialog dialog{{}};
        return runPreview(app, dialog);
    }
    if (preview == QLatin1String("situation-edit-seismic")) {
        cc::core::UserSituation s = cc::core::UserSituation::withDefaults(1);
        s.type = cc::core::SituationType::Seismic;
        cc::app::SituationEditDialog dialog{s, 1, cc::core::CoefficientMode::PartialAndCombination, {}};
        return runPreview(app, dialog);
    }
    if (preview == QLatin1String("situation-edit-service")) {
        cc::core::UserSituation s = cc::core::UserSituation::withDefaults(1);
        s.type = cc::core::SituationType::Service;
        cc::app::SituationEditDialog dialog{s, 1, cc::core::CoefficientMode::PartialFactors, {}};
        return runPreview(app, dialog);
    }
    if (preview == QLatin1String("situation-set-master") || preview == QLatin1String("situation-set-master-els")) {
        const bool service = preview == QLatin1String("situation-set-master-els");
        cc::core::UserSituationSet set;
        set.reference = "CA";
        set.service = service;
        set.categories = {"A"};
        for (const char* name : {"XX", "SS"}) {
            cc::core::UserSituation s = cc::core::UserSituation::withDefaults(1);
            s.name = name;
            s.type = service ? cc::core::SituationType::Service
                             : (std::string(name) == "SS" ? cc::core::SituationType::Seismic : cc::core::SituationType::PersistentOrTransient);
            set.situations.push_back(s);
        }
        cc::app::SituationSetDialog dialog{set, {}, true};
        if (!service) dialog.list()->setCurrentRow(1);
        return runPreview(app, dialog);
    }
    if (preview == QLatin1String("limit-state-concrete")) {
        cc::app::LimitStateKindDialog dialog{cc::core::LimitStateKind::Concrete, cc::core::StandardsSelection::cteSet(), cc::core::LimitStatesSettings{}};
        dialog.userRadio()->setChecked(true);
        return runPreview(app, dialog);
    }
    if (preview == QLatin1String("use-categories-cte") || preview == QLatin1String("use-categories-ec")) {
        const bool cte = preview == QLatin1String("use-categories-cte");
        const auto scheme = cte ? cc::app::UseCategorySelectionDialog::Scheme::Cte : cc::app::UseCategorySelectionDialog::Scheme::Eurocode;
        std::vector<bool> checked(cc::app::UseCategorySelectionDialog::categoryTexts(scheme).size(), false);
        if (cte) checked.back() = true;
        cc::app::UseCategorySelectionDialog dialog{scheme, checked};
        return runPreview(app, dialog);
    }
    if (parser.value(previewOpt) == QLatin1String("limit-states-verify")) {
        cc::app::LimitStatesVerifyDialog dialog{cc::core::LimitStatesSettings{}};
        return runPreview(app, dialog);
    }
    if (parser.value(previewOpt) == QLatin1String("empty-project")) {
        cc::app::EmptyProjectWizard dialog;
        return runPreview(app, dialog);
    }
    cc::app::MainWindow window;
    const int demoWindows = parser.value(demoOpt).toInt();
    if (demoWindows > 0) window.setProjectOpen(true);  // opens the first 3D window
    for (int i = 1; i < demoWindows; ++i) {
        window.workArea()->addDocumentWindow(QStringLiteral("3D"), new cc::app::ViewportFrame);
    }
    window.show();
    if (!parser.positionalArguments().isEmpty()) window.openProjectFile(parser.positionalArguments().first());
    return app.exec();
}

