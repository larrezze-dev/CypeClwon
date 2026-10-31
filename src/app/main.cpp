#include <QApplication>
#include <QDir>
#include <QLocale>
#include <QTranslator>

#include "MainWindow.h"

namespace {

/// Loads translations/cypeclwon_<locale>.qm next to the executable. Spanish (es) is the source
/// language, so no catalogue is needed for it.
void installTranslator(QApplication& app) {
    const QString locale = QLocale::system().name();  // e.g. "en_GB"
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
    installTranslator(app);
    cc::app::MainWindow window;
    window.show();
    return app.exec();
}

