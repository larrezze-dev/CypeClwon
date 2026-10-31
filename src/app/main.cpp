#include <QApplication>
#include <QDir>
#include <QCommandLineParser>
#include <QLocale>
#include <QMdiSubWindow>
#include <QWidget>
#include <QTranslator>

#include "MainWindow.h"
#include "NewProjectDialog.h"
#include "EmptyProjectWizard.h"
#include "LimitStatesDialog.h"
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

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("CypeClwon"));
    QCoreApplication::setApplicationName(QStringLiteral("CypeClwon"));

    // Developer aids: --locale forces a UI catalogue; --demo-windows opens N placeholder MDI windows.
    QCommandLineParser parser;
    QCommandLineOption localeOpt(QStringLiteral("locale"), QStringLiteral("Force UI locale."), QStringLiteral("name"));
    QCommandLineOption demoOpt(QStringLiteral("demo-windows"), QStringLiteral("Open N demo MDI windows."), QStringLiteral("n"));
    QCommandLineOption previewOpt(QStringLiteral("preview-dialog"), QStringLiteral("Show a dialog on its own (new-project, project-type, standards, empty-project)."), QStringLiteral("name"));
    parser.addOptions({localeOpt, demoOpt, previewOpt});
    parser.process(app);
    installTranslator(app, parser.value(localeOpt));
    if (parser.value(previewOpt) == QLatin1String("new-project")) {
        cc::app::NewProjectDialog dialog(cc::app::NewProjectDialog::defaultFolder());
        dialog.show();
        return app.exec();
    }
    if (parser.value(previewOpt) == QLatin1String("project-type")) {
        cc::app::ProjectTypeDialog dialog;
        dialog.show();
        return app.exec();
    }
    if (parser.value(previewOpt) == QLatin1String("standards")) {
        cc::app::StandardsDialog dialog{cc::core::StandardsSelection{}};
        dialog.show();
        return app.exec();
    }
    if (parser.value(previewOpt) == QLatin1String("limit-states")) {
        cc::app::LimitStatesDialog dialog{cc::core::StandardsSelection{}, cc::core::LimitStatesSettings{}};
        dialog.show();
        return app.exec();
    }
    if (parser.value(previewOpt) == QLatin1String("limit-states-verify")) {
        cc::app::LimitStatesVerifyDialog dialog{cc::core::LimitStatesSettings{}};
        dialog.show();
        return app.exec();
    }
    if (parser.value(previewOpt) == QLatin1String("empty-project")) {
        cc::app::EmptyProjectWizard dialog;
        dialog.show();
        return app.exec();
    }
    cc::app::MainWindow window;
    for (int i = 0; i < parser.value(demoOpt).toInt(); ++i) {
        window.workArea()->addDocumentWindow(QStringLiteral("3D"), new QWidget);
    }
    if (window.workArea()->windowCount() > 0) window.setProjectOpen(true);
    window.show();
    return app.exec();
}

