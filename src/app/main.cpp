#include <QApplication>
#include <QDir>
#include <QCommandLineParser>
#include <QLocale>
#include <QMdiSubWindow>
#include <QWidget>
#include <QTranslator>

#include "MainWindow.h"
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

    // Developer aids: --locale forces a UI catalogue; --demo-windows opens N placeholder MDI windows.
    QCommandLineParser parser;
    QCommandLineOption localeOpt(QStringLiteral("locale"), QStringLiteral("Force UI locale."), QStringLiteral("name"));
    QCommandLineOption demoOpt(QStringLiteral("demo-windows"), QStringLiteral("Open N demo MDI windows."), QStringLiteral("n"));
    parser.addOptions({localeOpt, demoOpt});
    parser.process(app);
    installTranslator(app, parser.value(localeOpt));
    cc::app::MainWindow window;
    for (int i = 0; i < parser.value(demoOpt).toInt(); ++i) {
        window.workArea()->addDocumentWindow(QStringLiteral("3D"), new QWidget);
    }
    window.show();
    return app.exec();
}

